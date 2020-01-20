#include <random>
#include <enoki/stl.h>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>


NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class VolumetricPathIntegrator : public MonteCarloIntegrator<Float, Spectrum> {

public:
    MTS_IMPORT_BASE(MonteCarloIntegrator, m_max_depth, m_rr_depth, m_hide_emitters)
    MTS_IMPORT_TYPES(Scene, Sampler, Emitter, EmitterPtr, BSDF, BSDFPtr,
                     Medium, MediumPtr, PhaseFunctionContext)

    VolumetricPathIntegrator(const Properties &props) : Base(props) {}

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const RayDifferential3f &ray_,
                                     Float * /* aovs */,
                                     Mask active) const override {


        // If there is an environment emitter and emitters are visible: all rays will be valid
        // Otherwise, it will depend on whether a valid interaction is sampled
        Mask valid_ray = !m_hide_emitters && neq(scene->environment(), nullptr);

        // For now, don't use ray differentials
        Ray3f ray = ray_;

        // Tracks radiance scaling due to index of refraction changes
        Float eta(1.f);

        Spectrum throughput(1.f), result(0.f);

        MediumPtr medium = nullptr;
        MediumInteraction3f mi;

        Mask specular_chain = active && !m_hide_emitters;
        for (int depth = 0;; ++depth) {
            // ----------------- Handle termination of paths ------------------

            // Russian roulette: try to keep path weights equal to one, while accounting for the
            // solid angle compression at refractive index boundaries. Stop with at least some
            // probability to avoid  getting stuck (e.g. due to total internal reflection)

            active &= any(neq(depolarize(throughput), 0.f));
            if (depth > m_rr_depth) {
                Float q = min(hmax(depolarize(throughput)) * sqr(eta), .95f);
                active &= sampler->next_1d(active) < q;
                throughput *= rcp(detach(q));
            }

            if (none(active) || (uint32_t) depth >= (uint32_t) m_max_depth)
                break;

            // ----------------------- Sampling the RTE -----------------------
            Mask active_medium  = active && neq(medium, nullptr);
            Mask active_surface = active && !active_medium;
            Mask escaped_medium = false;
            SurfaceInteraction3f si_medium;
            if (any_or<true>(active_medium)) {
                Spectrum medium_throughput;
                std::tie(si_medium, mi, medium_throughput) =
                    medium->sample_distance(scene, ray, sampler->next_2d(active_medium),
                                            sampler, active_medium);
                escaped_medium = active_medium && si_medium.is_valid() && !mi.is_valid();
                masked(throughput, active_medium) *= medium_throughput;
                active_medium &= mi.is_valid();
            }

            if (any_or<true>(active_medium)) {
                PhaseFunctionContext phase_ctx(sampler);

                auto phase = mi.medium->phase_function();
                masked(phase, !active_medium) = nullptr;
                auto [wo, phase_pdf] = phase->sample(phase_ctx, mi, sampler->next_2d(active_medium), active_medium);
                Ray3f new_ray  = mi.spawn_ray(mi.to_world(wo));
                new_ray.mint = 0.0f;
                new_ray.maxt = math::Infinity<ScalarFloat>;

                masked(ray, active_medium) = new_ray;

                Mask sample_emitters = mi.medium->use_emitter_sampling();

                valid_ray |= active_medium;
                specular_chain &= !active_medium;
                specular_chain |= active_medium && !sample_emitters;

                Mask active_e = active_medium && sample_emitters;
                if (any_or<true>(active_e)) {
                    auto [ds, emitter_val] = scene->sample_emitter_direction_attenuated(
                        mi, true, medium, sampler->next_2d(active_e), sampler, true, active_e);
                    active_e &= neq(ds.pdf, 0.f);
                    Float phase_val =
                        mi.medium->phase_function()->eval(phase_ctx, mi, ds.d, active_e);
                    masked(result, active_e) += throughput * phase_val * emitter_val;
                }
            }

            // --------------------- Surface Interactions ---------------------
            SurfaceInteraction3f si     = scene->ray_intersect(ray, active_surface);
            masked(si, escaped_medium) = si_medium;
            active_surface |= escaped_medium;

            if (any_or<true>(active_surface)) {
                // ---------------- Intersection with emitters ----------------
                EmitterPtr emitter = si.emitter(scene);
                Mask use_emitter_contribution =
                    active_surface && specular_chain && neq(emitter, nullptr);
                if (any_or<true>(use_emitter_contribution))
                    masked(result, use_emitter_contribution) +=
                        throughput * emitter->eval(si, use_emitter_contribution);
            }
            active_surface &= si.is_valid();
            if (any_or<true>(active_surface)) {
                // --------------------- Emitter sampling ---------------------
                BSDFContext ctx;
                BSDFPtr bsdf  = si.bsdf(ray);
                Mask active_e = active_surface && has_flag(bsdf->flags(), BSDFFlags::Smooth);
                if (likely(any_or<true>(active_e))) {
                    auto [ds, emitter_val] = scene->sample_emitter_direction_attenuated(
                        si, false, medium, sampler->next_2d(active_e), sampler, true, active_e);
                    active_e &= neq(ds.pdf, 0.f);

                    // Query the BSDF for that emitter-sampled direction
                    Vector3f wo       = si.to_local(ds.d);
                    Spectrum bsdf_val = bsdf->eval(ctx, si, wo, active_e);

                    // Determine probability of having sampled that same
                    // direction using BSDF sampling.
                    Float bsdf_pdf = bsdf->pdf(ctx, si, wo, active_e);
                    result[active_e] +=
                        throughput * emitter_val * bsdf_val * mis_weight(ds.pdf, bsdf_pdf);
                }

                // ----------------------- BSDF sampling ----------------------
                auto [bs, bsdf_val] = bsdf->sample(ctx, si, sampler->next_1d(active_surface),
                                                   sampler->next_2d(active_surface), active_surface);
                throughput[active_surface] *= bsdf_val;
                masked(eta, active_surface) *= bs.eta;

                Ray bsdf_ray                = si.spawn_ray(si.to_world(bs.wo));
                masked(ray, active_surface) = bsdf_ray;

                Mask non_null_bsdf = active_surface && !has_flag(bs.sampled_type, BSDFFlags::Null);
                valid_ray |= non_null_bsdf;
                specular_chain |= non_null_bsdf && has_flag(bs.sampled_type, BSDFFlags::Delta);
                specular_chain &= !(active_surface && has_flag(bs.sampled_type, BSDFFlags::Smooth));

                Mask add_emitter = active_surface && !has_flag(bs.sampled_type, BSDFFlags::Delta) &&
                                   any(neq(depolarize(throughput), 0.f));

                uint32_t max_intersections = (uint32_t) m_max_depth - (uint32_t) depth - 1;
                auto [emitted, emitter_pdf] =
                    ray_intersect_and_look_for_emitter(si, scene, sampler, medium, ray,
                                                       max_intersections, add_emitter);
                result += select(add_emitter && neq(emitter_pdf, 0),
                                 mis_weight(bs.pdf, emitter_pdf) * throughput * emitted, 0.0f);

                Mask has_medium_trans            = si.is_valid() && si.is_medium_transition();
                masked(medium, has_medium_trans) = si.target_medium(ray.d);
            }
            active &= (active_surface | active_medium);
        }
        return { result, valid_ray };
    }

    std::pair<Spectrum, Float>
    ray_intersect_and_look_for_emitter(const SurfaceInteraction3f &si_ref, const Scene *scene,
                                       Sampler *sampler, MediumPtr medium, RayDifferential3f ray,
                                       uint32_t maxInteractions, Mask active) const {
        using EmitterPtr = replace_scalar_t<Float, const Emitter *>;
        Spectrum value(0.0f);
        Spectrum transmittance(1.0f);
        Float emitter_pdf(0.0f);
        uint32_t interactions = 0;
        EmitterPtr emitter;
        while (any(active) && interactions < maxInteractions) {

            // Intersect the value ray with the scene
            SurfaceInteraction3f si = scene->ray_intersect(ray, active);

            // If intersection is found: Is it a null bsdf or an occlusion?
            Mask active_surface = active && si.is_valid();
            // Check if we hit an emitter and add illumination if needed
            emitter          = si.emitter(scene, active);
            Mask emitter_hit = neq(emitter, nullptr) && active;
            if (any_or<true>(emitter_hit)) {
                value[emitter_hit] += transmittance * emitter->eval(si, emitter_hit);
                DirectionSample3f ds(si, si_ref);
                ds.object = emitter;
                masked(emitter_pdf, emitter_hit) =
                    scene->pdf_emitter_direction(si_ref, ds, emitter_hit);
                active &= !emitter_hit; // turn off lanes which already
                                        // found emitter
            }
            if (any_or<true>(active_surface)) {
                auto bsdf         = si.bsdf(ray);
                Spectrum bsdf_val = bsdf->eval_null_transmission(si, active_surface);
                masked(transmittance, active_surface) *= bsdf_val;
            }
            Mask active_medium = neq(medium, nullptr) && active;
            if (any_or<true>(active_medium)) {
                masked(transmittance, active_medium) *=
                    medium->eval_transmittance(Ray(ray, 0, si.t), sampler, active_medium);
            }

            active &= si.is_valid() && any(neq(depolarize(transmittance), 0.f));

            // If a medium transition is taking place: Update the medium pointer
            Mask has_medium_trans = active && si.is_medium_transition();
            if (any_or<true>(has_medium_trans)) {
                masked(medium, has_medium_trans) = si.target_medium(ray.d);
            }
            // Update the ray with new origin & t parameter
            masked(ray.o, active)    = si.p;
            masked(ray.mint, active) = math::RayEpsilon<ScalarFloat>;
            interactions++;
        }
        return { value, emitter_pdf };
    }

    //! @}
    // =============================================================

    std::string to_string() const override {
        return tfm::format("VolumetricPathIntegrator[\n"
                           "  max_depth = %i,\n"
                           "  rr_depth = %i\n"
                           "]",
                           m_max_depth, m_rr_depth);
    }

    Float mis_weight(Float pdf_a, Float pdf_b) const {
        pdf_a *= pdf_a;
        pdf_b *= pdf_b;
        return select(pdf_a > 0.0f, pdf_a / (pdf_a + pdf_b), Float(0.0f));
    };

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS_VARIANT(VolumetricPathIntegrator, MonteCarloIntegrator);
MTS_EXPORT_PLUGIN(VolumetricPathIntegrator, "Volumetric Path Tracer integrator");
NAMESPACE_END(mitsuba)
