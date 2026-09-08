#pragma once

#include <bgfx/bgfx.h>
#include <imgui.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace solace::gfx
{

class texture
{
  public:
    texture() = default;
    explicit texture(bgfx::TextureHandle h) : handle_(h) {}
    ~texture()
    {
        reset();
    }
    texture(texture&& o) noexcept : handle_(o.handle_)
    {
        o.handle_ = BGFX_INVALID_HANDLE;
    }
    texture& operator=(texture&& o) noexcept
    {
        if (this != &o)
        {
            reset();
            handle_ = o.handle_;
            o.handle_ = BGFX_INVALID_HANDLE;
        }
        return *this;
    }
    texture(const texture&) = delete;
    texture& operator=(const texture&) = delete;

    void reset()
    {
        if (bgfx::isValid(handle_))
            bgfx::destroy(handle_);
        handle_ = BGFX_INVALID_HANDLE;
    }
    [[nodiscard]] bool valid() const
    {
        return bgfx::isValid(handle_);
    }
    [[nodiscard]] bgfx::TextureHandle get() const
    {
        return handle_;
    }
    [[nodiscard]] ImTextureID id() const;

  private:
    bgfx::TextureHandle handle_ = BGFX_INVALID_HANDLE;
};

texture create_rgba_texture(const unsigned char* pixels, unsigned int width, unsigned int height,
                            bool generate_mips = true);

class pixel_shader_pass
{
  public:
    pixel_shader_pass() = default;
    pixel_shader_pass(const pixel_shader_pass&) = delete;
    pixel_shader_pass& operator=(const pixel_shader_pass&) = delete;
    ~pixel_shader_pass()
    {
        reset();
    }

    // fragment_shader_name: embedded shader name, e.g. "fs_rounded_panel".
    // constants_size must be a multiple of 16; exposed to the shader as `uniform vec4 u_params[N]`.
    // sampler_count: number of SAMPLER2D stages the shader binds, starting at stage 1.
    bool initialize(const char* fragment_shader_name, const char* debug_name,
                    std::size_t constants_size, unsigned int sampler_count);
    void reset();
    [[nodiscard]] bool ready() const;

    bool upload_constants(const void* data, std::size_t size);
    void bind(const bgfx::TextureHandle* textures = nullptr, unsigned int texture_count = 0,
              std::uint32_t sampler_flags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

  private:
    static void apply_thunk(void* user);
    void apply() const;

    bgfx::ProgramHandle program_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_params_ = BGFX_INVALID_HANDLE;
    std::vector<bgfx::UniformHandle> samplers_;
    std::vector<std::uint8_t> staged_constants_;
    std::vector<bgfx::TextureHandle> staged_textures_;
    std::uint32_t staged_sampler_flags_ = 0;
    std::uint16_t vec4_count_ = 0;
};
} // namespace solace::gfx
