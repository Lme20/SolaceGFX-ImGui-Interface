#pragma once
#include "imgui.h"
#include <bgfx/bgfx.h>

namespace solace::snapshot
{
void request();
bool ready();
ImTextureID texture();
bgfx::TextureHandle texture_handle();
void poll();

void release();
void shutdown();
void attach(bgfx::TextureHandle scene_rt, unsigned width, unsigned height,
            bgfx::ViewId composite_view);

void capture_backdrop(ImDrawList* draw_list);
void invalidate_backdrop();
bool backdrop_ready();

ImTextureID backdrop();
bgfx::TextureHandle backdrop_handle();
} // namespace solace::snapshot
