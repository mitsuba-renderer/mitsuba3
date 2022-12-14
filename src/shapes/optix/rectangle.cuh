#pragma once

#include <math.h>
#include <mitsuba/render/optix/common.h>

struct OptixRectangleData {
    optix::BoundingBox3f bbox;
    optix::Transform4f to_object;
};

#ifdef __CUDACC__
extern "C" __global__ void __intersection__rectangle() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData*) optixGetSbtDataPointer();
    OptixRectangleData *rect = (OptixRectangleData *)sbt_data->data;

    // Ray in instance-space
    Ray3f ray = get_ray();
    // Ray in object-space
    ray = rect->to_object.transform_ray(ray);

    float t = -ray.o.z() / ray.d.z();
    Vector3f local = ray(t);

    if (abs(local.x()) <= 1.f && abs(local.y()) <= 1.f)
        optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
}

extern "C" __global__ void __closesthit__rectangle() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
    OptixRectangleData *rect = (OptixRectangleData *)sbt_data->data;

    // Ray in instance-space
    Ray3f ray_ = get_ray();

    // Ray in object-space
    Ray3f ray = rect->to_object.transform_ray(ray_);

    float t = -ray.o.z() / ray.d.z();
    Vector3f local = ray(t);
    Vector2f prim_uv = Vector2f(local.x(), local.y());

    set_preliminary_intersection_to_payload(t, prim_uv, 0,
                                            sbt_data->shape_registry_id);
}
#endif
