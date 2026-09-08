#pragma once

#define BGFX_PLATFORM_SUPPORTS_WGSL 0

#include <bgfx/embedded_shader.h>

#define SOLACE_SHADER_INCLUDES(name)                                                               \
                                                                                                   \
    SOLACE_INC(glsl, name)                                                                         \
    SOLACE_INC(essl, name)                                                                         \
    SOLACE_INC(spirv, name)

#include "essl/fs_fullscreen.sc.bin.h"
#include "essl/fs_glass_cursor.sc.bin.h"
#include "essl/fs_imgui.sc.bin.h"
#include "essl/fs_morph_slider.sc.bin.h"
#include "essl/fs_morph_slider_settled.sc.bin.h"
#include "essl/fs_rounded_panel.sc.bin.h"
#include "essl/vs_fullscreen.sc.bin.h"
#include "essl/vs_imgui.sc.bin.h"
#include "glsl/fs_fullscreen.sc.bin.h"
#include "glsl/fs_glass_cursor.sc.bin.h"
#include "glsl/fs_imgui.sc.bin.h"
#include "glsl/fs_morph_slider.sc.bin.h"
#include "glsl/fs_morph_slider_settled.sc.bin.h"
#include "glsl/fs_rounded_panel.sc.bin.h"
#include "glsl/vs_fullscreen.sc.bin.h"
#include "glsl/vs_imgui.sc.bin.h"
#include "spirv/fs_fullscreen.sc.bin.h"
#include "spirv/fs_glass_cursor.sc.bin.h"
#include "spirv/fs_imgui.sc.bin.h"
#include "spirv/fs_morph_slider.sc.bin.h"
#include "spirv/fs_morph_slider_settled.sc.bin.h"
#include "spirv/fs_rounded_panel.sc.bin.h"
#include "spirv/vs_fullscreen.sc.bin.h"
#include "spirv/vs_imgui.sc.bin.h"

#if defined(_WIN32)
#include "dx11/fs_fullscreen.sc.bin.h"
#include "dx11/fs_glass_cursor.sc.bin.h"
#include "dx11/fs_imgui.sc.bin.h"
#include "dx11/fs_morph_slider.sc.bin.h"
#include "dx11/fs_morph_slider_settled.sc.bin.h"
#include "dx11/fs_rounded_panel.sc.bin.h"
#include "dx11/vs_fullscreen.sc.bin.h"
#include "dx11/vs_imgui.sc.bin.h"
#elif defined(__APPLE__)
#include "metal/fs_fullscreen.sc.bin.h"
#include "metal/fs_glass_cursor.sc.bin.h"
#include "metal/fs_imgui.sc.bin.h"
#include "metal/fs_morph_slider.sc.bin.h"
#include "metal/fs_morph_slider_settled.sc.bin.h"
#include "metal/fs_rounded_panel.sc.bin.h"
#include "metal/vs_fullscreen.sc.bin.h"
#include "metal/vs_imgui.sc.bin.h"
#endif

namespace solace::gfx
{
inline const bgfx::EmbeddedShader k_embedded_shaders[] = {
    BGFX_EMBEDDED_SHADER(vs_imgui),
    BGFX_EMBEDDED_SHADER(fs_imgui),
    BGFX_EMBEDDED_SHADER(vs_fullscreen),
    BGFX_EMBEDDED_SHADER(fs_fullscreen),
    BGFX_EMBEDDED_SHADER(fs_rounded_panel),
    BGFX_EMBEDDED_SHADER(fs_morph_slider),
    BGFX_EMBEDDED_SHADER(fs_morph_slider_settled),
    BGFX_EMBEDDED_SHADER(fs_glass_cursor),
    BGFX_EMBEDDED_SHADER_END()};
}
