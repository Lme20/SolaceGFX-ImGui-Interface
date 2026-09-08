#include "ui/effects/glass_cursor.h"
#include "graphics/gfx_helpers.h"
#include "graphics/snapshot.h"
#include "ui/foundation/motion/motion.h"
#include "ui/foundation/theme.h"

#include <cstring>
#include <math.h>
#include <vector>

namespace solace::glass
{
namespace
{
constexpr int k_max_points = 64;

struct constants
{
    float res[4];
    float shape[4];
    float glass[4];
    float glass2[4];
    float warp[4];
    float bg[4];
    float points[k_max_points][4];
};

struct render_command
{
    constants values{};
    bgfx::TextureHandle backdrop = BGFX_INVALID_HANDLE;
};

gfx::pixel_shader_pass g_shader;

struct trail
{
    std::vector<ImVec2> points;
    ImVec2 smoothed{0.f, 0.f};
    float time = 0.f;
    float clock = 0.f;
    float carry = 0.f;
    float presence = 0.f;
    bool seeded = false;
    int frame = -1;
};

trail& state()
{
    static trail t;
    return t;
}

void callback(const ImDrawList*, const ImDrawCmd* cmd)
{
    if (!cmd || !cmd->UserCallbackData ||
        cmd->UserCallbackDataSize != static_cast<int>(sizeof(render_command)))
        return;

    render_command command{};
    std::memcpy(&command, cmd->UserCallbackData, sizeof(command));
    if (!g_shader.upload_constants(&command.values, sizeof(command.values)))
        return;

    g_shader.bind(&command.backdrop, 1);
}

void set_colour(float out[4], ImU32 col)
{
    const ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
    out[0] = c.x;
    out[1] = c.y;
    out[2] = c.z;
    out[3] = c.w;
}
} // namespace

cursor_options& settings()
{
    static cursor_options o;
    return o;
}

bool& enabled()
{
    static bool on = true;
    return on;
}

bool cursor_live()
{
    return g_shader.ready() && state().seeded && state().presence > 0.05 &&
           snapshot::backdrop_ready();
}

bool cursor_init()
{
    return g_shader.initialize("fs_glass_cursor", "glass cursor shader", sizeof(constants),
                               /*sampler_count=*/1);
}

void cursor_shutdown()
{
    g_shader.reset();
    state() = trail();
}

void cursor(ImDrawList* dl, const ImRect& viewport, const cursor_options& o)
{
    if (!g_shader.ready() || !dl || !enabled() || o.opacity <= 0.001f)
        return;

    ImGuiIO& io = ImGui::GetIO();
    trail& s = state();

    const int frame = ImGui::GetFrameCount();
    if (s.frame != frame)
    {
        s.frame = frame;

        const float dt = ImClamp(io.DeltaTime, 0.f, 0.1f);
        s.time += dt * ImMax(o.warp_speed, 0.f);
        s.clock += dt;

        const int want = ImClamp(o.trail_length, 2, k_max_points);

        const ImVec2 pointer = o.follow_pointer ? io.MousePos : o.pointer;
        const bool present = (pointer.x > -FLT_MAX * 0.5f) && viewport.Contains(pointer);

        const float toward = present ? 1.f : 0.f;
        const float rate = present ? 6.f : 4.5f;
        s.presence += (toward - s.presence) * ImMin(1.f, dt * rate);
        if (!present && s.presence < 0.01f)
        {
            s.presence = 0.f;
            s.seeded = false;
            s.points.clear();
            return;
        }

        if (present)
        {

            if (s.seeded)
            {
                const ImVec2 d(pointer.x - s.smoothed.x, pointer.y - s.smoothed.y);
                if (d.x * d.x + d.y * d.y > 0.25f * (viewport.GetWidth() * viewport.GetWidth() +
                                                     viewport.GetHeight() * viewport.GetHeight()))
                    s.seeded = false;
            }

            if (!s.seeded)
            {
                s.smoothed = pointer;
                s.seeded = true;
                s.carry = 0.f;
                s.points.assign((size_t)want, s.smoothed);
            }

            const float damp = ImClamp(o.dampening, 0.f, 0.999f);
            const float a = (damp <= 0.f) ? 1.f : 1.f - powf(damp, dt * 60.f);
            s.smoothed.x += (pointer.x - s.smoothed.x) * a;
            s.smoothed.y += (pointer.y - s.smoothed.y) * a;
        }

        if (!s.seeded)
            return;

        const float step = ImMax(o.trail_seconds, 0.008f) / (float)ImMax(want - 1, 1);

        s.carry += dt;
        int steps = (int)(s.carry / step);
        if (steps > 0)
        {
            s.carry -= (float)steps * step;
            steps = ImMin(steps, want);

            const ImVec2 from = s.points.empty() ? s.smoothed : s.points.front();
            for (int i = 1; i <= steps; i++)
            {
                const float t = (float)i / (float)steps;
                s.points.insert(s.points.begin(), ImVec2(from.x + (s.smoothed.x - from.x) * t,
                                                         from.y + (s.smoothed.y - from.y) * t));
            }
        }

        if ((int)s.points.size() > want)
            s.points.resize((size_t)want);
        while ((int)s.points.size() < want)
            s.points.push_back(s.points.back());
    }

    if (!s.seeded || s.points.size() < 2 || s.presence <= 0.004f)
        return;

    const float blob_px = ImMax(solace::px(o.blob_radius), 1.f);

    const float warp_px = ImMax(o.warp_amount, 0.f) * 0.15f;
    const int count = (int)s.points.size();

    ImRect box(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (int i = 0; i < count; i++)
    {
        box.Min.x = ImMin(box.Min.x, s.points[i].x);
        box.Min.y = ImMin(box.Min.y, s.points[i].y);
        box.Max.x = ImMax(box.Max.x, s.points[i].x);
        box.Max.y = ImMax(box.Max.y, s.points[i].y);
    }

    const float pad = blob_px * 1.6f + warp_px + 8.f;
    box.Expand(pad);
    box.ClipWith(viewport);
    if (box.GetWidth() < 1.f || box.GetHeight() < 1.f)
        return;

    snapshot::capture_backdrop(dl);
    if (!snapshot::backdrop_ready())
        return;

    render_command command{};
    constants& c = command.values;

    c.res[0] = viewport.GetWidth();
    c.res[1] = viewport.GetHeight();
    c.res[2] = 1.f / ImMax(c.res[0], 1.f);
    c.res[3] = 1.f / ImMax(c.res[1], 1.f);

    c.shape[0] = blob_px;
    c.shape[1] = (ImClamp(o.threshold, 0.f, 1.f) - 0.5f) * 2.f * blob_px;
    c.shape[2] = ImClamp(o.blur_spread * 2.f, 0.f, 1.f);
    c.shape[3] = (float)count;

    c.glass[0] = o.refraction;
    c.glass[1] = ImMax(o.blur_spread, 0.f) * 24.f;
    c.glass[2] = o.border_glow;
    c.glass[3] = o.specular_gain;

    c.glass2[0] = o.chromatic;
    c.glass2[1] = o.tint;
    c.glass2[2] = o.saturation;
    c.glass2[3] = o.brightness;

    c.warp[0] = warp_px;
    c.warp[1] = ImMax(o.warp_scale, 0.f) / ImMax(blob_px * 9.f, 1.f);
    c.warp[2] = s.time;
    c.warp[3] = ImClamp(o.opacity, 0.f, 1.f) * ImClamp(s.presence, 0.f, 1.f);

    set_colour(c.bg, o.background);

    const float fade = ImClamp(o.tail_fade, 0.f, 1.f);
    const float last = (float)(count - 1);
    const float flow = ImClamp(o.flow_amount, 0.f, 0.9f);

    for (int i = 0; i < count; i++)
    {
        const float u = (float)i / ImMax(last, 1.f);

        float r = blob_px * (1.f - fade * u);
        r *= 1.f + flow * sinf(u * 7.4f - s.clock * o.flow_speed);

        c.points[i][0] = s.points[i].x;
        c.points[i][1] = s.points[i].y;
        c.points[i][2] = ImMax(r, 1.f);
        c.points[i][3] = 0.f;
    }

    command.backdrop = snapshot::backdrop_handle();

    dl->AddCallback(callback, &command, sizeof(command));
    dl->AddRectFilled(box.Min, box.Max, IM_COL32_WHITE);
    dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}
} // namespace solace::glass
