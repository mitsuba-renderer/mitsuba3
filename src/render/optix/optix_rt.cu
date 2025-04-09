#include <math_constants.h>

#include <mitsuba/render/optix/matrix.cuh>
#include <mitsuba/render/optix/common.h>

// Include all shapes CUDA headers to generate their PTX programs
#include <mitsuba/render/optix/shapes.h>

extern "C" __global__ void __miss__ms() {
    optixSetPayload_0(__float_as_int(CUDART_INF_F));
    optixSetPayload_4(0);
}
