#include <metal_stdlib>

using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 uv;
};

vertex VertexOut vertex_main(const device float2 *position,
                             constant float4x4 &mvp,
                             uint id [[vertex_id]]) {
    VertexOut result;
    result.position = mvp * float4(position[id], 0.f, 1.f);
    result.uv = position[id];
    return result;
}
