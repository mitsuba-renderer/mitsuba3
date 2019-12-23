#include <metal_stdlib>

using namespace metal;

struct VertexOut {
    float4 position [[position]];
};

vertex VertexOut vertex_main(const device packed_float3 *position,
                             constant float4x4 &mvp,
                             uint id [[vertex_id]]) {
    VertexOut result;
    result.position = mvp * float4(position[id], 1.f);
    return result;
}
