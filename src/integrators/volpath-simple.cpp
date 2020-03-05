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
    }

    MTS_INLINE
    Float index_spectrum(const UnpolarizedSpectrum &spec, const UInt32 &idx) const {
        Float m = spec[0];
        if constexpr (is_rgb_v<Spectrum>) { // Handle RGB rendering
            masked(m, eq(idx, 1u)) = spec[1];
            masked(m, eq(idx, 2u)) = spec[2];
        } else {
            ENOKI_MARK_USED(idx);
        }
        return m;
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const RayDifferential3f &ray_,
                                     Float * /* aovs */,
                                     Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

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

        UInt32 channel = 0;
        if (is_rgb_v<Spectrum>) {
            uint32_t n_channels = (uint32_t) array_size_v<Spectrum>;
            channel = min(sampler->next_1d(active) * n_channels, n_channels - 1);
        }

        SurfaceInteraction3f si;
        Mask needs_intersection = true;
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
            Mask act_null_scatter = false, act_medium_scatter = false,
                 escaped_medium = false;

            // If the medium does not have a spectrally varying extinction,
            // we can perform a few optimizations to speed up rendering
            Mask is_spectral = active_medium;
            Mask not_spectral = false;
            if (any_or<true>(active_medium)) {
                is_spectral &= medium->has_spectral_extinction();
                not_spectral = !is_spectral && active_medium;
            }

            if (any_or<true>(active_medium)) {
                mi = medium->sample_interaction(ray, sampler->next_1d(active_medium), channel, active_medium);
                masked(ray.maxt, active_medium && medium->is_homogeneous() && mi.is_valid()) = mi.t;
                Mask intersect = needs_intersection && active_medium;
                if (any_or<true>(intersect))
                    masked(si, intersect) = scene->ray_intersect(ray, intersect);
                needs_intersection &= !active_medium;

                masked(mi.t, active_medium && (si.t < mi.t)) = math::Infinity<Float>;
                if (any_or<true>(is_spectral)) {
                    auto [tr, free_flight_pdf] = medium->eval_tr_and_pdf(mi, si, is_spectral);
                    Float tr_pdf = index_spectrum(free_flight_pdf, channel);
                    masked(throughput, is_spectral) *= select(tr_pdf > 0.f, tr / tr_pdf, 0.f);
                }

                escaped_medium = active_medium && !mi.is_valid();
                active_medium &= mi.is_valid();

                // Handle null and real scatter events
                Mask null_scatter = sampler->next_1d(active_medium) >= index_spectrum(mi.sigma_t, channel) / index_spectrum(mi.combined_extinction, channel);

                act_null_scatter |= null_scatter && active_medium;
                act_medium_scatter |= !act_null_scatter && active_medium;

                if (any_or<true>(is_spectral && act_null_scatter))
                    masked(throughput, is_spectral && act_null_scatter) *=
                        mi.sigma_n * index_spectrum(mi.combined_extinction, channel) /
                        index_spectrum(mi.sigma_n, channel);

                masked(depth, act_medium_scatter) += 1;
            }

            // Dont estimate lighting if we exceeded number of bounces
            active &= depth < (uint32_t) m_max_depth;
            act_medium_scatter &= active;

            if (any_or<true>(act_null_scatter)) {
                masked(ray.o, act_null_scatter) = mi.p;
                masked(ray.mint, act_null_scatter) = 0.f;
                masked(si.t, act_null_scatter) = si.t - mi.t;
            }

            if (any_or<true>(act_medium_scatter)) {
                if (any_or<true>(is_spectral))
                    masked(throughput, is_spectral && act_medium_scatter) *=
                        mi.sigma_s * index_spectrum(mi.combined_extinction, channel) / index_spectrum(mi.sigma_t, channel);
                if (any_or<true>(not_spectral))
                    masked(throughput, not_spectral && act_medium_scatter) *= mi.sigma_s / mi.sigma_t;

                PhaseFunctionContext phase_ctx(sampler);
                auto phase = mi.medium->phase_function();

                // --------------------- Emitter sampling ---------------------
                Mask sample_emitters = mi.medium->use_emitter_sampling();
                valid_ray |= act_medium_scatter;
                specular_chain &= !act_medium_scatter;
                specular_chain |= act_medium_scatter && !sample_emitters;

                Mask active_e = act_medium_scatter && sample_emitters;
                if (any_or<true>(active_e)) {
                    auto ds = scene->sample_emitter_direction(mi, sampler->next_2d(active_e), false, active_e).first;
                    active_e &= neq(ds.pdf, 0.f);
                    if (any_or<true>(active_e)) {
                        Ray3f nee_ray = mi.spawn_ray(ds.d);
                        nee_ray.mint = 0.f;
                        auto emitted = evaluate_direct_light(mi, scene, sampler, medium, nee_ray, true, si,
                                                             ds.dist, channel, active_e).first;
                        Float phase_val = phase->eval(phase_ctx, mi, ds.d, active_e);
                        masked(result, active_e) += throughput * phase_val * emitted / ds.pdf;
                    }
                }

                // ------------------ Phase function sampling -----------------
                masked(phase, !act_medium_scatter) = nullptr;
                auto [wo, phase_pdf] = phase->sample(phase_ctx, mi, sampler->next_2d(act_medium_scatter), act_medium_scatter);
                Ray3f new_ray  = mi.spawn_ray(wo);
                new_ray.mint = 0.0f;
                masked(ray, act_medium_scatter) = new_ray;
                needs_intersection |= act_medium_scatter;
            }

            // --------------------- Surface Interactions ---------------------
            active_surface |= escaped_medium;
            Mask intersect = active_surface && needs_intersection;
            if (any_or<true>(intersect))
                masked(si, intersect) = scene->ray_intersect(ray, intersect);

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
                    auto ds = scene->sample_emitter_direction(si, sampler->next_2d(active_e), false, active_e).first;
                    active_e &= neq(ds.pdf, 0.f);
                    if (any_or<true>(active_e)) {
                        Ray3f nee_ray = si.spawn_ray(ds.d);

                        // TODO: This has to be zero if its not reaching the same point
                        auto emitted = evaluate_direct_light(si, scene, sampler, medium, nee_ray, true, si,
                                                             ds.dist, channel, active_e).first;

                        // Query the BSDF for that emitter-sampled direction
                        Vector3f wo       = si.to_local(ds.d);
                        Spectrum bsdf_val = bsdf->eval(ctx, si, wo, active_e);
                        bsdf_val = si.to_world_mueller(bsdf_val, -wo, si.wi);

                        // Determine probability of having sampled that same
                        // direction using BSDF sampling.
                        Float bsdf_pdf = bsdf->pdf(ctx, si, wo, active_e);
                        result[active_e] += throughput * bsdf_val * mis_weight(ds.pdf, bsdf_pdf) * emitted / ds.pdf;
                    }
                }

                // ----------------------- BSDF sampling ----------------------
                auto [bs, bsdf_val] = bsdf->sample(ctx, si, sampler->next_1d(active_surface),
                                                   sampler->next_2d(active_surface), active_surface);
                bsdf_val = si.to_world_mueller(bsdf_val, -bs.wo, si.wi);

                masked(throughput, active_surface) *= bsdf_val;
                masked(eta, active_surface) *= bs.eta;

                Ray bsdf_ray                = si.spawn_ray(si.to_world(bs.wo));
                masked(ray, active_surface) = bsdf_ray;
                needs_intersection |= active_surface;

                Mask non_null_bsdf = active_surface && !has_flag(bs.sampled_type, BSDFFlags::Null);
                masked(depth, non_null_bsdf) += 1;

                valid_ray |= non_null_bsdf;
                specular_chain |= non_null_bsdf && has_flag(bs.sampled_type, BSDFFlags::Delta);
                specular_chain &= !(active_surface && has_flag(bs.sampled_type, BSDFFlags::Smooth));

                Mask add_emitter = active_surface && !has_flag(bs.sampled_type, BSDFFlags::Delta) &&
                                   any(neq(depolarize(throughput), 0.f)) && (depth < (uint32_t) m_max_depth);
                act_null_scatter |= active_surface && has_flag(bs.sampled_type, BSDFFlags::Null);

                // Intersect the indirect ray against the scene
                Mask intersect2 = active_surface && needs_intersection && add_emitter;
                SurfaceInteraction3f si_new = si;
                if (any_or<true>(intersect2))
                    masked(si_new, intersect2) = scene->ray_intersect(ray, intersect2);
                needs_intersection &= !intersect2;

                auto [emitted, emitter_pdf] = evaluate_direct_light(si, scene, sampler,
                                                                    medium, ray, false, si_new,
                                                                    -1.f, channel, add_emitter);
                result += select(add_emitter && neq(emitter_pdf, 0),
                                mis_weight(bs.pdf, emitter_pdf) * throughput * emitted, 0.0f);

                Mask has_medium_trans            = active_surface && si.is_medium_transition();
                masked(medium, has_medium_trans) = si.target_medium(ray.d);

                masked(si, intersect2) = si_new;
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
                          Mask needs_intersection, const SurfaceInteraction3f &si_ray,
                          Float dist, UInt32 channel, Mask active) const {

        using EmitterPtr = replace_scalar_t<Float, const Emitter *>;
        Spectrum emitter_val(0.0f);

        Spectrum transmittance(1.0f);
        Float emitter_pdf(0.0f);

        Float total_dist = 0.f;
        SurfaceInteraction3f si = si_ray;
        while (any(active)) {
            Mask escaped_medium = false;
            Mask active_medium  = active && neq(medium, nullptr);
            Mask active_surface = active && !active_medium;
            SurfaceInteraction3f si_medium;
            if (any_or<true>(active_medium)) {
                auto mi = medium->sample_interaction(ray, sampler->next_1d(active_medium), channel, active_medium);
                masked(ray.maxt, active_medium && medium->is_homogeneous() && mi.is_valid()) = mi.t;
                Mask intersect = needs_intersection && active_medium;
                if (any_or<true>(intersect))
                    masked(si, intersect) = scene->ray_intersect(ray, intersect);

                masked(mi.t, active_medium && (si.t < mi.t)) = math::Infinity<Float>;

                Mask is_spectral = medium->has_spectral_extinction() && active_medium;
                Mask not_spectral = !is_spectral && active_medium;
                if (any_or<true>(is_spectral)) {
                    auto [tr, free_flight_pdf] = medium->eval_tr_and_pdf(mi, si, is_spectral);
                    Float tr_pdf = index_spectrum(free_flight_pdf, channel);
                    masked(transmittance, is_spectral) *= select(tr_pdf > 0.f, tr / tr_pdf, 0.f);
                }

                needs_intersection &= !active_medium;
                escaped_medium = active_medium && !mi.is_valid();
                active_medium &= mi.is_valid();
                masked(total_dist, active_medium) += mi.t;

                if (any_or<true>(active_medium)) {
                    masked(ray.o, active_medium)    = mi.p;
                    masked(ray.mint, active_medium) = 0.f;
                    masked(si.t, active_medium) = si.t - mi.t;

                    if (any_or<true>(is_spectral))
                        masked(transmittance, is_spectral && active_medium) *= mi.sigma_n;
                    if (any_or<true>(not_spectral))
                        masked(transmittance, not_spectral && active_medium) *= mi.sigma_n / mi.combined_extinction;
                }
            }

            // Handle interactions with surfaces
            Mask intersect = active_surface && needs_intersection;
            masked(si, intersect)    = scene->ray_intersect(ray, intersect);
            needs_intersection &= !intersect;
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

            active_surface &= si.is_valid() && !active_medium;
            if (any_or<true>(active_surface)) {
                auto bsdf         = si.bsdf(ray);
                Spectrum bsdf_val = bsdf->eval_null_transmission(si, active_surface);
                bsdf_val = si.to_world_mueller(bsdf_val, si.wi, si.wi);

                masked(transmittance, active_surface) *= bsdf_val;
            }

            // Update the ray with new origin & t parameter
            masked(ray, active_surface) = si.spawn_ray(ray.d);
            needs_intersection |= active_surface;

            // Continue tracing through scene if non-zero weights exist
            active &= (active_medium || active_surface) && any(neq(depolarize(transmittance), 0.f));

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
