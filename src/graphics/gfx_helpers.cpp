#include "graphics/gfx_helpers.h"
#include "core/diagnostics.h"
#include "graphics/embedded_shaders.h"
#include "graphics/imgui_impl_bgfx.h"

#include <cstring>

namespace solace::gfx
{
ImTextureID texture::id() const
{
    return imgui_bgfx::to_texture_id(handle_);
}

texture create_rgba_texture(const unsigned char* pixels, unsigned int width, unsigned int height,
                            bool generate_mips)
{
    if (!pixels || width == 0 || height == 0 || width > UINT16_MAX || height > UINT16_MAX)
        return {};

    const bgfx::Memory* mem = bgfx::copy(pixels, width * height * 4u);

    // Create bgfx texture and upload at base level separately
    bgfx::TextureHandle h = bgfx::createTexture2D(
        static_cast<uint16_t>(width), static_cast<uint16_t>(height), generate_mips, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | (generate_mips ? BGFX_TEXTURE_RT : 0));

    if (!bgfx::isValid(h))
        return {};
    bgfx::updateTexture2D(h, 0, 0, 0, 0, static_cast<uint16_t>(width),
                          static_cast<uint16_t>(height), mem);
    return texture{h};
}

bool pixel_shader_pass::initialize(const char* fragment_shader_name, const char* debug_name,
                                   std::size_t constants_size, unsigned int sampler_count)
{
    reset();
    if (constants_size == 0 || constants_size % 16 != 0)
        return false;

    const bgfx::RendererType::Enum type = bgfx::getRendererType();
    bgfx::ShaderHandle vs = bgfx::createEmbeddedShader(k_embedded_shaders, type, "vs_imgui");
    bgfx::ShaderHandle fs =
        bgfx::createEmbeddedShader(k_embedded_shaders, type, fragment_shader_name);
    if (!bgfx::isValid(vs) || !bgfx::isValid(fs))
    {
        diagnostics::warning("gfx", debug_name);
        if (bgfx::isValid(vs))
            bgfx::destroy(vs);
        if (bgfx::isValid(fs))
            bgfx::destroy(fs);
        return false;
    }
    program_ = bgfx::createProgram(vs, fs, true);
    if (!bgfx::isValid(program_))
        return false;

    vec4_count_ = static_cast<std::uint16_t>(constants_size / 16);
    u_params_ = bgfx::createUniform("u_params", bgfx::UniformType::Vec4, vec4_count_);
    staged_constants_.assign(constants_size, 0);

    for (unsigned int i = 0; i < sampler_count; ++i)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "s_tex%u", i + 1);
        samplers_.push_back(bgfx::createUniform(name, bgfx::UniformType::Sampler));
    }
    return true;
}

void pixel_shader_pass::reset()
{
    for (auto& s : samplers_)
        if (bgfx::isValid(s))
            bgfx::destroy(s);
    samplers_.clear();
    if (bgfx::isValid(u_params_))
        bgfx::destroy(u_params_);
    if (bgfx::isValid(program_))
        bgfx::destroy(program_);
    u_params_ = BGFX_INVALID_HANDLE;
    program_ = BGFX_INVALID_HANDLE;
    staged_constants_.clear();
    staged_textures_.clear();
    vec4_count_ = 0;
}

bool pixel_shader_pass::ready() const
{
    return bgfx::isValid(program_);
}

bool pixel_shader_pass::upload_constants(const void* data, std::size_t size)
{
    if (!ready() || !data || size != staged_constants_.size())
        return false;
    std::memcpy(staged_constants_.data(), data, size); // applied per-submit in apply()
    return true;
}

void pixel_shader_pass::bind(const bgfx::TextureHandle* textures, unsigned int texture_count,
                             std::uint32_t sampler_flags)
{
    if (!ready())
        return;
    staged_textures_.assign(
        textures, textures + std::min<unsigned int>(texture_count,
                                                    static_cast<unsigned int>(samplers_.size())));
    staged_sampler_flags_ = sampler_flags;
    imgui_bgfx::set_program_override(program_, &pixel_shader_pass::apply_thunk, this);
}

void pixel_shader_pass::apply_thunk(void* user)
{
    static_cast<pixel_shader_pass*>(user)->apply();
}

void pixel_shader_pass::apply() const
{
    bgfx::setUniform(u_params_, staged_constants_.data(), vec4_count_);
    for (std::size_t i = 0; i < staged_textures_.size(); ++i)
        bgfx::setTexture(static_cast<uint8_t>(i + 1), samplers_[i], staged_textures_[i],
                         staged_sampler_flags_);
}
} // namespace solace::gfx
