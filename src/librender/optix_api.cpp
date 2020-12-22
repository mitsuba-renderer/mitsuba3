#if defined(MTS_ENABLE_CUDA)

#include <mitsuba/core/logger.h>

#include <enoki-jit/optix.h>
#define OPTIX_API_IMPL
#include <mitsuba/render/optix_api.h>
#include <mitsuba/render/optix/shapes.h>

NAMESPACE_BEGIN(mitsuba)

void optix_initialize() {
    if (optixAccelBuild)
        return;

    jitc_optix_context(); // Ensure OptiX is initialized

    #define L(name) name = (decltype(name)) jitc_optix_lookup(#name);

    L(optixAccelComputeMemoryUsage);
    L(optixAccelBuild);
    L(optixAccelCompact);
    L(optixModuleCreateFromPTX);
    L(optixModuleDestroy)
    L(optixProgramGroupCreate);
    L(optixProgramGroupDestroy)
    L(optixSbtRecordPackHeader);

    #undef L
}

NAMESPACE_END(mitsuba)

#endif // defined(MTS_ENABLE_CUDA)
