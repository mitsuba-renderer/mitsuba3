#if defined(MTS_ENABLE_CUDA)

#include <mitsuba/core/logger.h>

#include <enoki-jit/optix.h>
#define OPTIX_API_IMPL
#include <mitsuba/render/optix_api.h>
#include <mitsuba/render/optix/shapes.h>

static bool optix_init_attempted = false;
static bool optix_init_success = false;

NAMESPACE_BEGIN(mitsuba)

bool optix_initialize() {
    if (optix_init_attempted)
        return optix_init_success;
    optix_init_attempted = true;

    Log(LogLevel::Trace, "Dynamic loading of the Optix library ..");

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
    optix_init_success = true;
    return true;
}

void optix_shutdown() {
    if (!optix_init_success)
        return;

    #define Z(x) x = nullptr

    Z(optixAccelComputeMemoryUsage);
    Z(optixAccelBuild);
    Z(optixAccelCompact);
    Z(optixModuleCreateFromPTX);
    Z(optixModuleDestroy);
    Z(optixProgramGroupCreate);
    Z(optixProgramGroupDestroy);
    Z(optixSbtRecordPackHeader);

    #undef Z

    optix_init_success = false;
    optix_init_attempted = false;
}

NAMESPACE_END(mitsuba)

#endif // defined(MTS_ENABLE_CUDA)
