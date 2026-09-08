$input v_color0 , v_texcoord0

#include <bgfx_shader.sh>
#include "morph_slider_common.sh"

void main()
{
    vec2 frag = gl_FragCoord.xy;

    float uProgress = u_progress.x;
    float uDir = u_progress.y;
    int uMode = int(u_progress.z);
    float uIntensity = u_progress.w;
    float uScale = u_look.x;
    float uAberration = u_look.y;
    float uTime = u_look.w;
    vec2 uResolution = u_rect.zw;
    vec2 uPointer = u_pointer.xy;
    vec2 uv = animated_uv(frag);

    float p = clamp(uProgress, 0.0, 1.0);
    float env = sin(p * PI);
    vec2 uvC = uv;
    vec2 uvN = uv;
    float m = smoothstep(0.0, 1.0, p);

    if (uMode == 3)
    {
        vec2 c = uv - 0.5;
        float r = length(c);
        float ang = env * uIntensity * 3.5 * (1.0 - r);
        uvC = rot_mul(ang, c) + 0.5;
        uvN = rot_mul(-ang, c) + 0.5;
        m = smoothstep(0.0, 1.0, p);
    }
    else if (uMode == 1)
    {
        float d = distance(uv, uPointer);
        float ring = p * 1.6;
        float wave = sin((d - ring) * 30.0) * env;
        vec2 dir = normalize(uv - uPointer + vec2_splat(1e-4));
        vec2 disp = dir * wave * uIntensity * 0.25;
        uvC = uv + disp;
        uvN = uv + disp * 0.6;
        m = 1.0 - smoothstep(ring - 0.03, ring + 0.03, d);
    }
    else if (uMode == 2)
    {
        float slices = 14.0;
        float row = floor(uv.y * slices);
        float rnd = hash11(row);
        vec2 disp = vec2((rnd - 0.5) * env * uIntensity * 0.6, 0.0);
        uvC = uv + disp;
        uvN = uv + disp;
        float localX = uDir > 0.0 ? uv.x : 1.0 - uv.x;
        float th = p * 1.5 - 0.25 + (rnd - 0.5) * 0.25;
        m = 1.0 - smoothstep(th - 0.06, th + 0.06, localX);
    }
    else
    {
        float nn = fbm(uv * uScale + uTime * 0.03);
        float warp = fbm(uv * uScale * 1.7 - uTime * 0.02);
        vec2 g = vec2(nn, warp) - 0.5;
        uvC = uv + g * uIntensity * 0.5 * p;
        uvN = uv - g * uIntensity * 0.5 * (1.0 - p);
        m = smoothstep(nn - 0.15, nn + 0.15, p);
    }

    vec2 sC = coverUV(uvC, uResolution, u_sizes.xy);
    vec2 sN = coverUV(uvN, uResolution, u_sizes.zw);
    float ca = uAberration * env * 0.03;
    vec3 colC = vec3(
            texture2DLod(s_tex1, sC + vec2(ca, 0.0), 0.0).r,
            texture2DLod(s_tex1, sC, 0.0).g,
            texture2DLod(s_tex1, sC - vec2(ca, 0.0), 0.0).b);
    vec3 colN = vec3(
            texture2DLod(s_tex2, sN + vec2(ca, 0.0), 0.0).r,
            texture2DLod(s_tex2, sN, 0.0).g,
            texture2DLod(s_tex2, sN - vec2(ca, 0.0), 0.0).b);

    gl_FragColor = finish_pixel(frag, v_color0.a, uv, mix(colC, colN, m));
}
