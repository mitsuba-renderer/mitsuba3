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
    /// Shape id in Enoki's pointer registry
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

template <typename T>
struct OptixInputParam {
    const T *ptr;
    uint32_t width;

#ifdef __CUDACC__
    __device__ const T& operator[](int i) {
        return ptr[(width == 1 ? 0 : i)];
    }
#endif
};

/// Launch-varying data structure specifying data pointers for the input and output variables.
struct OptixParams {
    /// Input `active` mask data
    OptixInputParam<bool> in_mask;
    /// Input ray data
    OptixInputParam<float> in_o[3],
                           in_d[3],
                           in_mint,
                           in_maxt;
    /// Output preliminary intersection data pointers
    float   *out_t;
    float   *out_prim_uv[2];
    unsigned int *out_prim_index;
    unsigned int *out_shape_registry_id;
    unsigned int *out_inst_index;
    /// Output boolean data pointer for ray_test
    bool *out_hit;
    /// Handle for the acceleration data structure to trace against
    OptixTraversableHandle handle;

#ifdef __CUDACC__
    /// Return whether the current kernel is tracing test rays
    __device__ bool is_ray_test() { return out_hit; }
#endif

};

#ifdef __CUDACC__

/// Useful constants
constexpr float Pi       = float(3.14159265358979323846);
constexpr float TwoPi    = float(6.28318530717958647692);
constexpr float InvPi    = float(0.31830988618379067154);
constexpr float InvTwoPi = float(0.15915494309189533577);

/// Launch-varying parameters variable
extern "C" {
    __constant__ OptixParams params;
}

using namespace optix;

/// Write PreliminaryIntersection fields to the data pointers stored in the OptixParams
__device__ void
set_preliminary_intersection_to_payload(float t,
                                        const Vector2f &prim_uv,
                                        unsigned int prim_index,
                                        unsigned int shape_registry_id) {
    // Instance index is initialized to 0 when there is no instancing in the scene
    unsigned int out_inst_index = optixGetPayload_5();
    if (out_inst_index > 0) {
        // Check whether the current instance ID is a valid instance index
        unsigned int inst_index = optixGetInstanceId();
        if (inst_index < out_inst_index)
            out_inst_index = inst_index;
    }

    optixSetPayload_0(__float_as_int(t));
    optixSetPayload_1(__float_as_int(prim_uv[0]));
    optixSetPayload_2(__float_as_int(prim_uv[1]));
    optixSetPayload_3(prim_index);
    optixSetPayload_4(shape_registry_id);
    optixSetPayload_5(out_inst_index);
}

/// Returns a unique launch index per ray
__device__ unsigned int calculate_launch_index() {
    uint3 launch_dims = optixGetLaunchDimensions();
    uint3 launch_index3 = optixGetLaunchIndex();
    return launch_index3.x + (launch_index3.y + launch_index3.z * launch_dims.y) * launch_dims.x;
}
#endif