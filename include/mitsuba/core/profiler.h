/* Builtin sampling profiler based on that of PBRTv3 */

#pragma once

#include <mitsuba/core/object.h>

#if !defined(NDEBUG)
#  include <mitsuba/render/autodiff.h>
#endif

#if !defined(MTS_PROFILE_HASH_SIZE)
#  define MTS_PROFILE_HASH_SIZE 256
#endif

NAMESPACE_BEGIN(mitsuba)

/**
 * List of 'phases' that are handled by the profiler. Note that a partial order
 * is assumed -- if a method "B" can occur in a call graph of another method
 * "A", then "B" must occur after "A" in the list below.
 */
enum class EProfilerPhase : int {
    EInitScene = 0,              /* Scene initialization */
    ELoadGeometry,               /* Geometry loading */
    ELoadTexture,                /* Texture loading */
    EInitKDTree,                 /* kd-tree construction */
    ERender,                     /* Integrator::render() */

    ESamplingIntegratorEval,     /* SamplingIntegrator::eval() */
    ESamplingIntegratorEvalP,    /* SamplingIntegrator::eval [packet] () */
    ESampleEmitterDirection,     /* Scene::sample_emitter_direction() */
    ESampleEmitterDirectionP,    /* Scene::sample_emitter_direction() [packet] */
    ESampleEmitterRay,           /* Scene::sample_emitter_ray() */
    ESampleEmitterRayP,          /* Scene::sample_emitter_ray() [packet] */

    ERayTest,                    /* Scene::ray_test() */
    ERayTestP,                   /* Scene::ray_test() [packet] */
    ERayIntersect,               /* Scene::ray_intersect() */
    ERayIntersectP,              /* Scene::ray_intersect() [packet] */
    ECreateSurfaceInteraction,   /* KDTree::create_surface_interaction() */
    ECreateSurfaceInteractionP,  /* KDTree::create_surface_interaction() [packet] */
    EImageBlockPut,              /* ImageBlock::put() */
    EImageBlockPutP,             /* ImageBlock::put() [packet] */
    EBSDFEvaluate,               /* BSDF::eval() and BSDF::pdf() */
    EBSDFEvaluateP,              /* BSDF::eval() and BSDF::pdf() [packet] */
    EBSDFSample,                 /* BSDF::sample() */
    EBSDFSampleP,                /* BSDF::sample() [packet] */
    EEndpointEvaluate,           /* Endpoint::eval() and Endpoint::pdf() */
    EEndpointEvaluateP,          /* Endpoint::eval() and Endpoint::pdf() [packet] */
    EEndpointSampleRay,          /* Endpoint::sample_ray() */
    EEndpointSampleRayP,         /* Endpoint::sample_ray() [packet] */
    EEndpointSampleDirection,    /* Endpoint::sample_direction() */
    EEndpointSampleDirectionP,   /* Endpoint::sample_direction() [packet] */
    ESpectrumEval,               /* ContinuousSpectrum::eval() */
    ESpectrumEvalP,              /* ContinuousSpectrum::eval() [packet] */

    EProfilerPhaseCount
};

constexpr const char
    *profiler_phase_id[int(EProfilerPhase::EProfilerPhaseCount)] = {
        "Scene initialization",
        "Geometry loading",
        "Texture loading",
        "kd-tree construction",
        "Integrator::render()",
        "SamplingIntegrator::eval()",
        "SamplingIntegrator::eval() [packet]",
        "Scene::sample_emitter_direction()",
        "Scene::sample_emitter_direction() [packet]",
        "Scene::sample_emitter_ray()",
        "Scene::sample_emitter_ray() [packet]",
        "Scene::ray_test()",
        "Scene::ray_test() [packet]",
        "Scene::ray_intersect()",
        "Scene::ray_intersect() [packet]",
        "KDTree::create_surface_interaction()",
        "KDTree::create_surface_interaction() [packet]",
        "ImageBlock::put()",
        "ImageBlock::put() [packet]",
        "BSDF::eval(), pdf()",
        "BSDF::eval(), pdf() [packet]",
        "BSDF::sample()",
        "BSDF::sample() [packet]",
        "Endpoint::eval(), pdf()",
        "Endpoint::eval(), pdf() [packet]",
        "Endpoint::sample_ray()",
        "Endpoint::sample_ray() [packet]",
        "Endpoint::sample_direction()",
        "Endpoint::sample_direction() [packet]",
        "ContinuousSpectrum::eval()",
        "ContinuousSpectrum::eval() [packet]"
    };


static_assert(int(EProfilerPhase::EProfilerPhaseCount) <= 64,
              "List of profiler phases is limited to 64 entries");

static_assert(std::extent_v<decltype(profiler_phase_id)> ==
                  int(EProfilerPhase::EProfilerPhaseCount),
              "Profiler phases and descriptions don't have matching length!");

#if defined(MTS_ENABLE_PROFILER)
/* Inlining the access to a thread_local variable produces *awful* machine code
   with Clang on OSX. The combination of weak and noinline is needed to prevent
   the compiler from inlining it (just noinline does not seem to be enough). It
   is marked as 'const' because separate function calls always produce the same
   pointer. */
extern MTS_EXPORT_CORE uint64_t *profiler_flags()
    __attribute__((noinline, weak, const));

struct ScopedPhase {
    ScopedPhase(EProfilerPhase phase)
        : m_target(profiler_flags()), m_flag(1ull << int(phase)) {
        if ((*m_target & m_flag) == 0) {
            *m_target |= m_flag;

            #if !defined(NDEBUG) && defined(MTS_ENABLE_AUTODIFF)
                //FloatD::push_prefix_(profiler_phase_id[int(phase)]);
            #endif
        } else {
            m_flag = 0;
        }
    }

    ~ScopedPhase() {
        *m_target &= ~m_flag;
        #if !defined(NDEBUG) && defined(MTS_ENABLE_AUTODIFF)
            //if (m_flag != 0)
                //FloatD::pop_prefix_();
        #endif
    }

    ScopedPhase(const ScopedPhase &) = delete;
    ScopedPhase &operator=(const ScopedPhase &) = delete;

private:
    uint64_t* m_target;
    uint64_t  m_flag;
};

class MTS_EXPORT_CORE Profiler : public Object {
public:
    static void static_initialization();
    static void static_shutdown();
    static void print_report();

    MTS_DECLARE_CLASS()
private:
    Profiler() = delete;
};

#else

/* Profiler not supported on this platform */
struct ScopedPhase { ScopedPhase(EProfilerPhase) { } };
class Profiler {
public:
    static void static_initialization() { }
    static void static_shutdown() { }
    static void print_report() { }
};

#endif

NAMESPACE_END(mitsuba)
