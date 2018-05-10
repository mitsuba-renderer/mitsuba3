/* Builtin sampling profiler based on that of PBRTv3 */

#pragma once

#include <mitsuba/core/object.h>

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
    EInitScene,                  /* Scene initialization */
    ELoadGeometry,               /* Geometry loading */
    ELoadTexture,                /* Texture loading */
    EInitKDTree,                 /* kd-tree construction*/
    ERender,                     /* Integrator::render() */
    ESamplingIntegratorEval,     /* SamplingIntegrator::eval() */
    ESamplingIntegratorEvalP,    /* SamplingIntegrator::eval [packet] () */
    ESampleEmitterDirection,     /* Scene::sample_emitter_direction() */
    ESampleEmitterDirectionP,    /* Scene::sample_emitter_direction() [packet] */
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
    EProfilerPhaseCount
};

constexpr const char *profiler_phase_id[int(EProfilerPhase::EProfilerPhaseCount)] = {
    "Scene initialization",
    "Geometry loading",
    "Texture loading",
    "kd-tree construction",
    "Integrator::render()",
    "SamplingIntegrator::eval()",
    "SamplingIntegrator::eval() [packet]",
    "Scene::sample_emitter_direction()",
    "Scene::sample_emitter_direction() [packet]",
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
    "BSDF::sample() [packet]"
};

static_assert(int(EProfilerPhase::EProfilerPhaseCount) <= 64,
              "List of profiler phases is limited to 64 entries");

static_assert(std::extent<decltype(profiler_phase_id)>::value == int(EProfilerPhase::EProfilerPhaseCount),
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
    ScopedPhase(EProfilerPhase p)
        : m_target(profiler_flags()), m_flag(1ull << int(p)) {
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
