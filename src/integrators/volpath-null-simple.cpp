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
class VolumetricNullSimplePathIntegrator : public MonteCarloIntegrator<Float, Spectrum> {

public:
    MTS_IMPORT_BASE(MonteCarloIntegrator, m_max_depth, m_rr_depth, m_hide_emitters)
    MTS_IMPORT_TYPES(Scene, Sampler, Emitter, EmitterPtr, BSDF, BSDFPtr,
                     Medium, MediumPtr, PhaseFunctionContext)

    VolumetricNullSimplePathIntegrator(const Properties &props) : Base(props) {
        m_medium_mis = props.bool_("medium_mis", true);
    }

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
        UInt32 depth = 0;

        for (int bounce = 0;; ++bounce) {
            // ----------------- Handle termination of paths ------------------

            // Russian roulette: try to keep path weights equal to one, while accounting for the
            // solid angle compression at refractive index boundaries. Stop with at least some
            // probability to avoid  getting stuck (e.g. due to total internal reflection)

            // active &= any(neq(depolarize(throughput), 0.f));
            // Float q = min(hmax(depolarize(throughput)) * sqr(eta), .95f);
            // Mask perform_rr = (depth > (uint32_t) m_rr_depth);
            // active &= !(sampler->next_1d(active) >= q && perform_rr);
            // masked(throughput, perform_rr) *= rcp(detach(q));

            Mask exceeded_max_depth = depth >= (uint32_t) m_max_depth;
            if (none(active) || all(exceeded_max_depth))
                break;

            // ----------------------- Sampling the RTE -----------------------
            Mask active_medium  = active && neq(medium, nullptr);
            Mask active_surface = active && !active_medium;
            Mask act_null_scatter = false;
            Mask act_medium_scatter = false;
            Mask escaped_medium = false;
            SurfaceInteraction3f si_medium;
            if (any_or<true>(active_medium)) {
                Spectrum medium_throughput;
                std::tie(si_medium, mi, medium_throughput) = medium->sample_interaction(
                    scene, ray, sampler->next_1d(active_medium), 0, active_medium);

                escaped_medium = active_medium && si_medium.is_valid() && !mi.is_valid();
                active_medium &= mi.is_valid();

                // Handle null and real scatter events
                Mask null_scatter = sampler->next_1d(active_medium) >= depolarize(mi.sigma_t)[0] / depolarize(mi.combined_extinction)[0];

                act_null_scatter |= null_scatter && active_medium;
                act_medium_scatter |= !act_null_scatter && active_medium;

                masked(depth, act_medium_scatter) += 1;
            }

            // Dont estimate lighting if we exceeded number of bounces
            active &= depth < (uint32_t) m_max_depth;
            act_medium_scatter &= active;

            if (any_or<true>(act_null_scatter)) {
                // If null scatter: Spawn new ray into the current ray direction
                // TODO: Potentially we can optimize the number of computed ray intersections here
                masked(ray.o, act_null_scatter) = mi.p;
                masked(ray.mint, act_null_scatter) = 0.f;
            }

            if (any_or<true>(act_medium_scatter)) {
                masked(throughput, act_medium_scatter) *= depolarize(mi.sigma_s) / depolarize(mi.sigma_t);

                PhaseFunctionContext phase_ctx(sampler);
                auto phase = mi.medium->phase_function();

                // --------------------- Emitter sampling ---------------------
                Mask sample_emitters = mi.medium->use_emitter_sampling();
                valid_ray |= act_medium_scatter;
                specular_chain &= !act_medium_scatter;
                specular_chain |= act_medium_scatter && !sample_emitters;

                Mask active_e = act_medium_scatter && sample_emitters;
                if (any_or<true>(active_e)) {
                    auto [ds, _] = scene->sample_emitter_direction(mi, sampler->next_2d(active_e), false, active_e);
                    active_e &= neq(ds.pdf, 0.f);
                    if (any_or<true>(active_e)) {
                        Ray3f nee_ray = mi.spawn_ray(ds.d);
                        nee_ray.mint = 0.f;
                        auto [emitted, _] = evaluate_direct_light(mi, scene, sampler, medium, nee_ray,
                                                             (uint32_t) (int(-1)), ds.dist, active_e);
                        Float phase_val = phase->eval(phase_ctx, mi, ds.d, active_e);
                        if (m_medium_mis) {
                            masked(result, active_e) += throughput * emitted * phase_val * mis_weight(ds.pdf, phase_val) / ds.pdf;
                        } else {
                            masked(result, active_e) += throughput * phase_val * emitted / ds.pdf;
                        }
                    }
                }

                // ------------------ Phase function sampling -----------------
                masked(phase, !act_medium_scatter) = nullptr;
                auto [wo, phase_pdf] = phase->sample(phase_ctx, mi, sampler->next_2d(act_medium_scatter), act_medium_scatter);
                Ray3f new_ray  = mi.spawn_ray(wo);
                new_ray.mint = 0.0f;
                masked(ray, act_medium_scatter) = new_ray;

                if (m_medium_mis) {
                    active_e = act_medium_scatter && sample_emitters && any(neq(depolarize(throughput), 0.f));
                    if (any_or<true>(active_e)) {
                        auto [emitted, emitter_pdf] = evaluate_direct_light(mi, scene, sampler, medium, new_ray,
                                                             (uint32_t) (int(-1)), -1.f, active_e);
                        result += select(active_e && neq(emitter_pdf, 0),
                                        mis_weight(phase_pdf, emitter_pdf) * throughput * emitted, 0.0f);
                    }
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
                Mask active_e = active_surface && has_flag(bsdf->flags(), BSDFFlags::Smooth) && (depth + 1 < (uint32_t) m_max_depth);

                if (likely(any_or<true>(active_e))) {
                    auto [ds, _] = scene->sample_emitter_direction(si, sampler->next_2d(active_e), false, active_e);
                    active_e &= neq(ds.pdf, 0.f);
                    if (any_or<true>(active_e)) {
                        Ray3f nee_ray = si.spawn_ray(ds.d);

                        // TODO: This has to be zero if its not reaching the same point
                        auto [emitted, _] = evaluate_direct_light(si, scene, sampler, medium, nee_ray,
                                                             (uint32_t) (int(-1)), ds.dist, active_e);

                        // Query the BSDF for that emitter-sampled direction
                        Vector3f wo       = si.to_local(ds.d);
                        Spectrum bsdf_val = bsdf->eval(ctx, si, wo, active_e);

                        // Determine probability of having sampled that same
                        // direction using BSDF sampling.
                        Float bsdf_pdf = bsdf->pdf(ctx, si, wo, active_e);
                        result[active_e] += throughput * bsdf_val * mis_weight(ds.pdf, bsdf_pdf) * emitted / ds.pdf;
                    }
                }

                // ----------------------- BSDF sampling ----------------------
                auto [bs, bsdf_val] = bsdf->sample(ctx, si, sampler->next_1d(active_surface),
                                                   sampler->next_2d(active_surface), active_surface);
                throughput[active_surface] *= bsdf_val;
                masked(eta, active_surface) *= bs.eta;

                Ray bsdf_ray                = si.spawn_ray(si.to_world(bs.wo));
                masked(ray, active_surface) = bsdf_ray;

                Mask non_null_bsdf = active_surface && !has_flag(bs.sampled_type, BSDFFlags::Null);
                masked(depth, non_null_bsdf) += 1;

                valid_ray |= non_null_bsdf;
                specular_chain |= non_null_bsdf && has_flag(bs.sampled_type, BSDFFlags::Delta);
                specular_chain &= !(active_surface && has_flag(bs.sampled_type, BSDFFlags::Smooth));

                Mask add_emitter = active_surface && !has_flag(bs.sampled_type, BSDFFlags::Delta) &&
                                   any(neq(depolarize(throughput), 0.f)) && (depth < (uint32_t) m_max_depth);
                act_null_scatter |= active_surface && has_flag(bs.sampled_type, BSDFFlags::Null);
                // TODO: Should we just handle infinite null interactions here too?
                uint32_t max_intersections = (uint32_t) (int(-1));

                auto [emitted, emitter_pdf] =
                    evaluate_direct_light(si, scene, sampler, medium, ray,
                                                    max_intersections, -1.f, add_emitter);
                result += select(add_emitter && neq(emitter_pdf, 0),
                                mis_weight(bs.pdf, emitter_pdf) * throughput * emitted, 0.0f);

                Mask has_medium_trans            = si.is_valid() && si.is_medium_transition();
                masked(medium, has_medium_trans) = si.target_medium(ray.d);
            }
            active &= (active_surface | active_medium);
        }
        return { result, valid_ray };
    }

    // TODO: Make sure we match emitter IDs in case there are multiple emitters in the scene
    // (otherwise we could get the wrong MIS weights)

    std::pair<Spectrum, Float>
    evaluate_direct_light(const Interaction3f &ref_interaction, const Scene *scene,
                          Sampler *sampler, MediumPtr medium, Ray3f ray,
                          uint32_t maxInteractions, Float dist, Mask active) const {

        using EmitterPtr = replace_scalar_t<Float, const Emitter *>;
        Spectrum emitter_val(0.0f);
        uint32_t interactions = 0;

        Spectrum transmittance(1.0f);
        Float emitter_pdf(0.0f);

        UInt32 depth        = 0;
        Float total_dist = 0.f;
        while (any(active)) {
            Mask escaped_medium = false;
            Mask active_medium  = active && neq(medium, nullptr);
            Mask active_surface = active && !active_medium;

            SurfaceInteraction3f si_medium;
            if (any_or<true>(active_medium)) {
                Spectrum tr;
                MediumInteraction3f mi;
                std::tie(si_medium, mi, tr) = medium->sample_interaction(
                    scene, ray, sampler->next_1d(active_medium), 0, active_medium);

                escaped_medium = active_medium && si_medium.is_valid() && !mi.is_valid();
                active_medium &= mi.is_valid();
                masked(total_dist, active_medium) += mi.t;

                if (any_or<true>(active_medium)) {
                    masked(ray.o, active_medium)    = mi.p;
                    masked(ray.mint, active_medium) = 0.f;
                    masked(transmittance, active_medium) *= mi.sigma_n / depolarize(mi.combined_extinction);
                }
            }

            // Handle interactions with surfaces
            SurfaceInteraction3f si    = scene->ray_intersect(ray, active_surface);
            masked(si, escaped_medium) = si_medium;
            active_surface |= escaped_medium;
            masked(total_dist, active_surface) += si.t;

            // Check if we hit an emitter and add illumination if needed
            EmitterPtr emitter = si.emitter(scene, active_surface);
            Mask emitter_hit   = neq(emitter, nullptr) && active_surface;
            emitter_hit &= !si.is_valid() || (dist < 0.f) || (si.is_valid() &&
                            (total_dist >= dist - math::RayEpsilon<Float>) &&  (total_dist <= dist + math::RayEpsilon<Float>));
            if (any_or<true>(emitter_hit)) {
                DirectionSample3f ds(si, ref_interaction);
                ds.object                        = emitter;
                masked(emitter_val, emitter_hit) = emitter->eval(si, emitter_hit);
                masked(emitter_pdf, emitter_hit) = scene->pdf_emitter_direction(ref_interaction, ds, emitter_hit);
                active &= !emitter_hit; // disable lanes which found an emitter
                active_surface &= active;
                active_medium &= active;
            }

            active_surface &= si.is_valid();
            if (any_or<true>(active_surface)) {
                auto bsdf         = si.bsdf(ray);
                Spectrum bsdf_val = bsdf->eval_null_transmission(si, active_surface);
                masked(transmittance, active_surface) *= bsdf_val;
            }

            // Update the ray with new origin & t parameter
            masked(ray, active_surface) = si.spawn_ray(ray.d);
            masked(depth, active_surface) += 1; // only add depth if a surface is hit

            // Continue tracing through scene if non-zero weights exist
            active &= (active_medium || active_surface) && any(neq(transmittance, 0.f));

            // If a medium transition is taking place: Update the medium pointer
            Mask has_medium_trans = active_surface && si.is_medium_transition();
            if (any_or<true>(has_medium_trans)) {
                masked(medium, has_medium_trans) = si.target_medium(ray.d);
            }
        }
        return { transmittance * emitter_val, emitter_pdf };
    }


    //! @}
    // =============================================================

    std::string to_string() const override {
        return tfm::format("VolumetricNullSimplePathIntegrator[\n"
                           "  max_depth = %i,\n"
                           "  rr_depth = %i\n"
                           "]",
                           m_max_depth, m_rr_depth);
    }

    Float mis_weight(Float pdf_a, Float pdf_b) const {
        return select(pdf_a > 0.0f, pdf_a / (pdf_a + pdf_b), Float(0.0f));
        // TODO: Reenable power heuristic
        // pdf_a *= pdf_a;
        // pdf_b *= pdf_b;
        // return select(pdf_a > 0.0f, pdf_a / (pdf_a + pdf_b), Float(0.0f));
    };

    MTS_DECLARE_CLASS()
protected:
    bool m_medium_mis;
};

MTS_IMPLEMENT_CLASS_VARIANT(VolumetricNullSimplePathIntegrator, MonteCarloIntegrator);
MTS_EXPORT_PLUGIN(VolumetricNullSimplePathIntegrator, "Volumetric Path Tracer integrator");
NAMESPACE_END(mitsuba)
