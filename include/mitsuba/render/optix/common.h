#pragma once

#ifdef __CUDACC__
# include <optix.h>
#else
# include <mitsuba/render/optix_api.h>
#endif

#include <mitsuba/render/optix/bbox.cuh>
#include <mitsuba/render/optix/matrix.cuh>

/// Stores information about a Shape on the Optix side
struct OptixHitGroupData {
    /// Shape id in Dr.Jit's pointer registry
    uint32_t shape_registry_id;
    /// Pointer to the memory region of Shape data (e.g. \c OptixSphereData )
    void* data;
};

template <typename T>
struct alignas(OPTIX_SBT_RECORD_ALIGNMENT) SbtRecord {
    char header[OPTIX_SBT_RECORD_HEADER_SIZE];
    T data;
};

struct alignas(OPTIX_SBT_RECORD_ALIGNMENT) EmptySbtRecord {
    char header[OPTIX_SBT_RECORD_HEADER_SIZE];
};

using RayGenSbtRecord   = EmptySbtRecord;
using MissSbtRecord     = EmptySbtRecord;
using HitGroupSbtRecord = SbtRecord<OptixHitGroupData>;

#ifdef __CUDACC__

/// Useful constants
constexpr float Pi       = float(3.14159265358979323846);
constexpr float TwoPi    = float(6.28318530717958647692);
constexpr float InvPi    = float(0.31830988618379067154);
constexpr float InvTwoPi = float(0.15915494309189533577);

using namespace optix;

/// Write PreliminaryIntersection fields to the data pointers stored in the OptixParams
__device__ void
set_preliminary_intersection_to_payload(float t,
                                        const Vector2f &prim_uv,
                                        unsigned int prim_index,
                                        unsigned int shape_registry_id) {
    optixSetPayload_0(__float_as_int(t));
    optixSetPayload_1(__float_as_int(prim_uv[0]));
    optixSetPayload_2(__float_as_int(prim_uv[1]));
    optixSetPayload_3(prim_index);
    optixSetPayload_4(shape_registry_id);

    // Instance index is initialized to 0 when there is no instancing in the scene
    if (optixGetPayload_5() > 0)
        optixSetPayload_5(optixGetInstanceId());
}

#endif
