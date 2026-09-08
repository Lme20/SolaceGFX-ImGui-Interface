#pragma once

#include "imgui.h"

namespace solace::rounded_panel
{
[[nodiscard]] bool init();
void shutdown();

// Draws an analytic rounded silhouette with an 8x8 filtered subpixel edge.
void draw(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max, ImU32 fill, float radius);
} // namespace solace::rounded_panel
