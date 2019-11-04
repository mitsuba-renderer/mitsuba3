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
    ESampleEmitterRay,           /* Scene::sample_emitter_ray() */
    ESampleEmitterDirection,     /* Scene::sample_emitter_direction() */
    ERayTest,                    /* Scene::ray_test() */
    ERayIntersect,               /* Scene::ray_intersect() */
    ECreateSurfaceInteraction,   /* KDTree::create_surface_interaction() */
    EImageBlockPut,              /* ImageBlock::put() */
    EBSDFEvaluate,               /* BSDF::eval() and BSDF::pdf() */
    EBSDFSample,                 /* BSDF::sample() */
    EEndpointEvaluate,           /* Endpoint::eval() and Endpoint::pdf() */
    EEndpointSampleRay,          /* Endpoint::sample_ray() */
    EEndpointSampleDirection,    /* Endpoint::sample_direction() */
    ESpectrumEval,               /* ContinuousSpectrum::eval() */

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
        "Scene::sample_emitter_ray()",
        "Scene::sample_emitter_direction()",
        "Scene::ray_test()",
        "Scene::ray_intersect()",
        "KDTree::create_surface_interaction()",
        "ImageBlock::put()",
        "BSDF::eval(), pdf()",
        "BSDF::sample()",
        "Endpoint::eval(), pdf()",
        "Endpoint::sample_ray()",
        "Endpoint::sample_direction()",
        "ContinuousSpectrum::eval()"
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
        if ((*m_target & m_flag) == 0)
            *m_target |= m_flag;
        else
            m_flag = 0;
    }

    ~ScopedPhase() {
        *m_target &= ~m_flag;
    }

    ScopedPhase(const ScopedPhase &) = delete;
    ScopedPhase &operator=(const ScopedPhase &) = delete;

private:
    uint64_t* m_target;
    uint64_t  m_flag;
};

class MTS_EXPORT_CORE Profiler : public Object {
public:
    MTS_DECLARE_CLASS(Profiler, Object)

    static void static_initialization();
    static void static_shutdown();
    static void print_report();
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
