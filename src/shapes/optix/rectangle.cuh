#pragma once

#include <math.h>
#include <mitsuba/render/optix/common.h>

struct OptixRectangleData {
    optix::BoundingBox3f bbox;
    optix::Transform4f to_object;
    optix::Vector3f ns;
    optix::Vector3f dp_du;
    optix::Vector3f dp_dv;
};

#ifdef __CUDACC__
extern "C" __global__ void __intersection__rectangle() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData*) optixGetSbtDataPointer();
    OptixRectangleData *rect = (OptixRectangleData *)sbt_data->data;

    Vector3f ray_o = make_vector3f(optixGetWorldRayOrigin());
    Vector3f ray_d = make_vector3f(optixGetWorldRayDirection());

    ray_o = rect->to_object.transform_point(ray_o);
    ray_d = rect->to_object.transform_vector(ray_d);

    float t = -ray_o.z() / ray_d.z();
    Vector3f local = fmaf(t, ray_d, ray_o);

    if (abs(local.x()) <= 1.f && abs(local.y()) <= 1.f)
        optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
}

extern "C" __global__ void __closesthit__rectangle() {
    unsigned int launch_index = calculate_launch_index();

    if (params.out_hit != nullptr) {
        params.out_hit[launch_index] = true;
    } else {
        const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
        OptixRectangleData *rect = (OptixRectangleData *)sbt_data->data;

        /* Compute and store information describing the intersection. This is
           very similar to Rectangle::fill_surface_interaction() */

        Vector3f ns    = rect->ns;
        Vector3f dp_du = rect->dp_du;
        Vector3f dp_dv = rect->dp_dv;
        Vector3f ng = ns;

        // Ray in world-space
        Vector3f ray_o_ = make_vector3f(optixGetWorldRayOrigin());
        Vector3f ray_d_ = make_vector3f(optixGetWorldRayDirection());

        // Ray in object-space
        Vector3f ray_o = rect->to_object.transform_point(ray_o_);
        Vector3f ray_d = rect->to_object.transform_vector(ray_d_);

        float t = -ray_o.z() / ray_d.z();
        Vector3f p = ray_o_ + ray_d_ * t;

        Vector3f local = fmaf(t, ray_d, ray_o);
        Vector2f uv = 0.5f * Vector2f(local.x(), local.y()) + 0.5f;

        write_output_params(params, launch_index,
                            sbt_data->shape_ptr,
                            optixGetPrimitiveIndex(),
                            p, uv, ns, ng, dp_du, dp_dv, t);
    }
}
#endif