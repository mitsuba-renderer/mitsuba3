#include <math_constants.h>

#include <mitsuba/render/optix/matrix.cuh>
#include <mitsuba/render/optix/common.h>

// Include all shapes CUDA headers to generate their PTX programs
#include <mitsuba/render/optix/shapes.h>

extern "C" __global__ void __miss__ms() {
    if (optixGetRayFlags() == (OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT |
                               OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT)) { // ray_test
        optixSetPayload_0(0);
    } else {
        optixSetPayload_0(__float_as_int(CUDART_INF_F));
        optixSetPayload_4(0);
    }
}
