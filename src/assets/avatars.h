#pragma once
#include "imgui.h"

#include <filesystem>

namespace solace::avatars
{
void load(const std::filesystem::path& people_directory,
          const std::filesystem::path& logo_directory,
          const std::filesystem::path& brand_directory);
void shutdown();

ImTextureID me();
ImTextureID other(int index);
ImTextureID logo(int index);

ImTextureID brand(const char* name);

bool draw(ImDrawList* draw_list, ImTextureID texture, const ImVec2& top_left, float size,
          float alpha, float rounding = -1.f);
} // namespace solace::avatars
