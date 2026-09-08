#include "platform/bgfx_renderer.h"
#include "graphics/embedded_shaders.h"

#include <bx/math.h>

namespace solace::platform
{
namespace
{
struct quad_vertex
{
    float x, y;
    float u, v;
    std::uint32_t abgr;
};

bgfx::RendererType::Enum preferred_renderer()
{
#if defined(_WIN32)
    return bgfx::RendererType::Direct3D11;
#elif defined(__APPLE__)
    return bgfx::RendererType::Metal;
#else
    return bgfx::RendererType::Vulkan;
#endif
}
} // namespace

bgfx_renderer::~bgfx_renderer()
{
    reset();
}

bool bgfx_renderer::initialize(void* native_window, void* native_display, std::uint32_t width,
                               std::uint32_t height)
{
    reset();
    if (!native_window || width == 0 || height == 0)
        return false;

    bgfx::Init init;
    init.type = preferred_renderer();
    init.swapChain.nwh = native_window;
    init.swapChain.ndt = native_display;
    init.swapChain.width = width;
    init.swapChain.height = height;
    init.swapChain.formatColor = bgfx::TextureFormat::BGRA8; // alpha needed for transparent window
    init.reset = BGFX_RESET_VSYNC | BGFX_RESET_TRANSPARENT_BACKBUFFER;
    if (!bgfx::init(init))
        return false;
    initialized_ = true;

    width_ = width;
    height_ = height;

    const bgfx::RendererType::Enum type = bgfx::getRendererType();
    composite_ = bgfx::createProgram(
        bgfx::createEmbeddedShader(solace::gfx::k_embedded_shaders, type, "vs_fullscreen"),
        bgfx::createEmbeddedShader(solace::gfx::k_embedded_shaders, type, "fs_fullscreen"), true);
    s_tex_ = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

    quad_layout_.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    return create_scene_target();
}

bool bgfx_renderer::resize(std::uint32_t width, std::uint32_t height)
{
    if (!initialized_ || width == 0 || height == 0)
        return false;
    width_ = width;
    height_ = height;
    bgfx::SwapChain sc;
    sc.width = width;
    sc.height = height;
    sc.formatColor = bgfx::TextureFormat::BGRA8;
    bgfx::reset(BGFX_RESET_VSYNC, &sc);
    destroy_scene_target();
    return create_scene_target();
}

bool bgfx_renderer::create_scene_target()
{
    const std::uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    scene_texture_ =
        bgfx::createTexture2D(static_cast<uint16_t>(width_), static_cast<uint16_t>(height_), false,
                              1, bgfx::TextureFormat::RGBA8, flags);
    if (!bgfx::isValid(scene_texture_))
        return false;
    scene_fb_ = bgfx::createFrameBuffer(1, &scene_texture_, false);
    return bgfx::isValid(scene_fb_);
}

void bgfx_renderer::destroy_scene_target()
{
    if (bgfx::isValid(scene_fb_))
    {
        bgfx::destroy(scene_fb_);
        scene_fb_ = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(scene_texture_))
    {
        bgfx::destroy(scene_texture_);
        scene_texture_ = BGFX_INVALID_HANDLE;
    }
}

void bgfx_renderer::begin_frame()
{
    // All UI views share the RT; only the first clears. ImGui needs strict submit order.
    for (bgfx::ViewId v = k_view_ui_first; v <= k_view_ui_last; ++v)
    {
        bgfx::setViewFrameBuffer(v, scene_fb_);
        bgfx::setViewRect(v, 0, 0, static_cast<uint16_t>(width_), static_cast<uint16_t>(height_));
        bgfx::setViewMode(v, bgfx::ViewMode::Sequential);
        bgfx::setViewClear(v, v == k_view_ui_first ? BGFX_CLEAR_COLOR : BGFX_CLEAR_NONE,
                           0x00000000u);
    }
    bgfx::touch(k_view_ui_first);

    bgfx::setViewFrameBuffer(k_view_composite, BGFX_INVALID_HANDLE); // backbuffer
    bgfx::setViewRect(k_view_composite, 0, 0, static_cast<uint16_t>(width_),
                      static_cast<uint16_t>(height_));
    bgfx::setViewClear(k_view_composite, BGFX_CLEAR_COLOR, 0x00000000u);
    bgfx::setViewMode(k_view_composite, bgfx::ViewMode::Sequential);
}

void bgfx_renderer::composite_and_present()
{
    bgfx::TransientVertexBuffer tvb;
    if (bgfx::getAvailTransientVertexBuffer(3, quad_layout_) >= 3)
    {
        bgfx::allocTransientVertexBuffer(&tvb, 3, quad_layout_);

        // Full-screen triangle in NDC. Flip V for bottom-left-origin renderers (GL)
        const bool flip = bgfx::getCaps()->originBottomLeft;
        auto* v = reinterpret_cast<quad_vertex*>(tvb.data);
        v[0] = {-1.f, -1.f, 0.f, flip ? 0.f : 1.f, 0xffffffffu};
        v[1] = {3.f, -1.f, 2.f, flip ? 0.f : 1.f, 0xffffffffu};
        v[2] = {-1.f, 3.f, 0.f, flip ? 2.f : -1.f, 0xffffffffu};

        bgfx::setVertexBuffer(0, &tvb);
        bgfx::setTexture(0, s_tex_, scene_texture_);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A); // no blend: copy RT alpha
        bgfx::submit(k_view_composite, composite_);
    }
    bgfx::frame();
}

void bgfx_renderer::reset() noexcept
{
    if (!initialized_)
        return;
    destroy_scene_target();
    if (bgfx::isValid(composite_))
        bgfx::destroy(composite_);
    if (bgfx::isValid(s_tex_))
        bgfx::destroy(s_tex_);
    composite_ = BGFX_INVALID_HANDLE;
    s_tex_ = BGFX_INVALID_HANDLE;
    bgfx::shutdown();
    initialized_ = false;
}
} // namespace solace::platform
