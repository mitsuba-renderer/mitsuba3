#include <math_constants.h>

#include <mitsuba/render/optix/matrix.cuh>
#include <mitsuba/render/optix/common.h>

// Include all shapes CUDA headers to generate their PTX programs
#include <mitsuba/render/optix/shapes.h>

extern "C" __global__ void __raygen__rg() {
    unsigned int launch_index = calculate_launch_index();

    Vector3f ro = Vector3f(params.in_o[0][launch_index],
                           params.in_o[1][launch_index],
                           params.in_o[2][launch_index]),
             rd = Vector3f(params.in_d[0][launch_index],
                           params.in_d[1][launch_index],
                           params.in_d[2][launch_index]);
    float mint = params.in_mint[launch_index],
          maxt = params.in_maxt[launch_index];

    if (params.is_ray_test()) {
        if (!params.in_mask[launch_index]) {
            params.out_hit[launch_index] = false;
        } else {
            optixTrace(
                params.handle,
                make_float3(ro), make_float3(rd),
                mint, maxt, 0.0f,
                OptixVisibilityMask( 1 ),
                OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT,
                0, 1, 0
                );
        }
    } else {
        if (!params.in_mask[launch_index]) {
            params.out_shape_ptr[launch_index] = 0;
            params.out_t[launch_index] = CUDART_INF_F;
        } else {
            optixTrace(
                params.handle,
                make_float3(ro), make_float3(rd),
                mint, maxt, 0.0f,
                OptixVisibilityMask( 1 ),
                OPTIX_RAY_FLAG_NONE,
                0, 1, 0
                );
        }
    }
}

extern "C" __global__ void __miss__ms() {
    unsigned int launch_index = calculate_launch_index();

    if (params.is_ray_test()) {
        params.out_hit[launch_index] = false;
    } else {
        params.out_shape_ptr[launch_index] = 0;
        params.out_t[launch_index] = CUDART_INF_F;
    }
}

struct OptixException {
    int code;
    const char* string;
};

__constant__ OptixException exceptions[] = {
    { OPTIX_EXCEPTION_CODE_STACK_OVERFLOW, "OPTIX_EXCEPTION_CODE_STACK_OVERFLOW" },
    { OPTIX_EXCEPTION_CODE_TRACE_DEPTH_EXCEEDED, "OPTIX_EXCEPTION_CODE_TRACE_DEPTH_EXCEEDED" },
    { OPTIX_EXCEPTION_CODE_TRAVERSAL_DEPTH_EXCEEDED, "OPTIX_EXCEPTION_CODE_TRAVERSAL_DEPTH_EXCEEDED" },
    { OPTIX_EXCEPTION_CODE_TRAVERSAL_INVALID_TRAVERSABLE, "OPTIX_EXCEPTION_CODE_TRAVERSAL_INVALID_TRAVERSABLE" },
    { OPTIX_EXCEPTION_CODE_TRAVERSAL_INVALID_MISS_SBT, "OPTIX_EXCEPTION_CODE_TRAVERSAL_INVALID_MISS_SBT" },
    { OPTIX_EXCEPTION_CODE_TRAVERSAL_INVALID_HIT_SBT, "OPTIX_EXCEPTION_CODE_TRAVERSAL_INVALID_HIT_SBT" }
};

extern "C" __global__ void __exception__err() {
    int ex_code = optixGetExceptionCode();
    printf("Optix Exception %u: %s\n", ex_code, exceptions[ex_code].string);
    // TODO: retreive more informations based on exception
}
