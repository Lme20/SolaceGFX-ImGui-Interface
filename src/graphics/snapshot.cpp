#include "graphics/snapshot.h"
#include "graphics/imgui_impl_bgfx.h"

namespace solace::snapshot
{
namespace
{
struct texture_copy
{
    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    unsigned width = 0, height = 0;
    bool valid = false;

    bool ensure(unsigned w, unsigned h)
    {
        if (bgfx::isValid(handle) && width == w && height == h)
            return true;
        release();
        handle = bgfx::createTexture2D(static_cast<uint16_t>(w), static_cast<uint16_t>(h), false, 1,
                                       bgfx::TextureFormat::RGBA8,
                                       BGFX_TEXTURE_BLIT_DST | BGFX_SAMPLER_U_CLAMP |
                                           BGFX_SAMPLER_V_CLAMP);
        width = w;
        height = h;
        return bgfx::isValid(handle);
    }
    void release()
    {
        if (bgfx::isValid(handle))
            bgfx::destroy(handle);
        handle = BGFX_INVALID_HANDLE;
        valid = false;
    }
};

bgfx::TextureHandle g_scene = BGFX_INVALID_HANDLE;
unsigned g_width = 0, g_height = 0;
bgfx::ViewId g_composite_view = 0;
texture_copy g_backdrop;
texture_copy g_snapshot;
bool g_snapshot_requested = false;

// Runs inside imgui_bgfx::render_draw_data. Everything submitted lives in the
// current view; move on to the next view and blit the RT at the start of it, so the
// copy contains exactly what was drawn before this call.
void backdrop_callback(const ImDrawList*, const ImDrawCmd*)
{
    if (!bgfx::isValid(g_scene) || !g_backdrop.ensure(g_width, g_height))
        return;
    const bgfx::ViewId view = imgui_bgfx::split_view();

    bgfx::TextureRegion dst;
    dst.init(g_backdrop.handle, 0, 0, static_cast<uint16_t>(g_width),
             static_cast<uint16_t>(g_height));

    bgfx::TextureRegion src;
    src.init(g_scene, 0, 0, static_cast<uint16_t>(g_width), static_cast<uint16_t>(g_height));

    bgfx::blit(view, dst, src);

    g_backdrop.valid = true;
}
} // namespace

void attach(bgfx::TextureHandle scene_rt, unsigned width, unsigned height,
            bgfx::ViewId composite_view)
{
    g_scene = scene_rt;
    g_width = width;
    g_height = height;
    g_composite_view = composite_view;
}

void capture_backdrop(ImDrawList* dl)
{
    if (!dl)
        return;
    dl->AddCallback(backdrop_callback, nullptr);
    dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

void invalidate_backdrop()
{
    g_backdrop.release();
}
bool backdrop_ready()
{
    return g_backdrop.valid;
}
ImTextureID backdrop()
{
    return imgui_bgfx::to_texture_id(g_backdrop.handle);
}
bgfx::TextureHandle backdrop_handle()
{
    return g_backdrop.handle;
}

void request()
{
    g_snapshot_requested = true;
}
bool ready()
{
    return g_snapshot.valid;
}
ImTextureID texture()
{
    return imgui_bgfx::to_texture_id(g_snapshot.handle);
}
bgfx::TextureHandle texture_handle()
{
    return g_snapshot.handle;
}

void poll()
{
    if (!g_snapshot_requested || !bgfx::isValid(g_scene))
        return;
    g_snapshot_requested = false;
    if (!g_snapshot.ensure(g_width, g_height))
        return;

    // Blits on the composite view run before its draws
    bgfx::TextureRegion dst;
    dst.init(g_snapshot.handle, 0, 0, static_cast<uint16_t>(g_width),
             static_cast<uint16_t>(g_height));

    bgfx::TextureRegion src;
    src.init(g_scene, 0, 0, static_cast<uint16_t>(g_width), static_cast<uint16_t>(g_height));

    bgfx::blit(g_composite_view, dst, src);

    g_snapshot.valid = true;
}

void release()
{
    g_snapshot.release();
}
void shutdown()
{
    g_snapshot.release();
    g_backdrop.release();
    g_scene = BGFX_INVALID_HANDLE;
}
} // namespace solace::snapshot
