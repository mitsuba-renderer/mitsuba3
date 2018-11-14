#include <optix_world.h>

using namespace optix;

rtDeclareVariable(float3, p, attribute p, );
rtDeclareVariable(float2, uv, attribute uv, );
rtDeclareVariable(float3, ns, attribute ns, );
rtDeclareVariable(float3, ng, attribute ng, );
rtDeclareVariable(float3, dp_du, attribute dp_du, );
rtDeclareVariable(float3, dp_dv, attribute dp_dv, );

rtBuffer<uint3> faces;
rtBuffer<float3> vertex_positions;
rtBuffer<float3> vertex_normals;
rtBuffer<float2> vertex_texcoords;

__device__ void coordinate_system(float3 n, float3 &x, float3 &y) {
    /* Based on "Building an Orthonormal Basis, Revisited" by
       Tom Duff, James Burgess, Per Christensen,
       Christophe Hery, Andrew Kensler, Max Liani,
       and Ryusuke Villemin (JCGT Vol 6, No 1, 2017) */

    float s = copysignf(1.f, n.z),
          a = -1.f / (s + n.z),
          b = n.x * n.y * a;

    x = make_float3(n.x * n.x * a * s + 1.f, b * s, -n.x * s);
    y = make_float3(b, s + n.y * n.y * a, -n.y);
}

RT_PROGRAM void ray_attr() {
    uv = rtGetTriangleBarycentrics();
    float uv0 = 1.f - uv.x - uv.y,
          uv1 = uv.x,
          uv2 = uv.y;

    uint3 face = faces[rtGetPrimitiveIndex()];

    float3 p0 = vertex_positions[face.x],
           p1 = vertex_positions[face.y],
           p2 = vertex_positions[face.z];

    float3 dp0 = p1 - p0,
           dp1 = p2 - p0;

    p = p0 * uv0 + p1 * uv1 + p2 * uv2;

    ng = normalize(cross(dp0, dp1));
    coordinate_system(ng, dp_du, dp_dv);

    if (vertex_normals.size() > 0) {
        float3 n0 = vertex_normals[face.x],
               n1 = vertex_normals[face.y],
               n2 = vertex_normals[face.z];

        ns = n0 * uv0 + n1 * uv1 + n2 * uv2;
    } else {
        ns = ng;
    }

    if (vertex_texcoords.size() > 0) {
        float2 t0 = vertex_texcoords[face.x],
               t1 = vertex_texcoords[face.y],
               t2 = vertex_texcoords[face.z];

        uv = t0 * uv0 + t1 * uv1 + t2 * uv2;

        float2 dt0 = t1 - t0, dt1 = t2 - t0;
        float det = dt0.x * dt1.y - dt0.y * dt1.x;

        if (det != 0.f) {
            float inv_det = 1.f / det;
            dp_du = ( dt1.y * dp0 - dt0.y * dp1) * inv_det;
            dp_dv = (-dt1.x * dp0 + dt0.x * dp1) * inv_det;
        }
    }
}
