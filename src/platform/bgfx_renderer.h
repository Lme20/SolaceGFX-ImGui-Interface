#pragma once
#include <bgfx/bgfx.h>
#include <cstdint>

namespace solace::platform
{
// View layout (ascending id order):
//   k_view_ui_first .. k_view_ui_last : ImGui draw, split on every backdrop capture
//   k_view_composite                  : blit snapshot (start of view) + RT -> backbuffer quad
constexpr bgfx::ViewId k_view_ui_first = 0;
constexpr bgfx::ViewId k_view_ui_last = 200;
constexpr bgfx::ViewId k_view_composite = 201;

class bgfx_renderer final
{
  public:
    bgfx_renderer() = default;
    ~bgfx_renderer();
    bgfx_renderer(const bgfx_renderer&) = delete;
    bgfx_renderer& operator=(const bgfx_renderer&) = delete;

    [[nodiscard]] bool initialize(void* native_window, void* native_display, std::uint32_t width,
                                  std::uint32_t height);
    [[nodiscard]] bool resize(std::uint32_t width, std::uint32_t height);

    void begin_frame();           // clears RT, sets view rects/transforms
    void composite_and_present(); // draws RT to backbuffer, bgfx::frame()

    [[nodiscard]] bgfx::TextureHandle scene_texture() const noexcept
    {
        return scene_texture_;
    }
    [[nodiscard]] std::uint32_t width() const noexcept
    {
        return width_;
    }
    [[nodiscard]] std::uint32_t height() const noexcept
    {
        return height_;
    }

  private:
    bool create_scene_target();
    void destroy_scene_target();
    void reset() noexcept;

    bgfx::FrameBufferHandle scene_fb_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle scene_texture_ = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle composite_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_tex_ = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout quad_layout_;
    std::uint32_t width_ = 0, height_ = 0;
    bool initialized_ = false;
};
} // namespace solace::platform
