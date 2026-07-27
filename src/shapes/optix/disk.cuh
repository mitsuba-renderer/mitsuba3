#pragma once

#include <math.h>
#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/shapedata.h>

#ifdef __CUDACC__
extern "C" __global__ void __intersection__disk() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData*) optixGetSbtDataPointer();
    shapedata::DiskData *disk = (shapedata::DiskData *)sbt_data->data;

    // Ray in instance-space
    Ray3f ray = get_ray();
    // Ray in object-space
    ray = apply_affine_ray(disk->to_object, ray);

    float t = -ray.o.z() / ray.d.z();
    Vector3f local = ray(t);

    if (local.x() * local.x() + local.y() * local.y() <= 1.f && t > ray.mint && t < ray.maxt)
        optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE,
                                __float_as_uint(local.x()),
                                __float_as_uint(local.y()));
}
#endif
