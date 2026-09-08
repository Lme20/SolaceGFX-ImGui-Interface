#include "runtime/desktop_app.h"

#include "application/app.h"
#include "assets/asset_io.h"
#include "assets/avatars.h"
#include "assets/images.h"
#include "core/diagnostics.h"
#include "core/environment.h"
#include "core/product_info.h"
#include "generated/fonts/geist_data.h"
#include "graphics/imgui_impl_bgfx.h"
#include "graphics/snapshot.h"
#include "platform/bgfx_renderer.h"
#include "platform/glfw_window.h"
#include "platform/macos.h"
#include "ui/controls/morph_slider.h"
#include "ui/controls/widgets.h"
#include "ui/effects/glass_cursor.h"
#include "ui/foundation/rounded_panel.h"
#include "ui/foundation/runtime.h"
#include "ui/foundation/theme.h"
#include "ui/foundation/typography/font_cache.h"
#include "ui/screens/search_overlay.h"
#include "ui/screens/shell_menus.h"

#include "imgui_impl_glfw.h"
#include <GLFW/glfw3.h>

#include <chrono>
#include <optional>
#include <string>
#include <thread>

namespace solace::runtime
{
namespace
{
void set_ui_scale(float scale, bool rebuild_fonts)
{
    ui_runtime::set_scale(scale > 0.f ? scale : 1.f, rebuild_fonts);
}

void warm_fonts()
{
    fonts.get(geist_regular, 12);
    fonts.get(geist_regular, 14);
    fonts.get(geist_regular, 16);
    fonts.get(geist_medium, 14);
    fonts.get(geist_medium, 16);
    fonts.get(geist_semibold, 20);
}

bool escape_closes_app()
{
    return !solace::morphing_search_open() && !solace::overlay_open() &&
           !solace::target_menu_open() && !solace::profile_menu_open() &&
           !solace::notifications_open();
}

class imgui_session final
{
  public:
    ~imgui_session()
    {
        shutdown();
    }
    imgui_session() = default;
    imgui_session(const imgui_session&) = delete;
    imgui_session& operator=(const imgui_session&) = delete;

    [[nodiscard]] bool initialize(GLFWwindow* window)
    {
        window_ = window;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        context_created_ = true;

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;
        io.Fonts->TexMinWidth = 1024;
        io.Fonts->TexMinHeight = 1024;

        ImGui::StyleColorsDark();
        ImGui::GetStyle().CircleTessellationMaxError = 0.10f;

        platform_initialized_ = ImGui_ImplGlfw_InitForOther(window, true);
        if (!platform_initialized_)
            return false;

        renderer_initialized_ =
            imgui_bgfx::init(platform::k_view_ui_first, platform::k_view_ui_last);
        return renderer_initialized_;
    }

    void new_frame(platform::client_extent fb) const
    {
        imgui_bgfx::new_frame();
        ImGui_ImplGlfw_NewFrame();

        ImGuiIO& io = ImGui::GetIO();

        const float sx = io.DisplaySize.x > 0.f ? (float)fb.width / io.DisplaySize.x : 1.f;
        const float sy = io.DisplaySize.y > 0.f ? (float)fb.height / io.DisplaySize.y : 1.f;
        if (window_)
        {
            double cx = 0, cy = 0;
            glfwGetCursorPos(window_, &cx, &cy);
            io.AddMousePosEvent((float)cx * sx, (float)cy * sy);
        }

        io.DisplaySize = ImVec2((float)fb.width, (float)fb.height);
        io.DisplayFramebufferScale = ImVec2(1.f, 1.f);

        ImGui::NewFrame();
    }

  private:
    void shutdown() noexcept
    {
        if (renderer_initialized_)
        {
            imgui_bgfx::shutdown();
            renderer_initialized_ = false;
        }
        if (platform_initialized_)
        {
            ImGui_ImplGlfw_Shutdown();
            platform_initialized_ = false;
            window_ = nullptr;
        }
        if (context_created_)
        {
            ui_runtime::clear_animation_states();
            ImGui::DestroyContext();
            context_created_ = false;
        }
    }
    GLFWwindow* window_ = nullptr;
    bool context_created_ = false, platform_initialized_ = false, renderer_initialized_ = false;
};

class ui_services final
{
  public:
    ui_services() = default;
    ~ui_services()
    {
        shutdown();
    }
    ui_services(const ui_services&) = delete;
    ui_services& operator=(const ui_services&) = delete;

    void initialize()
    {
        active_ = true;
        const std::string theme = environment::value("THEME");
        if (!theme.empty())
            solace::set_dark(!(theme == "light" || theme == "Light" || theme == "LIGHT"));

        fonts.install_kerning();
        warm_fonts();

        images::options slides;
        slides.max_edge = 1024;
        slides.aspect = 416.f / 650.f;
        slides.radius_ratio = 0.f;
        slides.saturate = 1.f;
        images::load_folder(asset_io::asset_directory("slides", "SLIDES"), slides);

        const bool slider_initialized = slides::morph_slider_init();
        const bool panel_initialized = solace::rounded_panel::init();
        const bool cursor_initialized = glass::cursor_init();
        avatars::load(asset_io::asset_directory("avatars", "AVATARS"),
                      asset_io::asset_directory("logos", "LOGOS"),
                      asset_io::asset_directory("brands", "BRANDS"));

        if (!slider_initialized)
            diagnostics::warning("ui", "Morph slider shader unavailable; slides disabled.");
        if (!panel_initialized)
            diagnostics::warning("ui",
                                 "Rounded panel shader unavailable; using geometry fallback.");
        if (!cursor_initialized)
            diagnostics::warning("ui", "Glass cursor shader unavailable; custom cursor disabled.");
        else
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    }

    void begin_frame(const platform::bgfx_renderer& renderer) const
    {
        snapshot::attach(renderer.scene_texture(), renderer.width(), renderer.height(),
                         platform::k_view_composite);
        fonts.update();
        images::update();
    }

    void after_render() const
    {
        snapshot::poll();
    }

  private:
    void shutdown() noexcept
    {
        if (!active_)
            return;
        images::shutdown();
        glass::cursor_shutdown();
        solace::rounded_panel::shutdown();
        slides::morph_slider_shutdown();
        avatars::shutdown();
        snapshot::shutdown();
        active_ = false;
    }
    bool active_ = false;
};

void draw_cursor()
{
    /* ImGuiIO& io = ImGui::GetIO();
    if (io.MousePos.x < -FLT_MAX * 0.5f)
        return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImU32 fill = solace::is_dark() ? IM_COL32(255, 255, 255, 255) : IM_COL32(0, 0, 0, 255);
    const ImU32 outline = solace::is_dark() ? IM_COL32(0, 0, 0, 160) : IM_COL32(255, 255, 255, 160);

    const float r = px(5.f);
    dl->AddCircleFilled(io.MousePos, r, fill);
    dl->AddCircle(io.MousePos, r, outline, 0, px(1.f));*/
}

// Replaces GetCursorPos/GetWindowRect/SetWindowPos. GLFW positions are screen coords.
void drag_host_window(GLFWwindow* window)
{
    static bool dragging = false;
    static double start_cx = 0, start_cy = 0;
    static int start_wx = 0, start_wy = 0;

    const bool can_start = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) &&
                           !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive();
    if (can_start && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        dragging = true;
        glfwGetCursorPos(window, &start_cx, &start_cy); // window-relative
        glfwGetWindowPos(window, &start_wx, &start_wy);
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        dragging = false;
    if (!dragging)
        return;

    double cx = 0, cy = 0;
    glfwGetCursorPos(window, &cx, &cy);
    int wx = 0, wy = 0;
    glfwGetWindowPos(window, &wx, &wy);
    // cursor is window-relative, so the delta must be taken against the window's live position
    glfwSetWindowPos(window, wx + static_cast<int>(cx - start_cx),
                     wy + static_cast<int>(cy - start_cy));
}

void sync_host_size(const platform::glfw_window& window)
{
    const auto w = ui_runtime::host_size.x > 1.f
                       ? static_cast<std::uint32_t>(ui_runtime::host_size.x + 0.5f)
                       : 1u;
    const auto h = ui_runtime::host_size.y > 1.f
                       ? static_cast<std::uint32_t>(ui_runtime::host_size.y + 0.5f)
                       : 1u;
    window.set_framebuffer_size({w, h});
}

// void sync_cursor_visibility(const platform::glfw_window& window)
// {
// window.hide_cursor(ImGui::GetIO().WantCaptureMouse);
// }
} // namespace

int run_desktop_app()
{
    platform::window_config config;
    config.title = product_info::window_title;

    platform::glfw_window window;
    if (!window.create(config))
    {
        diagnostics::error("window", "Failed to create the GLFW host window.");
        return diagnostics::to_process_exit_code(diagnostics::exit_code::window_initialization);
    }
    set_ui_scale(window.content_scale(), false);

    const auto dbg_fb = window.framebuffer_size();
    int dbg_w = 0, dbg_h = 0;
    glfwGetWindowSize(window.handle(), &dbg_w, &dbg_h);
    diagnostics::info("dpi",
                      ("window=" + std::to_string(dbg_w) + "x" + std::to_string(dbg_h) +
                       " fb=" + std::to_string(dbg_fb.width) + "x" + std::to_string(dbg_fb.height) +
                       " content_scale=" + std::to_string(window.content_scale()))
                          .c_str());

    const platform::client_extent fb = window.framebuffer_size();
    platform::bgfx_renderer renderer;
    if (!renderer.initialize(window.native_window_handle(), window.native_display_handle(),
                             fb.width, fb.height))
    {
        diagnostics::error("renderer", "Failed to initialize bgfx.");
        return diagnostics::to_process_exit_code(diagnostics::exit_code::renderer_initialization);
    }

#if defined(__APPLE__)
    platform::make_window_transparent(window.native_window_handle());
#endif

    imgui_session imgui;
    if (!imgui.initialize(window.handle()))
    {
        diagnostics::error("imgui", "Failed to initialize an ImGui backend.");
        return diagnostics::to_process_exit_code(diagnostics::exit_code::imgui_initialization);
    }

    ui_services services;
    services.initialize();
    window.show();
    diagnostics::info("runtime", "Solace started.");

    diagnostics::exit_code exit_code = diagnostics::exit_code::success;
    while (window.pump_events())
    {
        if (window.iconified())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (const auto scale = window.take_pending_scale())
            set_ui_scale(*scale, true);

        if (const auto resize = window.take_pending_resize();
            resize && resize->width && resize->height)
        {
            snapshot::invalidate_backdrop();
            if (!renderer.resize(resize->width, resize->height))
            {
                diagnostics::error("renderer", "Failed to resize the bgfx backbuffer.");
                exit_code = diagnostics::exit_code::rendering_failure;
                break;
            }
        }

        renderer.begin_frame();
        services.begin_frame(renderer);
        imgui.new_frame(window.framebuffer_size());

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && escape_closes_app())
            window.request_close();

        solace::application::render_frame();
        ui_runtime::collect_animation_states();
        draw_cursor();
        drag_host_window(window.handle());
        sync_host_size(window);
        // sync_cursor_visibility(window);

        ImGui::Render();
        imgui_bgfx::render_draw_data(ImGui::GetDrawData());
        services.after_render();
        renderer.composite_and_present();
    }

    diagnostics::info("runtime", "Solace stopped.");
    return diagnostics::to_process_exit_code(exit_code);
}
} // namespace solace::runtime
