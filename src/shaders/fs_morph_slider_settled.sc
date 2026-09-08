$input v_color0 , v_texcoord0

#include <bgfx_shader.sh>
#include "morph_slider_common.sh"

void main()
{
    // Completion guarantees progress == 1 and current == next. The aberration
    // envelope is therefore zero, so two RGB samples preserve the settled melt.
    vec2 frag = gl_FragCoord.xy;
    vec2 uv = animated_uv(frag);
    float nn = fbm(uv * u_look.x + u_look.w * 0.03);
    float warp = fbm(uv * u_look.x * 1.7 - u_look.w * 0.02);
    vec2 g = vec2(nn, warp) - 0.5;
    vec2 uvC = uv + g * u_progress.w * 0.5;
    float m = smoothstep(nn - 0.15, nn + 0.15, 1.0);
    vec2 sC = coverUV(uvC, u_rect.zw, u_sizes.xy);
    vec2 sN = coverUV(uv, u_rect.zw, u_sizes.xy);
    vec3 colC = texture2DLod(s_tex1, sC, 0.0).rgb;
    vec3 colN = texture2DLod(s_tex1, sN, 0.0).rgb;

    gl_FragColor = finish_pixel(frag, v_color0.a, uv, mix(colC, colN, m));
}
