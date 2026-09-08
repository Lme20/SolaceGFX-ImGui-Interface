#pragma once

#include <cstdint>
#include <optional>

struct GLFWwindow;

namespace solace::platform
{
struct client_extent
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct window_config
{
    const char* title = "Desktop application";
    int logical_width = 384;
    int logical_height = 558;
};

class glfw_window final
{
  public:
    glfw_window() = default;
    ~glfw_window();
    glfw_window(const glfw_window&) = delete;
    glfw_window& operator=(const glfw_window&) = delete;

    [[nodiscard]] bool create(const window_config& config);
    void show() const;
    [[nodiscard]] bool pump_events() const; // false when close requested
    [[nodiscard]] std::optional<client_extent> take_pending_resize();
    [[nodiscard]] std::optional<float> take_pending_scale();

    void set_framebuffer_size(client_extent size) const; // pixels
    [[nodiscard]] client_extent framebuffer_size() const;
    [[nodiscard]] float content_scale() const;
    [[nodiscard]] bool iconified() const;
    void request_close() const;
    void hide_cursor(bool hidden) const;
    // void set_cursor_theme(bool theme) const;

    [[nodiscard]] GLFWwindow* handle() const noexcept
    {
        return window_;
    }
    [[nodiscard]] void* native_window_handle() const;  // HWND / NSWindow* / X11 Window
    [[nodiscard]] void* native_display_handle() const; // X11 Display*, else nullptr

  private:
    static void on_framebuffer_size(GLFWwindow*, int w, int h);
    static void on_content_scale(GLFWwindow*, float x, float y);
    void reset() noexcept;

    GLFWwindow* window_ = nullptr;
    bool glfw_initialized_ = false;
    std::optional<client_extent> pending_resize_;
    std::optional<float> pending_scale_;
};
} // namespace solace::platform
