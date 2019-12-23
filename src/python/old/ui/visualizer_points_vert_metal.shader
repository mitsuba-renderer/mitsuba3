#include <metal_stdlib>

using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float pointsize [[point_size]];
    float4 frag_color;
};

vertex VertexOut vertex_main(const device packed_float3 *position,
                             const device packed_float3 *color,
                             constant float4x4 &mvp,
                             uint id [[vertex_id]]) {
    VertexOut result;

    result.position = mvp * float4(position[id], 1.f);
    result.pointsize = 3.f;

    if (isnan(position[id][0])) /* nan (missing value) */
        result.frag_color = float4(0.f);
    else
        result.frag_color = float4(color[id], 1.f);

    return result;
}
