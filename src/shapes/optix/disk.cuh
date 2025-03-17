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
        optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE,
                                __float_as_uint(local.x()),
                                __float_as_uint(local.y()));
}

extern "C" __global__ void __closesthit__disk() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
    Vector2f prim_uv = Vector2f(__uint_as_float(optixGetAttribute_0()),
                                __uint_as_float(optixGetAttribute_1()));
    set_preliminary_intersection_to_payload(optixGetRayTmax(), prim_uv, 0,
                                            sbt_data->shape_registry_id);
}
#endif
