#include "ui/foundation/rounded_panel.h"
#include "graphics/gfx_helpers.h"

#include <algorithm>

namespace solace::rounded_panel
{
namespace
{
struct float4
{
    float x;
    float y;
    float z;
    float w;
};

struct alignas(16) shader_constants
{
    float4 bounds;
    float4 fill_color;
    float4 shape;
    float4 viewport_transform;
};

static_assert(sizeof(float4) == 16);
static_assert(sizeof(shader_constants) == 64);

gfx::pixel_shader_pass g_renderer;

float4 to_float4(ImU32 color)
{
    const ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
    return {value.x, value.y, value.z, value.w};
}

void bind_shader_callback(const ImDrawList*, const ImDrawCmd* command)
{
    if (!command || !command->UserCallbackData ||
        command->UserCallbackDataSize != static_cast<int>(sizeof(shader_constants)))
        return;

    if (g_renderer.upload_constants(command->UserCallbackData, sizeof(shader_constants)))
        g_renderer.bind();
}

void draw_fallback(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max, ImU32 fill,
                   float radius)
{
    draw_list->AddRectFilled(min, max, fill, radius);
}
} // namespace

bool init()
{
    return g_renderer.initialize("fs_rounded_panel", "rounded panel shader",
                                 sizeof(shader_constants), /*sampler_count=*/0);
}

void shutdown()
{
    g_renderer.reset();
}

void draw(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max, ImU32 fill, float radius)
{
    if (!draw_list || max.x <= min.x || max.y <= min.y)
        return;

    radius = (std::max)(radius, 0.f);
    if (!g_renderer.ready())
    {
        draw_fallback(draw_list, min, max, fill, radius);
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float framebuffer_x =
        io.DisplayFramebufferScale.x > 0.f ? io.DisplayFramebufferScale.x : 1.f;
    const float framebuffer_y =
        io.DisplayFramebufferScale.y > 0.f ? io.DisplayFramebufferScale.y : 1.f;
    shader_constants values{};
    values.bounds = {min.x, min.y, max.x, max.y};
    values.fill_color = to_float4(fill);
    values.shape = {radius, 0.f, 0.f, 0.f};
    values.viewport_transform = {viewport ? viewport->Pos.x : 0.f, viewport ? viewport->Pos.y : 0.f,
                                 1.f / framebuffer_x, 1.f / framebuffer_y};

    draw_list->AddCallback(bind_shader_callback, &values, sizeof(values));
    draw_list->AddRectFilled(min, max, IM_COL32_WHITE);
    draw_list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}
} // namespace solace::rounded_panel
