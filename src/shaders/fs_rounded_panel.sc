$input v_color0 , v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_params[4];
#define u_bounds             u_params[0]
#define u_fill               u_params[1]
#define u_shape              u_params[2]
#define u_viewport_transform u_params[3]

float rounded_box(vec2 p, vec2 half_size, float radius)
{
    vec2 q = abs(p) - half_size + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main()
{
    vec2 inv_fb = u_viewport_transform.zw;
    vec2 pixel = gl_FragCoord.xy * inv_fb + u_viewport_transform.xy;
    vec2 center = (u_bounds.xy + u_bounds.zw) * 0.5;
    vec2 half_sz = max((u_bounds.zw - u_bounds.xy) * 0.5, 0.0);
    float radius = min(max(u_shape.x, 0.0), min(half_sz.x, half_sz.y));
    float d = rounded_box(pixel - center, half_sz, radius);

    if (d > 1.0) {
        gl_FragColor = vec4_splat(0.0);
        return;
    }
    if (d < -1.0) {
        gl_FragColor = u_fill;
        return;
    }

    float coverage = 0.0;
    float feather = 0.5 * max(inv_fb.x, inv_fb.y);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
        {
            vec2 offset = (vec2(float(x), float(y)) + 0.5) * (1.0 / 8.0) - 0.5;
            float dd = rounded_box(pixel + offset * inv_fb - center, half_sz, radius);
            coverage += saturate(0.5 - dd / feather);
        }
    gl_FragColor = vec4(u_fill.rgb, u_fill.a * coverage * (1.0 / 64.0));
}
