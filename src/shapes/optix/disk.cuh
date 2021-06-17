#pragma once

#include <math.h>
#include <mitsuba/render/optix/common.h>

struct OptixDiskData {
    optix::BoundingBox3f bbox;
    optix::Transform4f to_object;
};

#ifdef __CUDACC__
extern "C" __global__ void __intersection__disk() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData*) optixGetSbtDataPointer();
    OptixDiskData *disk = (OptixDiskData *)sbt_data->data;

    // Ray in instance-space
    Ray3f ray = get_ray();
    // Ray in object-space
    ray = disk->to_object.transform_ray(ray);

    float t = -ray.o.z() / ray.d.z();
    Vector3f local = ray(t);

    if (local.x() * local.x() + local.y() * local.y() <= 1.f)
        optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
}

extern "C" __global__ void __closesthit__disk() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
    OptixDiskData *disk = (OptixDiskData *)sbt_data->data;

    // Ray in instance-space
    Ray3f ray_ = get_ray();

    // Ray in object-space
    Ray3f ray = disk->to_object.transform_ray(ray_);

    float t = -ray.o.z() / ray.d.z();

    Vector3f local = ray(t);
    Vector2f prim_uv = Vector2f(local.x(), local.y());

    set_preliminary_intersection_to_payload(t, prim_uv, 0,
                                            sbt_data->shape_registry_id);
}
#endif