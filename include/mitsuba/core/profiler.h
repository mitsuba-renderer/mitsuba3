#pragma once

#include <mitsuba/core/object.h>

#if defined(MI_ENABLE_ITTNOTIFY)
#  include <ittnotify.h>
#endif

#if defined(MI_ENABLE_NVTX)
#  include <nvtx3/nvToolsExt.h>
#endif

NAMESPACE_BEGIN(mitsuba)

/**
 * List of 'phases' that are handled by the profiler. Note that a partial order
 * is assumed -- if a method "B" can occur in a call graph of another method
 * "A", then "B" must occur after "A" in the list below.
 */
enum class ProfilerPhase : int {
    InitScene = 0,              /* Scene initialization */
    LoadGeometry,               /* Geometry loading */
    BitmapRead,                 /* Bitmap loading */
    BitmapWrite,                /* Bitmap writing */
    InitAccel,                  /* Acceleration data structure creation */
    Render,                     /* Integrator::render() */
    SamplingIntegratorSample,   /* SamplingIntegrator::sample() */
    SampleEmitter,              /* Scene::sample_emitter() */
    SampleEmitterRay,           /* Scene::sample_emitter_ray() */
    SampleEmitterDirection,     /* Scene::sample_emitter_direction() */
    RayTest,                    /* Scene::ray_test() */
    RayIntersect,               /* Scene::ray_intersect() */
    CreateSurfaceInteraction,   /* KDTree::create_surface_interaction() */
    ImageBlockPut,              /* ImageBlock::put() */
    BSDFEvaluate,               /* BSDF::eval() and BSDF::pdf() */
    BSDFSample,                 /* BSDF::sample() */
    PhaseFunctionEvaluate,      /* PhaseFunction::eval() and PhaseFunction::pdf() */
    PhaseFunctionSample,        /* PhaseFunction::sample() */
    MediumEvaluate,             /* Medium::eval() and Medium::pdf() */
    MediumSample,               /* Medium::sample() */
    EndpointEvaluate,           /* Endpoint::eval() and Endpoint::pdf() */
    EndpointSampleRay,          /* Endpoint::sample_ray() */
    EndpointSampleDirection,    /* Endpoint::sample_direction() */
    EndpointSamplePosition,     /* Endpoint::sample_position() */
    TextureSample,              /* Texture::sample() */
    TextureEvaluate,            /* Texture::eval() and Texture::pdf() */

    ProfilerPhaseCount
};

constexpr const char
    *profiler_phase_id[int(ProfilerPhase::ProfilerPhaseCount)] = {
        "Scene initialization",
        "Geometry loading",
        "Bitmap loading",
        "Bitmap writing",
        "Acceleration data structure creation",
        "Integrator::render()",
        "SamplingIntegrator::sample()",
        "Scene::sample_emitter()",
        "Scene::sample_emitter_ray()",
        "Scene::sample_emitter_direction()",
        "Scene::ray_test()",
        "Scene::ray_intersect()",
        "KDTree::create_surface_interaction()",
        "ImageBlock::put()",
        "BSDF::eval(), pdf()",
        "BSDF::sample()",
        "PhaseFunction::eval(), pdf()",
        "PhaseFunction::sample()",
        "Medium::eval(), pdf()",
        "Medium::sample()",
        "Endpoint::eval(), pdf()",
        "Endpoint::sample_ray()",
        "Endpoint::sample_direction()",
        "Endpoint::sample_position()",
        "Texture::sample()",
        "Texture::eval()"
    };

#if defined(MI_ENABLE_ITTNOTIFY)
extern MI_EXPORT_LIB __itt_domain *mitsuba_itt_domain;
extern MI_EXPORT_LIB __itt_string_handle *
    mitsuba_itt_phase[int(ProfilerPhase::ProfilerPhaseCount)];
#endif

struct ScopedPhase {
    ScopedPhase(ProfilerPhase phase) {
        /// Interface with various external visual profilers
#if defined(MI_ENABLE_ITTNOTIFY)
        __itt_task_begin(mitsuba_itt_domain, __itt_null, __itt_null,
                         mitsuba_itt_phase[(int) phase]);
#endif

#if defined(MI_ENABLE_NVTX)
        nvtxRangePush(profiler_phase_id[(int) phase]);
#endif
        (void) phase;
    }

    ~ScopedPhase() {
#if defined(MI_ENABLE_ITTNOTIFY)
        __itt_task_end(mitsuba_itt_domain);
#endif

#if defined(MI_ENABLE_NVTX)
        nvtxRangePop();
#endif
    }

    ScopedPhase(const ScopedPhase &) = delete;
    ScopedPhase &operator=(const ScopedPhase &) = delete;
};

class MI_EXPORT_LIB Profiler {
public:
    static void static_initialization();
    static void static_shutdown();
};

NAMESPACE_END(mitsuba)
