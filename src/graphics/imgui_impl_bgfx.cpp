#include "graphics/imgui_impl_bgfx.h"
#include "graphics/embedded_shaders.h"

#include <bx/math.h>
#include <cstring>

namespace solace::imgui_bgfx
{
namespace
{
struct state
{
    bgfx::VertexLayout layout;
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_tex = BGFX_INVALID_HANDLE;
    bgfx::ViewId first_view = 0, last_view = 0, view = 0;

    bgfx::ProgramHandle override_program = BGFX_INVALID_HANDLE;
    apply_fn override_apply = nullptr;
    void* override_user = nullptr;

    float ortho[16]{};
    bool initialized = false;
};
state g;

void apply_view_transform(bgfx::ViewId v)
{
    bgfx::setViewTransform(v, nullptr, g.ortho);
}

void update_texture(ImTextureData* tex)
{
    if (tex->Status == ImTextureStatus_WantCreate)
    {
        IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);
        bgfx::TextureHandle h = bgfx::createTexture2D(
            static_cast<uint16_t>(tex->Width), static_cast<uint16_t>(tex->Height), false, 1,
            bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            nullptr /*mutable*/);
        const bgfx::Memory* mem =
            bgfx::copy(tex->GetPixels(), static_cast<uint32_t>(tex->GetSizeInBytes()));
        bgfx::updateTexture2D(h, 0, 0, 0, 0, static_cast<uint16_t>(tex->Width),
                              static_cast<uint16_t>(tex->Height), mem);
        tex->SetTexID(to_texture_id(h));
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantUpdates)
    {
        bgfx::TextureHandle h = from_texture_id(tex->TexID);
        for (const ImTextureRect& r : tex->Updates)
        {
            const bgfx::Memory* mem =
                bgfx::alloc(static_cast<uint32_t>(r.w) * r.h * tex->BytesPerPixel);
            for (int y = 0; y < r.h; ++y)
                std::memcpy(mem->data + static_cast<size_t>(y) * r.w * tex->BytesPerPixel,
                            tex->GetPixelsAt(r.x, r.y + y),
                            static_cast<size_t>(r.w) * tex->BytesPerPixel);
            bgfx::updateTexture2D(h, 0, 0, r.x, r.y, r.w, r.h, mem);
        }
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0)
    {
        bgfx::TextureHandle h = from_texture_id(tex->TexID);
        if (bgfx::isValid(h))
            bgfx::destroy(h);
        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}
} // namespace

bool init(bgfx::ViewId first_view, bgfx::ViewId last_view)
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == nullptr);
    io.BackendRendererName = "solace_imgui_impl_bgfx";
    io.BackendRendererUserData = &g;
    io.BackendFlags |=
        ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;

    g.first_view = first_view;
    g.last_view = last_view;

    g.layout.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();
    static_assert(sizeof(ImDrawVert) == 20);

    const bgfx::RendererType::Enum type = bgfx::getRendererType();
    g.program = bgfx::createProgram(
        bgfx::createEmbeddedShader(solace::gfx::k_embedded_shaders, type, "vs_imgui"),
        bgfx::createEmbeddedShader(solace::gfx::k_embedded_shaders, type, "fs_imgui"), true);
    g.s_tex = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
    g.initialized = bgfx::isValid(g.program);
    return g.initialized;
}

void shutdown()
{
    if (!g.initialized)
        return;
    if (ImFontAtlas* atlas = ImGui::GetIO().Fonts)
        for (ImTextureData* tex : atlas->TexList)
            if (tex->TexID != ImTextureID_Invalid)
            {
                bgfx::destroy(from_texture_id(tex->TexID));
                tex->SetTexID(ImTextureID_Invalid);
                tex->SetStatus(ImTextureStatus_Destroyed);
            }
    bgfx::destroy(g.program);
    bgfx::destroy(g.s_tex);
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &=
        ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
    g = state{};
}

void new_frame()
{
    g.view = g.first_view;
    clear_program_override();
}

void set_program_override(bgfx::ProgramHandle program, apply_fn apply, void* user)
{
    g.override_program = program;
    g.override_apply = apply;
    g.override_user = user;
}
void clear_program_override()
{
    g.override_program = BGFX_INVALID_HANDLE;
    g.override_apply = nullptr;
    g.override_user = nullptr;
}
bgfx::ViewId current_view()
{
    return g.view;
}
bgfx::ViewId split_view()
{
    if (g.view < g.last_view)
    {
        ++g.view;
        apply_view_transform(g.view);
    }
    return g.view;
}

void render_draw_data(ImDrawData* dd)
{
    if (!dd || dd->DisplaySize.x <= 0.f || dd->DisplaySize.y <= 0.f)
        return;

    if (dd->Textures)
        for (ImTextureData* tex : *dd->Textures)
            if (tex->Status != ImTextureStatus_OK)
                update_texture(tex);

    const bgfx::Caps* caps = bgfx::getCaps();
    const float L = dd->DisplayPos.x, R = L + dd->DisplaySize.x;
    const float T = dd->DisplayPos.y, B = T + dd->DisplaySize.y;
    bx::mtxOrtho(g.ortho, L, R, B, T, 0.f, 1000.f, 0.f, caps->homogeneousDepth);
    apply_view_transform(g.view);

    const ImVec2 clip_off = dd->DisplayPos, clip_scale = dd->FramebufferScale;
    const int fb_w = static_cast<int>(dd->DisplaySize.x * clip_scale.x);
    const int fb_h = static_cast<int>(dd->DisplaySize.y * clip_scale.y);

    for (int n = 0; n < dd->CmdListsCount; ++n)
    {
        const ImDrawList* cl = dd->CmdLists[n];
        const uint32_t nv = static_cast<uint32_t>(cl->VtxBuffer.Size);
        const uint32_t ni = static_cast<uint32_t>(cl->IdxBuffer.Size);
        if (nv == 0 || ni == 0)
            continue;

        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer tib;
        if (!bgfx::allocTransientBuffers(&tvb, g.layout, nv, &tib, ni, sizeof(ImDrawIdx) == 4))
            break; // out of transient memory; raise Init.limits.transientVbSize/transientIbSize
        std::memcpy(tvb.data, cl->VtxBuffer.Data, nv * sizeof(ImDrawVert));
        std::memcpy(tib.data, cl->IdxBuffer.Data, ni * sizeof(ImDrawIdx));

        for (const ImDrawCmd& cmd : cl->CmdBuffer)
        {
            if (cmd.UserCallback)
            {
                if (cmd.UserCallback == ImDrawCallback_ResetRenderState)
                    clear_program_override();
                else
                    cmd.UserCallback(cl, &cmd); // rounded_panel / morph_slider / glass / snapshot
                continue;
            }
            if (cmd.ElemCount == 0)
                continue;

            ImVec2 cmin((cmd.ClipRect.x - clip_off.x) * clip_scale.x,
                        (cmd.ClipRect.y - clip_off.y) * clip_scale.y);
            ImVec2 cmax((cmd.ClipRect.z - clip_off.x) * clip_scale.x,
                        (cmd.ClipRect.w - clip_off.y) * clip_scale.y);
            cmin.x = bx::max(cmin.x, 0.f);
            cmin.y = bx::max(cmin.y, 0.f);
            cmax.x = bx::min(cmax.x, static_cast<float>(fb_w));
            cmax.y = bx::min(cmax.y, static_cast<float>(fb_h));
            if (cmax.x <= cmin.x || cmax.y <= cmin.y)
                continue;

            bgfx::setScissor(static_cast<uint16_t>(cmin.x), static_cast<uint16_t>(cmin.y),
                             static_cast<uint16_t>(cmax.x - cmin.x),
                             static_cast<uint16_t>(cmax.y - cmin.y));
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_BLEND_FUNC_SEPARATE(
                               BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA,
                               BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
            bgfx::setTexture(0, g.s_tex, from_texture_id(cmd.GetTexID()));
            bgfx::setVertexBuffer(0, &tvb, cmd.VtxOffset, nv - cmd.VtxOffset);
            bgfx::setIndexBuffer(&tib, cmd.IdxOffset, cmd.ElemCount);

            if (bgfx::isValid(g.override_program))
            {
                if (g.override_apply)
                    g.override_apply(g.override_user);
                bgfx::submit(g.view, g.override_program);
            }
            else
                bgfx::submit(g.view, g.program);
        }
    }
}
} // namespace solace::imgui_bgfx
