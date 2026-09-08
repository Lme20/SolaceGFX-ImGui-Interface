#include "platform/glfw_window.h"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <cmath>

namespace solace::platform
{
glfw_window::~glfw_window()
{
    reset();
}

bool glfw_window::create(const window_config& config)
{
    reset();
    if (!config.title || config.logical_width <= 0 || config.logical_height <= 0)
        return false;

    if (!glfwInit())
        return false;
    glfw_initialized_ = true;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);            // bgfx owns the swap chain
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);              // WS_POPUP equivalent
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE); // WS_EX_LAYERED + DWM equivalent
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE); // Windows/X11: size in DPI-scaled px
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);

    window_ = glfwCreateWindow(config.logical_width, config.logical_height, config.title, nullptr,
                               nullptr);
    if (!window_)
    {
        reset();
        return false;
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, on_framebuffer_size);
    glfwSetWindowContentScaleCallback(window_, on_content_scale);

    // Center on the primary monitor's work area (replaces GetSystemMetrics math).
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    int wx = 0, wy = 0, ww = 0, wh = 0;
    glfwGetMonitorWorkarea(monitor, &wx, &wy, &ww, &wh);
    int win_w = 0, win_h = 0;
    glfwGetWindowSize(window_, &win_w, &win_h);
    glfwSetWindowPos(window_, wx + (ww - win_w) / 2, wy + (wh - win_h) / 2);
    return true;
}

void glfw_window::show() const
{
    if (window_)
        glfwShowWindow(window_);
}

bool glfw_window::pump_events() const
{
    glfwPollEvents();
    return window_ && !glfwWindowShouldClose(window_);
}

std::optional<client_extent> glfw_window::take_pending_resize()
{
    auto r = pending_resize_;
    pending_resize_.reset();
    return r;
}
std::optional<float> glfw_window::take_pending_scale()
{
    auto r = pending_scale_;
    pending_scale_.reset();
    return r;
}

client_extent glfw_window::framebuffer_size() const
{
    int w = 0, h = 0;
    if (window_)
        glfwGetFramebufferSize(window_, &w, &h);
    return {static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h)};
}

float glfw_window::content_scale() const
{
    float sx = 1.f, sy = 1.f;
    if (window_)
        glfwGetWindowContentScale(window_, &sx, &sy);
    return sx > 0.f ? sx : 1.f;
}

// GLFW window sizes are in screen coordinates; on macOS those differ from pixels.
void glfw_window::set_framebuffer_size(client_extent size) const
{
    if (!window_)
        return;
    const client_extent current = framebuffer_size();
    if (current.width == size.width && current.height == size.height)
        return;

    int win_w = 0, win_h = 0;
    glfwGetWindowSize(window_, &win_w, &win_h);
    const float ratio_x = current.width ? static_cast<float>(win_w) / current.width : 1.f;
    const float ratio_y = current.height ? static_cast<float>(win_h) / current.height : 1.f;
    glfwSetWindowSize(window_, static_cast<int>(std::lround(size.width * ratio_x)),
                      static_cast<int>(std::lround(size.height * ratio_y)));
}

bool glfw_window::iconified() const
{
    return window_ && glfwGetWindowAttrib(window_, GLFW_ICONIFIED) == GLFW_TRUE;
}
void glfw_window::request_close() const
{
    if (window_)
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
}
void glfw_window::hide_cursor(bool hidden) const
{
    if (window_)
        glfwSetInputMode(window_, GLFW_CURSOR, hidden ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);
}

void* glfw_window::native_window_handle() const
{
#if defined(_WIN32)
    return glfwGetWin32Window(window_);
#elif defined(__APPLE__)
    return glfwGetCocoaWindow(window_);
#else
    return reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(window_)));
#endif
}
void* glfw_window::native_display_handle() const
{
#if defined(__linux__)
    return glfwGetX11Display();
#else
    return nullptr;
#endif
}

void glfw_window::on_framebuffer_size(GLFWwindow* w, int width, int height)
{
    auto* self = static_cast<glfw_window*>(glfwGetWindowUserPointer(w));
    if (self && width > 0 && height > 0)
        self->pending_resize_ =
            client_extent{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
}
void glfw_window::on_content_scale(GLFWwindow* w, float x, float)
{
    auto* self = static_cast<glfw_window*>(glfwGetWindowUserPointer(w));
    if (self)
        self->pending_scale_ = x;
}

void glfw_window::reset() noexcept
{
    if (window_)
    {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    if (glfw_initialized_)
    {
        glfwTerminate();
        glfw_initialized_ = false;
    }
    pending_resize_.reset();
    pending_scale_.reset();
}
} // namespace solace::platform
