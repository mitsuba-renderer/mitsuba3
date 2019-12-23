#include <metal_stdlib>

using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float pointsize [[point_size]];
    float4 frag_color;
};

fragment float4 fragment_main(VertexOut vert [[stage_in]]) {
    if (vert.frag_color.w == 0.f)
        discard_fragment();

    return vert.frag_color;
}
