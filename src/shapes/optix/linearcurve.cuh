#pragma once

#include <mitsuba/render/optix/common.h>

#ifdef __CUDACC__

extern "C" __global__ void __closesthit__linearcurve() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
    unsigned int prim_index = optixGetPrimitiveIndex();

    float t = optixGetRayTmax();
    float u = optixGetCurveParameter();

    set_preliminary_intersection_to_payload(
        t, Vector2f(u, 0), prim_index, sbt_data->shape_registry_id);
}
#endif
