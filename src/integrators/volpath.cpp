#include <random>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>


NAMESPACE_BEGIN(mitsuba)

/**!

.. _integrator-volpath:

Volumetric path tracer (:monosp:`volpath`)
-------------------------------------------

.. pluginparameters::

 * - max_depth
   - |int|
   - Specifies the longest path depth in the generated output image (where -1 corresponds to
     :math:`\infty`). A value of 1 will only render directly visible light sources. 2 will lead
     to single-bounce (direct-only) illumination, and so on. (Default: -1)
 * - rr_depth
   - |int|
   - Specifies the minimum path depth, after which the implementation will start to use the
     *russian roulette* path termination criterion. (Default: 5)
 * - hide_emitters
   - |bool|
   - Hide directly visible emitters. (Default: no, i.e. |false|)

This plugin provides a volumetric path tracer that can be used to compute approximate solutions
of the radiative transfer equation. Its implementation makes use of multiple importance sampling
to combine BSDF and phase function sampling with direct illumination sampling strategies. On
surfaces, it behaves exactly like the standard path tracer.

This integrator has special support for index-matched transmission events (i.e. surface scattering
events that do not change the direction of light). As a consequence, participating media enclosed by
a stencil shape are rendered considerably more efficiently when this shape
has a :ref:`null <bsdf-null>` or :ref:`thin dielectric <bsdf-thindielectric>` BSDF assigned
to it (as compared to, say, a :ref:`dielectric <bsdf-dielectric>` or
:ref:`roughdielectric <bsdf-roughdielectric>` BSDF).

.. note:: This integrator does not implement good sampling strategies to render
    participating media with a spectrally varying extinction coefficient. For these cases,
    it is better to use the more advanced :ref:`volumetric path tracer with
    spectral MIS <integrator-volpathmis>`, which will produce in a significantly less noisy
    rendered image.

*/
template <typename Float, typename Spectrum>
class VolumetricPathIntegrator : public MonteCarloIntegrator<Float, Spectrum> {

public:
    MTS_IMPORT_BASE(MonteCarloIntegrator, m_max_depth, m_rr_depth, m_hide_emitters)
    MTS_IMPORT_TYPES(Scene, Sampler, Emitter, EmitterPtr, BSDF, BSDFPtr,
                     Medium, MediumPtr, PhaseFunctionContext)

    VolumetricPathIntegrator(const Properties &props) : Base(props) {
    }

    MTS_INLINE
    Float index_spectrum(const UnpolarizedSpectrum &spec, const UInt32 &idx) const {
        Float m = spec[0];
        if constexpr (is_rgb_v<Spectrum>) { // Handle RGB rendering
            ek::masked(m, ek::eq(idx, 1u)) = spec[1];
            ek::masked(m, ek::eq(idx, 2u)) = spec[2];
        } else {
            ENOKI_MARK_USED(idx);
        }
        return m;
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const RayDifferential3f &ray_,
                                     const Medium *initial_medium,
                                     Float * /* aovs */,
                                     Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        // If there is an environment emitter and emitters are visible: all rays will be valid
        // Otherwise, it will depend on whether a valid interaction is sampled
        Mask valid_ray = !m_hide_emitters && ek::neq(scene->environment(), nullptr);

        // For now, don't use ray differentials
        Ray3f ray = ray_;

        // Tracks radiance scaling due to index of refraction changes
        Float eta(1.f);

        Spectrum throughput(1.f), result(0.f);
        MediumPtr medium = initial_medium;
        MediumInteraction3f mi = ek::zero<MediumInteraction3f>();
        Mask specular_chain = active && !m_hide_emitters;
        UInt32 depth = 0;

        UInt32 channel = 0;
        if (is_rgb_v<Spectrum>) {
            uint32_t n_channels = (uint32_t) ek::array_size_v<Spectrum>;
            channel = (UInt32) ek::min(sampler->next_1d(active) * n_channels, n_channels - 1);
        }

        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();
        Mask needs_intersection = true;
        Interaction3f last_scatter_event = ek::zero<Interaction3f>();
        Float last_scatter_direction_pdf = 1.f;

        for (int bounce = 0;; ++bounce) {
            // ----------------- Handle termination of paths ------------------
            // Russian roulette: try to keep path weights equal to one, while accounting for the
            // solid angle compression at refractive index boundaries. Stop with at least some
            // probability to avoid  getting stuck (e.g. due to total internal reflection)
            active &= ek::any(ek::neq(depolarize(throughput), 0.f));
            Float q = ek::min(ek::hmax(depolarize(throughput)) * ek::sqr(eta), .95f);
            Mask perform_rr = (depth > (uint32_t) m_rr_depth);
            active &= sampler->next_1d(active) < q || !perform_rr;
            ek::masked(throughput, perform_rr) *= ek::rcp(ek::detach(q));

            Mask exceeded_max_depth = depth >= (uint32_t) m_max_depth;
            if (ek::none(active) || ek::all(exceeded_max_depth))
                break;

            // ----------------------- Sampling the RTE -----------------------
            Mask active_medium  = active && ek::neq(medium, nullptr);
            Mask active_surface = active && !active_medium;
            Mask act_null_scatter = false, act_medium_scatter = false,
                 escaped_medium = false;

            // If the medium does not have a spectrally varying extinction,
            // we can perform a few optimizations to speed up rendering
            Mask is_spectral = active_medium;
            Mask not_spectral = false;
            if (ek::any_or<true>(active_medium)) {
                is_spectral &= medium->has_spectral_extinction();
                not_spectral = !is_spectral && active_medium;
            }

            if (ek::any_or<true>(active_medium)) {
                mi = medium->sample_interaction(ray, sampler->next_1d(active_medium), channel, active_medium);
                ek::masked(ray.maxt, active_medium && medium->is_homogeneous() && mi.is_valid()) = mi.t;
                Mask intersect = needs_intersection && active_medium;
                if (ek::any_or<true>(intersect))
                    ek::masked(si, intersect) = scene->ray_intersect(ray, intersect);
                needs_intersection &= !active_medium;

                ek::masked(mi.t, active_medium && (si.t < mi.t)) = ek::Infinity<Float>;
                if (ek::any_or<true>(is_spectral)) {
                    auto [tr, free_flight_pdf] = medium->eval_tr_and_pdf(mi, si, is_spectral);
                    Float tr_pdf = index_spectrum(free_flight_pdf, channel);
                    ek::masked(throughput, is_spectral) *= ek::select(tr_pdf > 0.f, tr / tr_pdf, 0.f);
                }

                escaped_medium = active_medium && !mi.is_valid();
                active_medium &= mi.is_valid();

                // Handle null and real scatter events
                Mask null_scatter = sampler->next_1d(active_medium) >= index_spectrum(mi.sigma_t, channel) / index_spectrum(mi.combined_extinction, channel);

                act_null_scatter |= null_scatter && active_medium;
                act_medium_scatter |= !act_null_scatter && active_medium;

                if (ek::any_or<true>(is_spectral && act_null_scatter))
                    ek::masked(throughput, is_spectral && act_null_scatter) *=
                        mi.sigma_n * index_spectrum(mi.combined_extinction, channel) /
                        index_spectrum(mi.sigma_n, channel);

                ek::masked(depth, act_medium_scatter) += 1;
                ek::masked(last_scatter_event, act_medium_scatter) = mi;
            }

            // Dont estimate lighting if we exceeded number of bounces
            active &= depth < (uint32_t) m_max_depth;
            act_medium_scatter &= active;

            if (ek::any_or<true>(act_null_scatter)) {
                ek::masked(ray.o, act_null_scatter) = mi.p;
                ek::masked(ray.mint, act_null_scatter) = 0.f;
                ek::masked(si.t, act_null_scatter) = si.t - mi.t;
            }

            if (ek::any_or<true>(act_medium_scatter)) {
                if (ek::any_or<true>(is_spectral))
                    ek::masked(throughput, is_spectral && act_medium_scatter) *=
                        mi.sigma_s * index_spectrum(mi.combined_extinction, channel) / index_spectrum(mi.sigma_t, channel);
                if (ek::any_or<true>(not_spectral))
                    ek::masked(throughput, not_spectral && act_medium_scatter) *= mi.sigma_s / mi.sigma_t;

                PhaseFunctionContext phase_ctx(sampler);
                auto phase = mi.medium->phase_function();

                // --------------------- Emitter sampling ---------------------
                Mask sample_emitters = mi.medium->use_emitter_sampling();
                valid_ray |= act_medium_scatter;
                specular_chain &= !act_medium_scatter;
                specular_chain |= act_medium_scatter && !sample_emitters;

                Mask active_e = act_medium_scatter && sample_emitters;
                if (ek::any_or<true>(active_e)) {
                    auto [emitted, ds] = sample_emitter(mi, true, scene, sampler, medium, channel, active_e);
                    Float phase_val = phase->eval(phase_ctx, mi, ds.d, active_e);
                    ek::masked(result, active_e) += throughput * phase_val * emitted * mis_weight(ds.pdf, phase_val);
                }

                // ------------------ Phase function sampling -----------------
                ek::masked(phase, !act_medium_scatter) = nullptr;
                auto [wo, phase_pdf] = phase->sample(phase_ctx, mi,
                    sampler->next_1d(act_medium_scatter),
                    sampler->next_2d(act_medium_scatter),
                    act_medium_scatter);
                Ray3f new_ray  = mi.spawn_ray(wo);
                new_ray.mint = 0.0f;
                ek::masked(ray, act_medium_scatter) = new_ray;
                needs_intersection |= act_medium_scatter;
                ek::masked(last_scatter_direction_pdf, act_medium_scatter) = phase_pdf;
            }

            // --------------------- Surface Interactions ---------------------
            active_surface |= escaped_medium;
            Mask intersect = active_surface && needs_intersection;
            if (ek::any_or<true>(intersect))
                ek::masked(si, intersect) = scene->ray_intersect(ray, intersect);

            if (ek::any_or<true>(active_surface)) {
                // ---------------- Intersection with emitters ----------------
                Mask ray_from_camera = active_surface && ek::eq(depth, 0u);
                Mask count_direct = ray_from_camera || specular_chain;
                EmitterPtr emitter = si.emitter(scene);
                Mask active_e = active_surface && ek::neq(emitter, nullptr)
                                && !(ek::eq(depth, 0u) && m_hide_emitters);
                if (ek::any_or<true>(active_e)) {
                    Float emitter_pdf = 1.0f;
                    if (ek::any_or<true>(active_e && !count_direct)) {
                        // Get the PDF of sampling this emitter using next event estimation
                        DirectionSample3f ds(scene, si, last_scatter_event);
                        emitter_pdf = scene->pdf_emitter_direction(last_scatter_event, ds, active_e);
                    }
                    Spectrum emitted = emitter->eval(si, active_e);
                    Spectrum contrib = ek::select(count_direct, throughput * emitted,
                                                  throughput * mis_weight(last_scatter_direction_pdf, emitter_pdf) * emitted);
                    ek::masked(result, active_e) += contrib;
                }
            }
            active_surface &= si.is_valid();
            if (ek::any_or<true>(active_surface)) {
                // --------------------- Emitter sampling ---------------------
                BSDFContext ctx;
                BSDFPtr bsdf  = si.bsdf(ray);
                Mask active_e = active_surface && has_flag(bsdf->flags(), BSDFFlags::Smooth) && (depth + 1 < (uint32_t) m_max_depth);

                if (likely(ek::any_or<true>(active_e))) {
                    auto [emitted, ds] = sample_emitter(si, false, scene, sampler, medium, channel, active_e);

                    // Query the BSDF for that emitter-sampled direction
                    Vector3f wo       = si.to_local(ds.d);
                    Spectrum bsdf_val = bsdf->eval(ctx, si, wo, active_e);
                    bsdf_val = si.to_world_mueller(bsdf_val, -wo, si.wi);

                    // Determine probability of having sampled that same
                    // direction using BSDF sampling.
                    Float bsdf_pdf = bsdf->pdf(ctx, si, wo, active_e);
                    result[active_e] += throughput * bsdf_val * mis_weight(ds.pdf, ek::select(ds.delta, 0.f, bsdf_pdf)) * emitted;
                }

                // ----------------------- BSDF sampling ----------------------
                auto [bs, bsdf_val] = bsdf->sample(ctx, si, sampler->next_1d(active_surface),
                                                   sampler->next_2d(active_surface), active_surface);
                bsdf_val = si.to_world_mueller(bsdf_val, -bs.wo, si.wi);

                ek::masked(throughput, active_surface) *= bsdf_val;
                ek::masked(eta, active_surface) *= bs.eta;

                Ray bsdf_ray                    = si.spawn_ray(si.to_world(bs.wo));
                ek::masked(ray, active_surface) = bsdf_ray;
                needs_intersection |= active_surface;

                Mask non_null_bsdf = active_surface && !has_flag(bs.sampled_type, BSDFFlags::Null);
                ek::masked(depth, non_null_bsdf) += 1;

                // update the last scatter PDF event if we encountered a non-null scatter event
                ek::masked(last_scatter_event, non_null_bsdf) = si;
                ek::masked(last_scatter_direction_pdf, non_null_bsdf) = bs.pdf;

                valid_ray |= non_null_bsdf;
                specular_chain |= non_null_bsdf && has_flag(bs.sampled_type, BSDFFlags::Delta);
                specular_chain &= !(active_surface && has_flag(bs.sampled_type, BSDFFlags::Smooth));
                act_null_scatter |= active_surface && has_flag(bs.sampled_type, BSDFFlags::Null);
                Mask has_medium_trans                = active_surface && si.is_medium_transition();
                ek::masked(medium, has_medium_trans) = si.target_medium(ray.d);
            }
            active &= (active_surface | active_medium);
        }
        return { result, valid_ray };
    }


    /// Samples an emitter in the scene and evaluates it's attenuated contribution
    std::tuple<Spectrum, DirectionSample3f>
    sample_emitter(const Interaction3f &ref_interaction, Mask is_medium_interaction, const Scene *scene,
                          Sampler *sampler, MediumPtr medium, UInt32 channel, Mask active) const {
        using EmitterPtr = ek::replace_scalar_t<Float, const Emitter *>;
        Spectrum transmittance(1.0f);

        auto [ds, emitter_val] = scene->sample_emitter_direction(ref_interaction, sampler->next_2d(active), false, active);
        ek::masked(emitter_val, ek::eq(ds.pdf, 0.f)) = 0.f;
        active &= ek::neq(ds.pdf, 0.f);

        if (ek::none_or<false>(active)) {
            return { emitter_val, ds };
        }

        Ray3f ray = ref_interaction.spawn_ray(ds.d);
        ek::masked(ray.mint, is_medium_interaction) = 0.f;

        Float total_dist = 0.f;
        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();
        Mask needs_intersection = true;
        while (ek::any(active)) {
            Float remaining_dist = ds.dist * (1.f - math::ShadowEpsilon<Float>) - total_dist;
            ray.maxt = remaining_dist;
            active &= remaining_dist > 0.f;
            if (ek::none(active))
                break;

            Mask escaped_medium = false;
            Mask active_medium  = active && ek::neq(medium, nullptr);
            Mask active_surface = active && !active_medium;

            if (ek::any_or<true>(active_medium)) {
                auto mi = medium->sample_interaction(ray, sampler->next_1d(active_medium), channel, active_medium);
                ek::masked(ray.maxt, active_medium && medium->is_homogeneous() && mi.is_valid()) = ek::min(mi.t, remaining_dist);
                Mask intersect = needs_intersection && active_medium;
                if (ek::any_or<true>(intersect))
                    ek::masked(si, intersect) = scene->ray_intersect(ray, intersect);

                ek::masked(mi.t, active_medium && (si.t < mi.t)) = ek::Infinity<Float>;
                needs_intersection &= !active_medium;

                Mask is_spectral = medium->has_spectral_extinction() && active_medium;
                Mask not_spectral = !is_spectral && active_medium;
                if (ek::any_or<true>(is_spectral)) {
                    Float t      = ek::min(remaining_dist, ek::min(mi.t, si.t)) - mi.mint;
                    UnpolarizedSpectrum tr  = ek::exp(-t * mi.combined_extinction);
                    UnpolarizedSpectrum free_flight_pdf = ek::select(si.t < mi.t || mi.t > remaining_dist, tr, tr * mi.combined_extinction);
                    Float tr_pdf = index_spectrum(free_flight_pdf, channel);
                    ek::masked(transmittance, is_spectral) *= ek::select(tr_pdf > 0.f, tr / tr_pdf, 0.f);
                }

                // Handle exceeding the maximum distance by medium sampling
                ek::masked(total_dist, active_medium && (mi.t > remaining_dist) && mi.is_valid()) = ds.dist;
                ek::masked(mi.t, active_medium && (mi.t > remaining_dist)) = ek::Infinity<Float>;

                escaped_medium = active_medium && !mi.is_valid();
                active_medium &= mi.is_valid();
                is_spectral &= active_medium;
                not_spectral &= active_medium;

                ek::masked(total_dist, active_medium) += mi.t;

                if (ek::any_or<true>(active_medium)) {
                    ek::masked(ray.o, active_medium)    = mi.p;
                    ek::masked(ray.mint, active_medium) = 0.f;
                    ek::masked(si.t, active_medium) = si.t - mi.t;

                    if (ek::any_or<true>(is_spectral))
                        ek::masked(transmittance, is_spectral) *= mi.sigma_n;
                    if (ek::any_or<true>(not_spectral))
                        ek::masked(transmittance, not_spectral) *= mi.sigma_n / mi.combined_extinction;
                }
            }

            // Handle interactions with surfaces
            Mask intersect = active_surface && needs_intersection;
            if (ek::any_or<true>(intersect))
                ek::masked(si, intersect)    = scene->ray_intersect(ray, intersect);
            needs_intersection &= !intersect;
            active_surface |= escaped_medium;
            ek::masked(total_dist, active_surface) += si.t;

            active_surface &= si.is_valid() && active && !active_medium;
            if (ek::any_or<true>(active_surface)) {
                auto bsdf         = si.bsdf(ray);
                Spectrum bsdf_val = bsdf->eval_null_transmission(si, active_surface);
                bsdf_val = si.to_world_mueller(bsdf_val, si.wi, si.wi);
                ek::masked(transmittance, active_surface) *= bsdf_val;
            }

            // Update the ray with new origin & t parameter
            ek::masked(ray, active_surface) = si.spawn_ray(ray.d);
            ray.maxt = remaining_dist;
            needs_intersection |= active_surface;

            // Continue tracing through scene if non-zero weights exist
            active &= (active_medium || active_surface) && ek::any(ek::neq(depolarize(transmittance), 0.f));

            // If a medium transition is taking place: Update the medium pointer
            Mask has_medium_trans = active_surface && si.is_medium_transition();
            if (ek::any_or<true>(has_medium_trans)) {
                ek::masked(medium, has_medium_trans) = si.target_medium(ray.d);
            }
        }
        return { transmittance * emitter_val, ds };
    }


    //! @}
    // =============================================================

    std::string to_string() const override {
        return tfm::format("VolumetricSimplePathIntegrator[\n"
                           "  max_depth = %i,\n"
                           "  rr_depth = %i\n"
                           "]",
                           m_max_depth, m_rr_depth);
    }

    Float mis_weight(Float pdf_a, Float pdf_b) const {
        pdf_a *= pdf_a;
        pdf_b *= pdf_b;
        Float w = pdf_a / (pdf_a + pdf_b);
        return ek::select(ek::isfinite(w), w, 0.f);
    };

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS_VARIANT(VolumetricPathIntegrator, MonteCarloIntegrator);
MTS_EXPORT_PLUGIN(VolumetricPathIntegrator, "Volumetric Path Tracer integrator");
NAMESPACE_END(mitsuba)
