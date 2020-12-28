#pragma once

#include <mitsuba/render/optix/common.h>


#ifdef __CUDACC__

extern "C" __global__ void __closesthit__mesh() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
    unsigned int prim_index = optixGetPrimitiveIndex();

    float t = optixGetRayTmax();
    float2 float2_uv = optixGetTriangleBarycentrics();
    Vector2f prim_uv = Vector2f(float2_uv.x, float2_uv.y);

    set_preliminary_intersection_to_payload(
        t, prim_uv, prim_index, sbt_data->shape_registry_id);
}
#endif