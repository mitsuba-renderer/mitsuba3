#include <random>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class PathIntegrator : public MonteCarloIntegrator<Float, Spectrum> {
public:
    MI_IMPORT_BASE(MonteCarloIntegrator, m_max_depth, m_rr_depth)
    MI_IMPORT_TYPES(Scene, Sampler, Medium, Emitter, EmitterPtr, BSDF, BSDFPtr)

    PathIntegrator(const Properties &props) : Base(props) { }

    std::pair<Spectrum, Bool> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const RayDifferential3f &ray_,
                                     const Medium * /* medium */,
                                     Float * /* aovs */,
                                     Bool active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        if (unlikely(m_max_depth == 0))
            return { 0.f, false };

        // Tracks radiance scaling due to index of refraction changes
        Float eta(1.f);
        Ray3f ray(ray_);
        Spectrum result(0.f), throughput(1.f);
        BSDFContext ctx;
        UInt32 depth = 1;

        // ---------------------- First intersection ----------------------

        SurfaceInteraction3f si = scene->ray_intersect(
            ray, +RayFlags::All, /* coherent = */ true, active);

        // Used to compute the alpha channel of the image
        Bool valid_ray = active && si.is_valid();

        // Account for directly visible emitter
        EmitterPtr emitter = si.emitter(scene);
        if (dr::any_or<true>(dr::neq(emitter, nullptr)))
            result = emitter->eval(si, active);

        active &= si.is_valid() && depth < m_max_depth;

        /* Set up a Dr.Jit loop (optimizes away to a normal loop in scalar mode,
           generates wavefront or megakernel renderer based on configuration).
           Register everything that changes as part of the loop here */
        dr::Loop<Bool> loop("PathIntegrator",
                            /* loop state: */ active, depth, ray,
                            throughput, result, si, eta, sampler);

        while (loop(active)) {
            // --------------------- Emitter sampling ---------------------

            BSDFPtr bsdf = si.bsdf(ray);
            Bool active_e = active && has_flag(bsdf->flags(), BSDFFlags::Smooth);

            if (likely(dr::any_or<true>(active_e))) {
                auto [ds, emitter_val] = scene->sample_emitter_direction(
                    si, sampler->next_2d(active_e), true, active_e);
                active_e &= dr::neq(ds.pdf, 0.f);

                // Query the BSDF for that emitter-sampled direction
                Vector3f wo = si.to_local(ds.d);

                // Determine BSDF value and density of sampling that direction using BSDF sampling
                auto [bsdf_val, bsdf_pdf] = bsdf->eval_pdf(ctx, si, wo, active_e);
                bsdf_val = si.to_world_mueller(bsdf_val, -wo, si.wi);

                Float mis = dr::select(ds.delta, 1.f, mis_weight(ds.pdf, bsdf_pdf));

                if constexpr (is_polarized_v<Spectrum>)
                    result[active_e] += mis * throughput * bsdf_val * emitter_val;
                else
                    result[active_e] = dr::fmadd(mis * bsdf_val * emitter_val, throughput, result);
            }

            // ----------------------- BSDF sampling ----------------------

            // Sample BSDF * dr::cos(theta)
            auto [bs, bsdf_val] = bsdf->sample(ctx, si, sampler->next_1d(active),
                                               sampler->next_2d(active), active);
            bsdf_val = si.to_world_mueller(bsdf_val, -bs.wo, si.wi);

            throughput = throughput * bsdf_val;
            active &= dr::any(dr::neq(unpolarized_spectrum(throughput), 0.f));
            if (dr::none_or<false>(active))
                break;

            eta *= bs.eta;

            // Intersect the BSDF ray against the scene geometry
            ray = si.spawn_ray(si.to_world(bs.wo));
            SurfaceInteraction3f si_bsdf = scene->ray_intersect(ray, active);

            DirectionSample3f ds(scene, si_bsdf, si);

            // Did we happen to hit an emitter?
            if (dr::any_or<true>(dr::neq(ds.emitter, nullptr))) {
                Bool delta = has_flag(bs.sampled_type, BSDFFlags::Delta);

                /* If so, determine probability of having sampled that same
                   direction using the emitter sampling. */
                Float emitter_pdf =
                    dr::select(delta, 0.f, scene->pdf_emitter_direction(si, ds, active));

                Float mis = mis_weight(bs.pdf, emitter_pdf);
                EmitterPtr emitter_b = si_bsdf.emitter(scene, active);
                if (dr::any_or<true>(dr::neq(emitter_b, nullptr))) {
                    Spectrum emitter_val = emitter_b->eval(si_bsdf, active);

                    if constexpr (is_polarized_v<Spectrum>)
                        result[active] += mis * throughput * emitter_val;
                    else
                        result[active] = dr::fmadd(mis * emitter_val, throughput, result);
                }
            }

            si = std::move(si_bsdf);
            depth++;

            active &= si.is_valid() && depth < m_max_depth;

            /* Russian roulette: try to keep path weights equal to one,
               while accounting for the solid angle compression at refractive
               index boundaries. Stop with at least some probability to avoid
               getting stuck (e.g. due to total internal reflection) */
            Bool use_rr = depth > m_rr_depth;
            if (dr::any_or<true>(use_rr)) {
                Float q = dr::minimum(dr::max(unpolarized_spectrum(throughput)) * dr::sqr(eta), .95f);
                dr::masked(active, use_rr) &= sampler->next_1d(active) < q;
                dr::masked(throughput, use_rr) *= dr::detach(dr::rcp(q));
            }
        }

        return { result, valid_ray };
    }

    //! @}
    // =============================================================

    std::string to_string() const override {
        return tfm::format("PathIntegrator[\n"
            "  max_depth = %u,\n"
            "  rr_depth = %u\n"
            "]", m_max_depth, m_rr_depth);
    }

    Float mis_weight(Float pdf_a, Float pdf_b) const {
        pdf_a *= pdf_a;
        pdf_b *= pdf_b;
        Float w = pdf_a / (pdf_a + pdf_b);
        return dr::select(dr::isfinite(w), w, 0.f);
    }

    Spectrum spec_fma(const Spectrum &a, const Spectrum &b,
                      const Spectrum &c) const {
        if constexpr (is_polarized_v<Spectrum>)
            return a * b + c; // Mueller matrix multiplication
        else
            return dr::fmadd(a, b, c);
    }

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(PathIntegrator, MonteCarloIntegrator)
MI_EXPORT_PLUGIN(PathIntegrator, "Path Tracer integrator");
NAMESPACE_END(mitsuba)
