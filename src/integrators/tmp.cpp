#include <mitsuba/core/ray.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class TmpIntegrator : public MonteCarloIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(MonteCarloIntegrator, m_max_depth, m_rr_depth, m_hide_emitters)
    MI_IMPORT_TYPES(Scene, Sampler, Medium, Emitter, EmitterPtr, BSDF, BSDFPtr)

    TmpIntegrator(const Properties &props) : Base(props) { }

    std::pair<Spectrum, Bool> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const RayDifferential3f &ray_,
                                     const Medium * /* medium */,
                                     Float * /* aovs */,
                                     Bool active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        if (unlikely(m_max_depth == 0))
            return { 0.f, false };

        // --------------------- Configure loop state ----------------------

        Ray3f ray                     = Ray3f(ray_);
        Spectrum throughput           = 1.f;
        Spectrum result               = 0.f;
        UInt32 depth                  = 0;

        // Variables caching information from the previous bounce
        Interaction3f prev_si         = dr::zeros<Interaction3f>();
        BSDFContext   bsdf_ctx;

        /* Set up a Dr.Jit loop. This optimizes away to a normal loop in scalar
           mode, and it generates either a a megakernel (default) or
           wavefront-style renderer in JIT variants. This can be controlled by
           passing the '-W' command line flag to the mitsuba binary or
           enabling/disabling the JitFlag.LoopRecord bit in Dr.Jit.
        */
        struct LoopState {
            Ray3f ray;
            SurfaceInteraction3f si;
            UInt32 depth;
            Interaction3f prev_si;
            Bool active;
            Sampler* sampler;

            DRJIT_STRUCT(LoopState, si, depth, prev_si, active, sampler)
        } ls = {
            ray,
            dr::zeros<SurfaceInteraction3f>(),
            depth,
            prev_si,
            active,
            sampler
        };

        ls.si = scene->ray_intersect(Ray3f(ray_), /* ray_flags = */ +RayFlags::All, true);

        dr::tie(ls) = dr::while_loop(
            dr::make_tuple(ls), [](const LoopState &ls) { return ls.active; },
            [this, scene, bsdf_ctx](LoopState &ls) {

                ls.prev_si = Interaction3f(ls.si);
                dr::set_label(ls.prev_si, "Prev Si inside");

                Float sample_1 = ls.sampler->next_1d();
                Point2f sample_2 = ls.sampler->next_2d();
                BSDFPtr bsdf     = ls.si.bsdf(ls.ray);
                auto [bsdf_sample, bsdf_weight] =
                    bsdf->sample(bsdf_ctx, ls.si, sample_1, sample_2);

                ls.ray = ls.si.spawn_ray(ls.si.to_world(bsdf_sample.wo));

                ls.depth += 1;

                ls.active = ls.depth < m_max_depth && ls.si.is_valid();

                ls.si = scene->ray_intersect(ls.ray,
                                             /* ray_flags = */ +RayFlags::All,
                                             /* coherent = */ ls.depth == 0u,
                                             ls.active);
        });

        std::cout << "tmp prev_si: " << ls.prev_si.p << std::endl;
        std::cout << "tmp si: " << ls.si.p << std::endl;

        if constexpr (is_rgb_v<Spectrum>)
            return {
                /* spec  = */ ls.prev_si.p,
                /* valid = */ true,
            };
        else 
            return { 0, true };
    }

    //! @}
    // =============================================================

    std::string to_string() const override {
        return tfm::format("PathIntegrator[\n"
            "  max_depth = %u,\n"
            "  rr_depth = %u\n"
            "]", m_max_depth, m_rr_depth);
    }

    /// Compute a multiple importance sampling weight using the power heuristic
    Float mis_weight(Float pdf_a, Float pdf_b) const {
        pdf_a *= pdf_a;
        pdf_b *= pdf_b;
        Float w = pdf_a / (pdf_a + pdf_b);
        return dr::detach<true>(dr::select(dr::isfinite(w), w, 0.f));
    }

    /**
     * \brief Perform a Mueller matrix multiplication in polarized modes, and a
     * fused multiply-add otherwise.
     */
    Spectrum spec_fma(const Spectrum &a, const Spectrum &b,
                      const Spectrum &c) const {
        if constexpr (is_polarized_v<Spectrum>)
            return a * b + c;
        else
            return dr::fmadd(a, b, c);
    }

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(TmpIntegrator, MonteCarloIntegrator)
MI_EXPORT_PLUGIN(TmpIntegrator, "Path Tracer integrator");
NAMESPACE_END(mitsuba)
