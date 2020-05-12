#pragma once

#include <mitsuba/render/optix/common.h>

struct OptixMeshData {
    const optix::Vector3u *faces;
    const optix::Vector3f *vertex_positions;
    const optix::Vector3f *vertex_normals;
    const optix::Vector2f *vertex_texcoords;
};

#ifdef __CUDACC__

/// Unaligned load from buffer
template <typename T>
DEVICE Array<T, 2> load_2d(const Array<T, 2> *buf, size_t i) {
    T *ptr = (T*)buf + 2 * i;
    return Array<T, 2>(ptr[0], ptr[1]);
}
template <typename T>
DEVICE Array<T, 3> load_3d(const Array<T, 3> *buf, size_t i) {
    T *ptr = (T*)buf + 3 * i;
    return Array<T, 3>(ptr[0], ptr[1], ptr[2]);
}

extern "C" __global__ void __closesthit__mesh() {
    unsigned int launch_index = calculate_launch_index();

    if (params.out_hit != nullptr) {
        params.out_hit[launch_index] = true;
    } else {
        const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
        OptixMeshData *mesh = (OptixMeshData *)sbt_data->data;

        /* Compute and store information describing the intersection. This is
           very similar to Mesh::fill_surface_interaction() */

        float2 float2_uv = optixGetTriangleBarycentrics();
        Vector2f uv = Vector2f(float2_uv.x, float2_uv.y);
        float uv0 = 1.f - uv.x() - uv.y(),
              uv1 = uv.x(),
              uv2 = uv.y();

        Vector3u face = load_3d(mesh->faces, optixGetPrimitiveIndex());

        Vector3f p0 = load_3d(mesh->vertex_positions, face.x()),
                 p1 = load_3d(mesh->vertex_positions, face.y()),
                 p2 = load_3d(mesh->vertex_positions, face.z());

        Vector3f dp0 = p1 - p0,
                 dp1 = p2 - p0;

        Vector3f p = p0 * uv0 + p1 * uv1 + p2 * uv2;

        Vector3f ng = normalize(cross(dp0, dp1));
        Vector3f dp_du, dp_dv;
        coordinate_system(ng, dp_du, dp_dv);

        Vector3f ns;
        if (mesh->vertex_normals != nullptr) {
            Vector3f n0 =  load_3d(mesh->vertex_normals, face.x()),
                     n1 =  load_3d(mesh->vertex_normals, face.y()),
                     n2 =  load_3d(mesh->vertex_normals, face.z());

            ns = normalize(n0 * uv0 + n1 * uv1 + n2 * uv2);
        } else {
            ns = ng;
        }

        if (mesh->vertex_texcoords != nullptr) {
            Vector2f t0 = load_2d(mesh->vertex_texcoords, face.x()),
                     t1 = load_2d(mesh->vertex_texcoords, face.y()),
                     t2 = load_2d(mesh->vertex_texcoords, face.z());

            uv = t0 * uv0 + t1 * uv1 + t2 * uv2;

            Vector2f dt0 = t1 - t0,
                     dt1 = t2 - t0;
            float det = dt0.x() * dt1.y() - dt0.y() * dt1.x();

            if (det != 0.f) {
                float inv_det = 1.f / det;
                dp_du = ( dt1.y() * dp0 - dt0.y() * dp1) * inv_det;
                dp_dv = (-dt1.x() * dp0 + dt0.x() * dp1) * inv_det;
            }
        }

        Vector3f ray_o = make_vector3f(optixGetWorldRayOrigin());
        Vector3f ray_d = make_vector3f(optixGetWorldRayDirection());
        float t = sqrt(squared_norm(p - ray_o) / squared_norm(ray_d));

        write_output_params(params, launch_index,
                            sbt_data->shape_ptr,
                            optixGetPrimitiveIndex(),
                            p, uv, ns, ng, dp_du, dp_dv, t);
    }
}
#endif