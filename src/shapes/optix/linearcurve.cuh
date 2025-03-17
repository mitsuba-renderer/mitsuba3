#pragma once

#include <mitsuba/render/optix/common.h>

#ifdef __CUDACC__

extern "C" __global__ void __closesthit__linearcurve() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
    unsigned int prim_index = optixGetPrimitiveIndex();
    Vector2f prim_uv = Vector2f(__uint_as_float(optixGetAttribute_0()), 0);
    set_preliminary_intersection_to_payload(
        optixGetRayTmax(), prim_uv, prim_index, sbt_data->shape_registry_id);
}
#endif
