cmake_minimum_required(VERSION 3.24)
project(Solace LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# ---- dependencies -------------------------------------------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE)

# Pass -DSOLACE_SHADERC=/path/to/shaderc to use a prebuilt one and skip the tools entirely.
set(SOLACE_SHADERC "" CACHE FILEPATH "Prebuilt bgfx shaderc executable (empty = build from source)")

set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
set(BGFX_CONFIG_MULTITHREADED OFF CACHE BOOL "" FORCE)
if(SOLACE_SHADERC)
    set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
else()
    set(BGFX_BUILD_TOOLS ON CACHE BOOL "" FORCE)
    set(BGFX_BUILD_TOOLS_SHADER ON CACHE BOOL "" FORCE)
    set(BGFX_BUILD_TOOLS_GEOMETRY OFF CACHE BOOL "" FORCE)
    set(BGFX_BUILD_TOOLS_TEXTURE OFF CACHE BOOL "" FORCE)
    set(BGFX_BUILD_TOOLS_BIN2C OFF CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(bgfx
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG master)

set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
FetchContent_Declare(freetype
    GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
    GIT_TAG VER-2-14-3
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(glfw bgfx freetype)

if(NOT COMMAND bgfx_compile_shaders)
    include(${bgfx_SOURCE_DIR}/cmake/bgfxToolUtils.cmake)
endif()

# ---- shaders ---------------------------------------------------------------
if(SOLACE_SHADERC)
    if(NOT EXISTS "${SOLACE_SHADERC}")
        message(FATAL_ERROR "SOLACE_SHADERC does not exist: ${SOLACE_SHADERC}")
    endif()
    add_executable(bgfx::shaderc IMPORTED GLOBAL)
    set_target_properties(bgfx::shaderc PROPERTIES IMPORTED_LOCATION "${SOLACE_SHADERC}")
endif()

# Not set by bgfx.cmake in a FetchContent build; required for `#include <bgfx_shader.sh>`.
set(BGFX_SHADER_INCLUDE_PATH ${bgfx_SOURCE_DIR}/bgfx/src)

if(NOT COMMAND bgfx_compile_shaders)
    include(${bgfx_SOURCE_DIR}/cmake/bgfxToolUtils.cmake)
endif()
if(NOT COMMAND bgfx_compile_shaders)
    message(FATAL_ERROR "bgfx::shaderc target missing; set SOLACE_SHADERC or enable BGFX_BUILD_TOOLS")
endif()

set(SHADER_DIR ${CMAKE_SOURCE_DIR}/src/shaders)
set(SHADER_OUT ${CMAKE_BINARY_DIR}/generated/shaders)

bgfx_compile_shaders(
    TYPE VERTEX
    SHADERS ${SHADER_DIR}/vs_imgui.sc
            ${SHADER_DIR}/vs_fullscreen.sc
    VARYING_DEF ${SHADER_DIR}/varying.def.sc
    OUTPUT_DIR ${SHADER_OUT}
    INCLUDE_DIRS ${SHADER_DIR}
    AS_HEADERS
    OUT_FILES_VAR VS_HEADERS)

bgfx_compile_shaders(
    TYPE FRAGMENT
    SHADERS ${SHADER_DIR}/fs_imgui.sc
            ${SHADER_DIR}/fs_fullscreen.sc
            ${SHADER_DIR}/fs_rounded_panel.sc
            ${SHADER_DIR}/fs_morph_slider.sc
            ${SHADER_DIR}/fs_morph_slider_settled.sc
            ${SHADER_DIR}/fs_glass_cursor.sc
    VARYING_DEF ${SHADER_DIR}/varying.def.sc
    OUTPUT_DIR ${SHADER_OUT}
    INCLUDE_DIRS ${SHADER_DIR}
    AS_HEADERS
    OUT_FILES_VAR FS_HEADERS)

# ---- imgui ---------------------------------------------------------------
add_library(imgui STATIC
    thirdparty/imgui/imgui.cpp
    thirdparty/imgui/imgui_draw.cpp
    thirdparty/imgui/imgui_tables.cpp
    thirdparty/imgui/imgui_widgets.cpp
    thirdparty/imgui/backends/imgui_impl_glfw.cpp
    thirdparty/imgui/misc/freetype/imgui_freetype.cpp)
target_include_directories(imgui PUBLIC
    thirdparty/imgui
    thirdparty/imgui/backends
    thirdparty/imgui/misc/freetype)
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS IMGUI_ENABLE_FREETYPE)
target_link_libraries(imgui PUBLIC glfw freetype)

# ---- app -----------------------------------------------------------------
file(GLOB_RECURSE SOLACE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)

if(APPLE)
    list(APPEND SOLACE_SOURCES ${CMAKE_SOURCE_DIR}/src/platform/macos.mm)
endif()

add_executable(Solace WIN32 MACOSX_BUNDLE
    ${SOLACE_SOURCES}
    ${VS_HEADERS}
    ${FS_HEADERS}
    $<$<PLATFORM_ID:Windows>:${CMAKE_SOURCE_DIR}/src/resources/Solace.rc>)

target_include_directories(Solace PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty/stb
    ${SHADER_OUT})

if(APPLE)
    find_library(QUARTZCORE_LIBRARY QuartzCore REQUIRED)
endif()

target_link_libraries(Solace PRIVATE imgui glfw bgfx bx bimg
    $<$<PLATFORM_ID:Darwin>:${QUARTZCORE_LIBRARY}>)

target_compile_definitions(Solace PRIVATE
    $<$<PLATFORM_ID:Windows>:GLFW_EXPOSE_NATIVE_WIN32;NOMINMAX;WIN32_LEAN_AND_MEAN>
    $<$<PLATFORM_ID:Darwin>:GLFW_EXPOSE_NATIVE_COCOA>
    $<$<PLATFORM_ID:Linux>:GLFW_EXPOSE_NATIVE_X11>)

set_source_files_properties(${CMAKE_SOURCE_DIR}/src/generated/fonts/geist_data.cpp
    PROPERTIES COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:MSVC>:/bigobj>")

if(APPLE)
    add_custom_command(TARGET Solace POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_BUNDLE_CONTENT_DIR:Solace>/Resources/assets)
endif()

if(MSVC)
    target_compile_options(Solace PRIVATE /W4 /WX /utf-8 /MP /permissive-)
    target_link_options(Solace PRIVATE /ENTRY:mainCRTStartup)
else()
    target_compile_options(Solace PRIVATE -Wall -Wextra)
endif()

set_target_properties(Solace PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
