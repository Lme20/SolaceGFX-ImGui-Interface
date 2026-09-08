$input v_color0 , v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_params[70];
#define u_res        u_params[0]
#define u_shape      u_params[1]
#define u_glass      u_params[2]
#define u_glass2     u_params[3]
#define u_warp       u_params[4]
#define u_bg         u_params[5]
#define u_point(i)   u_params[6 + (i)]
#define MAX_POINTS   64

SAMPLER2D(s_tex1, 1); // tBackdrop

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float vnoise(vec2 p)
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

float fbm2(vec2 p)
{
    return vnoise(p) * 0.65 + vnoise(p * 2.03 + 11.7) * 0.35;
}

vec2 warp_point(vec2 p)
{
    vec2 q = p * u_warp.y;
    float t = u_warp.z;
    vec2 n = vec2(fbm2(q + vec2(0.0, t)), fbm2(q + vec2(37.4, -t)));
    return p + (n - 0.5) * u_warp.x;
}

float link_sdf(vec2 p, vec4 a, vec4 b)
{
    vec2 ba = b.xy - a.xy;
    float l2 = dot(ba, ba);

    if (l2 < 1e-5)
        return length(p - a.xy) - max(a.z, b.z);

    float rr = a.z - b.z;
    float a2 = l2 - rr * rr;
    float il2 = 1.0 / l2;

    vec2 pa = p - a.xy;
    float y = dot(pa, ba);
    float z = y - l2;

    vec2 w = pa * l2 - ba * y;
    float x2 = dot(w, w);
    float y2 = y * y * l2;
    float z2 = z * z * l2;

    float k = sign(rr) * rr * rr * x2;

    if (sign(z) * a2 * z2 > k) return sqrt(x2 + z2) * il2 - b.z;
    if (sign(y) * a2 * y2 < k) return sqrt(x2 + y2) * il2 - a.z;
    return (sqrt(max(x2 * a2 * il2, 0.0)) + y * rr) * il2 - a.z;
}

float trail_sdf(vec2 p)
{
    vec2 w = warp_point(p);
    int n = int(u_shape.w);

    float d = 1e9;
    // Constant trip count + early break: portable across GLSL/ESSL/HLSL/MSL.
    for (int i = 0; i < MAX_POINTS - 1; i++)
    {
        if (i >= n - 1) break;
        d = min(d, link_sdf(w, u_point(i), u_point(i + 1)));
    }
    return d + u_shape.y;
}

float erf_approx(float x)
{
    // tanh(1.7724538509 * x), written without tanh() for ESSL compatibility
    float e = exp(2.0 * 1.7724538509 * x);
    return 1.0 - 2.0 / (e + 1.0);
}

float dome_height(float inside, float blob, float zR)
{
    if (inside <= 0.0) return 0.0;

    float s = max(blob - min(inside, blob), 0.0);
    float R = (blob * blob + zR * zR) / (2.0 * zR);
    return sqrt(max(R * R - s * s, 0.0)) - (R - zR);
}

vec4 sample_bg(vec2 uv)
{
    return texture2DLod(s_tex1, uv, 0.0);
}

// 12-tap ring, unrolled (constant array initialisers are not portable through shaderc).
vec4 sample_blur(vec2 uv, float radius_px)
{
    if (radius_px < 0.5) return sample_bg(uv);

    vec2 s = radius_px * u_res.zw;
    vec4 acc = sample_bg(uv) * 0.16;

    acc += sample_bg(uv + vec2(1.000, 0.000) * s) * 0.05;
    acc += sample_bg(uv + vec2(0.500, 0.866) * s) * 0.05;
    acc += sample_bg(uv + vec2(-0.500, 0.866) * s) * 0.05;
    acc += sample_bg(uv + vec2(-1.000, 0.000) * s) * 0.05;
    acc += sample_bg(uv + vec2(-0.500, -0.866) * s) * 0.05;
    acc += sample_bg(uv + vec2(0.500, -0.866) * s) * 0.05;

    acc += sample_bg(uv + vec2(0.383, 0.321) * s) * 0.09;
    acc += sample_bg(uv + vec2(-0.117, 0.484) * s) * 0.09;
    acc += sample_bg(uv + vec2(-0.484, 0.117) * s) * 0.09;
    acc += sample_bg(uv + vec2(-0.383, -0.321) * s) * 0.09;
    acc += sample_bg(uv + vec2(0.117, -0.484) * s) * 0.09;
    acc += sample_bg(uv + vec2(0.484, -0.117) * s) * 0.09;

    return acc;
}

void main()
{
    vec2 px = gl_FragCoord.xy;
    float sdf = trail_sdf(px);

    float mask = 1.0 - smoothstep(-2.5, 1.0, sdf);
    if (mask <= 0.002)
        discard;

    float blob = u_shape.x;
    float inside = -sdf;
    float edge = smoothstep(blob * 0.35, 0.0, inside);

    float zR = blob * 0.75;
    float e = 2.0;
    float dC = inside;
    float dR = -trail_sdf(px + vec2(e, 0.0));
    float dL = -trail_sdf(px - vec2(e, 0.0));
    float dU = -trail_sdf(px + vec2(0.0, e));
    float dD = -trail_sdf(px - vec2(0.0, e));

    float hC = dome_height(dC, blob, zR);
    float hR = dome_height(dR, blob, zR);
    float hL = dome_height(dL, blob, zR);
    float hU = dome_height(dU, blob, zR);
    float hD = dome_height(dD, blob, zR);

    vec2 hGrad = vec2(hR - hL, hU - hD) / (2.0 * e);
    vec3 N = normalize(vec3(-hGrad, 1.0));

    float depth = smoothstep(0.0, zR, inside);

    vec2 pxToUV = u_res.zw;
    float ior = 1.5;
    float refrPow = 1.0 - 1.0 / ior;
    float thickness = hC * 2.0;
    float thickNorm = thickness / max(zR * 2.0, 1.0);

    vec2 exitRefr = hGrad * refrPow;
    vec2 entryRefr = hGrad * refrPow;
    vec2 throughRefr = entryRefr * thickNorm * 0.5;
    vec2 refrPx = (exitRefr + entryRefr + throughRefr) * u_glass.x * 30.0;

    float falloff = erf_approx(inside / max(blob * 0.5 * 1.41421356, 1e-3));
    float rimLip = pow(edge, 10.0);

    refrPx *= falloff * (1.0 - 0.35 * depth * depth);
    refrPx += hGrad * rimLip * u_glass.x * 42.0;

    vec2 gradSdf = vec2(dL - dR, dD - dU) / (2.0 * e);
    vec2 centerDir = (length(gradSdf) > 1e-5) ? -normalize(gradSdf) : vec2(0.0, 0.0);
    refrPx += centerDir * (1.0 - depth) * u_glass.x * 4.0 * depth;

    vec2 refr = refrPx * pxToUV;

    float caS = u_glass2.x * 18.0 * (edge * 0.7 + 0.3) * 2.0;
    vec2 caD = N.xy * caS * pxToUV;

    vec2 flat_uv = px * pxToUV;
    vec2 base = flat_uv + refr;
    float oob = max(max(-base.x, base.x - 1.0), max(-base.y, base.y - 1.0));
    base = mix(base, flat_uv, smoothstep(0.0, 0.012, oob));

    vec4 sG = sample_bg(base);
    vec3 sharp = vec3(sample_bg(base + caD).r, sG.g, sample_bg(base - caD).b);

    vec4 bG = sample_blur(base, u_glass.y);

    float frost = saturate(u_glass2.x * 0.0 + u_shape.z);
    float edgeMix = (0.20 + 0.80 * edge) * frost;
    vec3 col = mix(sharp, bG.rgb, edgeMix);

    float bga = mix(sG.a, bG.a, edgeMix);
    col = mix(u_bg.rgb, col, saturate(bga));

    col *= 1.0 + u_glass2.w;

    float lum = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3_splat(lum), col, 1.0 + u_glass2.z);

    col = mix(col, col * vec3(0.92, 0.95, 1.05), u_glass2.y);
    col *= 1.0 + 0.06 * depth;

    float fres = pow(1.0 - abs(N.z), 4.0);

    vec3 V = vec3(0.0, 0.0, 1.0);
    vec3 L1 = normalize(vec3(0.4, 0.7, 1.0));
    vec3 H1 = normalize(L1 + V);
    float sp1 = pow(max(dot(N, H1), 0.0), 90.0);
    vec3 L2 = normalize(vec3(-0.3, -0.5, 1.0));
    vec3 H2 = normalize(L2 + V);
    float sp2 = pow(max(dot(N, H2), 0.0), 50.0) * 0.3;
    vec3 L3 = normalize(vec3(0.1, 0.3, 1.0));
    float spB = pow(max(dot(N, L3), 0.0), 6.0) * 0.1;
    vec3 L4 = normalize(vec3(0.0, 0.9, 0.4));
    vec3 H4 = normalize(L4 + V);
    float sp4 = pow(max(dot(N, H4), 0.0), 120.0) * 0.6;
    float totalSpec = (sp1 + sp2 + spB + sp4) * u_glass.w;

    float borderWidth = 1.5;
    float innerStroke = smoothstep(-borderWidth - 1.0, -borderWidth, sdf)
            * (1.0 - smoothstep(-1.0, 0.0, sdf));
    float topBias = 0.5 + 0.5 * centerDir.y;
    innerStroke *= (0.4 + 0.6 * topBias);

    float rim = edge * u_glass.z * 0.22;
    float innerGlow = smoothstep(5.0, 0.0, -sdf) * u_glass.z * 0.15;
    float envRefl = (N.y * 0.5 + 0.5) * fres * 0.08;

    vec3 fin = col;
    fin += totalSpec;
    fin += rim + innerGlow;
    fin += innerStroke * u_glass.z * 0.55;
    fin += envRefl;

    fin = mix(fin, vec3_splat(1.0), fres * 0.2 * saturate(u_glass.z * 4.0));

    float a = mask * u_warp.w * v_color0.a;
    if (a <= 0.002)
        discard;

    gl_FragColor = vec4(fin, a);
}
