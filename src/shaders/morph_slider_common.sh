// Shared between fs_morph_slider.sc and fs_morph_slider_settled.sc.
uniform vec4 u_params[8];
#define u_rect     u_params[0]
#define u_card     u_params[1]
#define u_progress u_params[2]
#define u_look     u_params[3]
#define u_sizes    u_params[4]
#define u_pointer  u_params[5]
#define u_overlay  u_params[6]
#define u_fade     u_params[7]

SAMPLER2D(s_tex1, 1);   // tCurrent
SAMPLER2D(s_tex2, 2);   // tNext

#define PI 3.14159265359

float hash11(float p)
{
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

float hash21(vec2 p)
{
    vec3 p3 = fract(vec3(p.x, p.y, p.x) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p)
{
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 5; i++)
    {
        v += a * noise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

vec2 rot_mul(float a, vec2 v)
{
    float s = sin(a), c = cos(a);
    return vec2(c * v.x + s * v.y, -s * v.x + c * v.y);
}

vec2 coverUV(vec2 uv, vec2 res, vec2 img)
{
    float rA = res.x / max(res.y, 1.0);
    float iA = img.x / max(img.y, 1.0);
    vec2 s = vec2(1.0, 1.0);
    float ratio = rA / max(iA, 0.0001);
    if (ratio > 1.0) s.y = 1.0 / ratio;
    else             s.x = ratio;
    return (uv - 0.5) * s + 0.5;
}

float smootherstep(float t)
{
    t = saturate(t);
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float rounded_box(vec2 p, vec2 half_size, float r)
{
    vec2 q = abs(p) - half_size + r;
    return length(max(q, vec2_splat(0.0))) + min(max(q.x, q.y), 0.0) - r;
}

// frag = gl_FragCoord.xy (framebuffer pixels, top-left origin on D3D/Metal/Vulkan)
vec2 animated_uv(vec2 frag)
{
    vec2 uResolution = u_rect.zw;
    vec2 local = frag - u_rect.xy;
    vec2 vUv = vec2(local.x / uResolution.x, 1.0 - local.y / uResolution.y);
    vec2 uv = vUv;
    uv += vec2(sin(u_look.w * 0.25 + uv.y * 4.0),
               cos(u_look.w * 0.22 + uv.x * 4.0)) * u_look.z * 0.008;
    return (uv - 0.5) * (1.0 - u_look.z * 0.02 * sin(u_look.w * 0.4)) + 0.5;
}

vec4 finish_pixel(vec2 frag, float vertex_alpha, vec2 uv, vec3 col)
{
    float vig = smoothstep(1.25, 0.25, length(uv - 0.5));
    col = mix(col, u_overlay.rgb, (1.0 - vig) * 0.28);

    vec2 half_size = u_card.zw * 0.5;
    vec2 centre = u_card.xy + half_size;
    float d = rounded_box(frag - centre, half_size, u_pointer.z);
    float inside = 1.0 - smoothstep(-0.75, 0.75, d);

    float cover = 0.0;
    if (u_fade.x > 0.5) cover = max(cover, 1.0 - smootherstep((frag.x - u_rect.x) / u_fade.x));
    if (u_fade.y > 0.5) cover = max(cover, 1.0 - smootherstep((frag.y - u_rect.y) / u_fade.y));
    if (u_fade.z > 0.5) cover = max(cover, 1.0 - smootherstep(((u_rect.y + u_rect.w) - frag.y) / u_fade.z));

    float a = inside * (1.0 - cover) * u_pointer.w * vertex_alpha;
    if (a <= 0.002)
        discard;

    return vec4(col, a);
}
