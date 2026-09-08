#pragma once

#include "imgui.h"
#include <bgfx/bgfx.h>

namespace solace::imgui_bgfx
{
bool init(bgfx::ViewId first_view, bgfx::ViewId last_view);
void shutdown();
void new_frame();
void render_draw_data(ImDrawData* draw_data);

// ---- hooks used by graphics/gfx_helpers and graphics/snapshot ----------------
// Called from inside an ImDrawCallback: subsequent draw commands use `program`
// and call `apply(user)` right before each bgfx::submit (uniforms are per-submit).
using apply_fn = void (*)(void* user);
void set_program_override(bgfx::ProgramHandle program, apply_fn apply, void* user);
void clear_program_override();

// Called from inside an ImDrawCallback: moves subsequent draws to the next view id.
// Returns the new view id (so a blit can be queued on it), or the current one if exhausted.
bgfx::ViewId split_view();
bgfx::ViewId current_view();

// ImTextureID <-> bgfx handle. idx+1 so a valid handle 0 never equals ImTextureID_Invalid.
inline ImTextureID to_texture_id(bgfx::TextureHandle h)
{
    return bgfx::isValid(h) ? static_cast<ImTextureID>(h.idx) + 1 : ImTextureID_Invalid;
}
inline bgfx::TextureHandle from_texture_id(ImTextureID id)
{
    bgfx::TextureHandle h{static_cast<uint16_t>(id - 1)};
    return id == ImTextureID_Invalid ? bgfx::TextureHandle(BGFX_INVALID_HANDLE) : h;
}
} // namespace solace::imgui_bgfx
