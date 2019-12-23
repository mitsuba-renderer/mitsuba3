#include <metal_stdlib>

using namespace metal;

/* http://paulbourke.net/texture_colour/colourspace/ */
float3 colormap(float v, float vmin, float vmax) {
    float3 c = float3(1.f);
    if (v < vmin)
        v = vmin;
    if (v > vmax)
        v = vmax;
    float dv = vmax - vmin;
    if (v < (vmin + 0.25f * dv)) {
        c.r = 0.f;
        c.g = 4.f * (v - vmin) / dv;
    } else if (v < (vmin + 0.5f * dv)) {
        c.r = 0.f;
        c.b = 1.f + 4.f * (vmin + 0.25f * dv - v) / dv;
    } else if (v < (vmin + 0.75f * dv)) {
        c.r = 4.f * (v - vmin - 0.5f * dv) / dv;
        c.b = 0.f;
    } else {
        c.g = 1.f + 4.f * (vmin + 0.75f * dv - v) / dv;
        c.b = 0.f;
    }
    return c;
}

struct VertexOut {
    float4 position [[position]];
    float2 uv;
};

fragment float4 fragment_main(VertexOut vert [[stage_in]],
                              texture2d<float, access::sample> tex,
                              sampler tex_sampler) {
    float value = tex.sample(tex_sampler, vert.uv).r;
    return float4(colormap(value, 0.0, 1.f), 1.f);
}
