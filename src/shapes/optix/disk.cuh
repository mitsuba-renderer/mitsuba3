#pragma once

#include <math.h>
#include <mitsuba/render/optix/common.h>

struct OptixDiskData {
    optix::BoundingBox3f bbox;
    optix::Transform4f to_world;
    optix::Transform4f to_object;
};

#ifdef __CUDACC__
extern "C" __global__ void __intersection__disk() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData*) optixGetSbtDataPointer();
    OptixDiskData *disk = (OptixDiskData *)sbt_data->data;

    Vector3f ray_o = make_vector3f(optixGetWorldRayOrigin());
    Vector3f ray_d = make_vector3f(optixGetWorldRayDirection());

    ray_o = disk->to_object.transform_point(ray_o);
    ray_d = disk->to_object.transform_vector(ray_d);

    float t = -ray_o.z() / ray_d.z();
    Vector3f local = fmaf(t, ray_d, ray_o);

    if (local.x() * local.x() + local.y() * local.y() <= 1.f)
        optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
}

extern "C" __global__ void __closesthit__disk() {
    unsigned int launch_index = calculate_launch_index();

    if (params.is_ray_test()) {
        params.out_hit[launch_index] = true;
    } else {
        const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
        OptixDiskData *disk = (OptixDiskData *)sbt_data->data;

        // Ray in world-space
        Vector3f ray_o_ = make_vector3f(optixGetWorldRayOrigin());
        Vector3f ray_d_ = make_vector3f(optixGetWorldRayDirection());
        float t = optixGetRayTmax();

        // Ray in object-space
        Vector3f ray_o = disk->to_object.transform_point(ray_o_);
        Vector3f ray_d = disk->to_object.transform_vector(ray_d_);

        Vector3f local = fmaf(t, ray_d, ray_o);
        Vector2f prim_uv = Vector2f(local.x(), local.y());

        // Early return for ray_intersect_preliminary call
        if (params.is_ray_intersect_preliminary()) {
            write_output_pi_params(params, launch_index, sbt_data->shape_ptr, 0, prim_uv, t);
            return;
        }

        /* Compute and store information describing the intersection. This is
           very similar to Disk::compute_surface_interaction() */

        Vector3f p = ray_o_ + ray_d_ * t;

        Vector3f ns = normalize(disk->to_world.transform_normal(Vector3f(0.f, 0.f, 1.f)));
        Vector3f ng = ns;

        Vector2f uv;
        Vector3f dp_du, dp_dv;
        if (params.has_uv()) {
            float r = norm(prim_uv),
                  inv_r = 1.f / r;

            float v = atan2(local.y(), local.x()) * InvTwoPi;
            if (v < 0.f)
                v += 1.f;

            uv = Vector2f(r, v);

            if (params.has_dp_duv()) {
                float cos_phi = (r != 0.f ? local.x() * inv_r : 1.f),
                      sin_phi = (r != 0.f ? local.y() * inv_r : 0.f);

                dp_du = disk->to_world.transform_vector(Vector3f( cos_phi, sin_phi, 0.f));
                dp_dv = disk->to_world.transform_vector(Vector3f(-sin_phi, cos_phi, 0.f));
            }
        }

        write_output_si_params(params, launch_index, sbt_data->shape_ptr, 0, p,
                               uv, ns, ng, dp_du, dp_dv, Vector3f(0.f), Vector3f(0.f), t);
    }
}
#endif