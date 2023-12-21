#if defined(MI_ENABLE_CUDA)

#include <mitsuba/core/logger.h>

#include <drjit-core/optix.h>
#define OPTIX_API_IMPL
#include <mitsuba/render/optix_api.h>
#include <mitsuba/render/optix/shapes.h>

NAMESPACE_BEGIN(mitsuba)

void optix_initialize() {
    if (optixAccelBuild)
        return;

    jit_optix_context(); // Ensure OptiX is initialized

    #define L(name) name = (decltype(name)) jit_optix_lookup(#name);

    L(optixAccelComputeMemoryUsage);
    L(optixAccelBuild);
    L(optixAccelCompact);
    L(optixBuiltinISModuleGet);
    L(optixDenoiserCreate);
    L(optixDenoiserDestroy);
    L(optixDenoiserComputeMemoryResources);
    L(optixDenoiserSetup);
    L(optixDenoiserInvoke);
    L(optixDenoiserComputeIntensity);
    L(optixModuleCreateFromPTXWithTasks);
    L(optixModuleGetCompilationState);
    L(optixTaskExecute);
    L(optixProgramGroupCreate);
    L(optixSbtRecordPackHeader);

    #undef L
}

scoped_optix_context::scoped_optix_context() {
    jit_cuda_push_context(jit_cuda_context());
}

scoped_optix_context::~scoped_optix_context() {
    jit_cuda_pop_context();
}

NAMESPACE_END(mitsuba)

#endif // defined(MI_ENABLE_CUDA)
