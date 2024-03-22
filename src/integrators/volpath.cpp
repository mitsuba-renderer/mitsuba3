#include <random>
#include <tuple>
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

.. warning:: This integrator does not support forward-mode differentiation.

.. tabs::
    .. code-tab::  xml

        <integrator type="volpath">
            <integer name="max_depth" value="8"/>
        </integrator>

    .. code-tab:: python

        'type': 'volpath',
        'max_depth': 8

*/
template <typename Float, typename Spectrum>
class VolumetricPathIntegrator : public MonteCarloIntegrator<Float, Spectrum> {

public:
    MI_IMPORT_BASE(MonteCarloIntegrator, m_max_depth, m_rr_depth, m_hide_emitters)
    MI_IMPORT_TYPES(Scene, Sampler, Emitter, EmitterPtr, BSDF, BSDFPtr,
                     Medium, MediumPtr, PhaseFunctionContext)


    VolumetricPathIntegrator(const Properties &props) : Base(props) {
        auto sampling_mode = props.get<int>("sampling_mode", 0);
        if (sampling_mode == 0) {
            m_use_emitter_sampling = true;
            m_use_uni_sampling     = true;
        } else if (sampling_mode == 1) {
            m_use_emitter_sampling = false;
            m_use_uni_sampling     = true;
        } else if (sampling_mode == 2) {
            m_use_emitter_sampling = true;
            m_use_uni_sampling     = false;
        }
    }

    MI_INLINE
    Float index_spectrum(const UnpolarizedSpectrum &spec, const UInt32 &idx) const {
        Float m = spec[0];
        if constexpr (is_rgb_v<Spectrum>) { // Handle RGB rendering
            dr::masked(m, dr::eq(idx, 1u)) = spec[1];
            dr::masked(m, dr::eq(idx, 2u)) = spec[2];
        } else {
            DRJIT_MARK_USED(idx);
        }
        return m;
    }

    std::pair<Spectrum, Mask> sample(const Scene *scene,
                                     Sampler *sampler,
                                     const RayDifferential3f &ray_,
                                     const Medium *initial_medium,
                                     Float * /* aovs */,
                                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::SamplingIntegratorSample, active);

        // If there is an environment emitter and emitters are visible: all rays will be valid
        // Otherwise, it will depend on whether a valid interaction is sampled
        Mask valid_ray = !m_hide_emitters && dr::neq(scene->environment(), nullptr);

        // For now, don't use ray differentials
        Ray3f ray = ray_;

        // Tracks radiance scaling due to index of refraction changes
        Float eta(1.f);

        Spectrum throughput(1.f), result(0.f);
        MediumPtr medium = initial_medium;
        auto mei = dr::zeros<MediumInteraction3f>();
        Mask specular_chain = active && !m_hide_emitters;
        UInt32 depth = 0;

        UInt32 channel = 0;
        if (is_rgb_v<Spectrum>) {
            auto n_channels = (uint32_t) dr::array_size_v<Spectrum>;
            channel = (UInt32) dr::minimum(sampler->next_1d(active) * n_channels, n_channels - 1);
        }

        auto si = dr::zeros<SurfaceInteraction3f>();
        auto last_scatter_event = dr::zeros<Interaction3f>();

        Mask needs_intersection = true;
        Float last_scatter_direction_pdf = 1.f;

        /* Set up a Dr.Jit loop (optimizes away to a normal loop in scalar mode,
           generates wavefront or megakernel renderer based on configuration).
           Register everything that changes as part of the loop here */
        dr::Loop<Mask> loop("Volpath integrator",
                            /* loop state: */ active, depth, ray, throughput,
                            result, si, mei, medium, eta, last_scatter_event,
                            last_scatter_direction_pdf, needs_intersection,
                            specular_chain, valid_ray, sampler);

        while (loop(active)) {
            // ----------------- Handle termination of paths ------------------
            // Russian roulette: try to keep path weights equal to one, while accounting for the
            // solid angle compression at refractive index boundaries. Stop with at least some
            // probability to avoid  getting stuck (e.g. due to total internal reflection)
            active &= dr::any(dr::neq(unpolarized_spectrum(throughput), 0.f));
            Float q = dr::minimum(dr::max(unpolarized_spectrum(throughput)) * dr::sqr(eta), .95f);
            Mask perform_rr = (depth > (uint32_t) m_rr_depth);
            active &= sampler->next_1d(active) < q || !perform_rr;
            dr::masked(throughput, perform_rr) *= dr::rcp(dr::detach(q));

            active &= depth < (uint32_t) m_max_depth;
            if (dr::none_or<false>(active))
                break;

            // ----------------------- Sampling the RTE -----------------------
            Mask active_medium  = active && dr::neq(medium, nullptr);
            Mask active_surface = active && !active_medium;
            Mask act_null_scatter = false, act_medium_scatter = false,
                 escaped_medium = false;

            // If the medium does not have a spectrally varying extinction,
            // we can perform a few optimizations to speed up rendering
            Mask is_spectral = active_medium;
            Mask not_spectral = false;
            if (dr::any_or<true>(active_medium)) {
                is_spectral &= medium->has_spectral_extinction();
                not_spectral = !is_spectral && active_medium;
            }

            if (dr::any_or<true>(active_medium)) {
                mei = medium->sample_interaction(ray, sampler->next_1d(active_medium), channel, active_medium);
                dr::masked(ray.maxt, active_medium && medium->is_homogeneous() && mei.is_valid()) = mei.t;
                Mask intersect = needs_intersection && active_medium;
                if (dr::any_or<true>(intersect))
                    dr::masked(si, intersect) = scene->ray_intersect(ray, intersect);
                needs_intersection &= !active_medium;
                dr::masked(mei.t, active_medium && (si.t < mei.t)) = dr::Infinity<Float>;

                if (dr::any_or<true>(is_spectral)) {
                    auto [tr, free_flight_pdf] = medium->transmittance_eval_pdf(mei, si, active_medium);
                    Float tr_pdf = index_spectrum(free_flight_pdf, channel);
                    dr::masked(throughput, active_medium) *= dr::select(tr_pdf > 0.f, tr / tr_pdf, 0.f);
                }

                escaped_medium = active_medium && !mei.is_valid();
                active_medium &= mei.is_valid();
                is_spectral &= active_medium;
                not_spectral &= active_medium;
            }

            if (dr::any_or<true>(active_medium)) {
                // Compute emission, scatter and null event probabilities
                auto radiance = medium->get_radiance(mei, active_medium);
                auto [probabilities, weights] = medium->get_interaction_probabilities(radiance, mei, throughput);

                auto [prob_scatter, prob_null]     = probabilities;
                auto [weight_scatter, weight_null] = weights;

                // Handle null and real scatter events
                Mask null_scatter = sampler->next_1d(active_medium) >= index_spectrum(prob_scatter, channel);

                act_null_scatter   =  null_scatter && active_medium;
                act_medium_scatter = !null_scatter && active_medium;

                // ---------------- Intersection with emitters ----------------
                Mask ray_from_camera_medium = active_medium && dr::eq(depth, 0u);
                Mask count_direct_medium = ray_from_camera_medium || specular_chain;
                EmitterPtr emitter_medium = mei.emitter(active_medium);
                Mask active_medium_e = active_medium && dr::neq(emitter_medium, nullptr) &&
                                       !(dr::eq(depth, 0u) && m_hide_emitters);
                if (dr::any_or<true>(active_medium_e)) {
                    Float emitter_pdf = (m_use_emitter_sampling ? 1.0f : 0.0f);
                    Spectrum weight = 1.0f;
                    if (m_use_emitter_sampling && dr::any_or<true>(active_medium_e && !count_direct_medium)) {
                        DirectionSample3f ds(mei, last_scatter_event);
                        dr::masked(emitter_pdf, active_medium_e && !count_direct_medium) = scene->pdf_emitter_direction(last_scatter_event, ds, active_medium_e);

                        // Get the PDF of sampling this emitter using next event estimation
                        dr::masked(weight, !count_direct_medium) *= mis_weight(last_scatter_direction_pdf, emitter_pdf);
                    }
                    dr::masked(result, active_medium_e) += weight * throughput * radiance;
                }

                if (dr::any_or<true>(act_null_scatter)) {
                    dr::masked(throughput, act_null_scatter) *= index_spectrum(weight_null, channel) * mei.sigma_n;

                    // Move the ray along
                    dr::masked(ray.o, act_null_scatter) = mei.p;
                    dr::masked(si.t, act_null_scatter)  = si.t - mei.t;
                }

                dr::masked(depth, act_medium_scatter) += 1;
                dr::masked(last_scatter_event, act_medium_scatter) = mei;

                // Don't estimate lighting if we exceeded number of bounces
                active &= depth < (uint32_t) m_max_depth;
                act_medium_scatter &= active;

                if (dr::any_or<true>(act_medium_scatter)) {
                    dr::masked(throughput, act_medium_scatter) *= index_spectrum(weight_scatter, channel) * mei.sigma_s;

                    PhaseFunctionContext phase_ctx(sampler);
                    auto phase = mei.medium->phase_function();

                    // --------------------- Emitter sampling ---------------------
                    if (m_use_emitter_sampling) {
                        Mask sample_emitters = mei.medium->use_emitter_sampling();
                        valid_ray |= act_medium_scatter;
                        specular_chain &= !act_medium_scatter;
                        specular_chain |= act_medium_scatter && !sample_emitters;

                        Mask active_e = act_medium_scatter && sample_emitters;
                        if (dr::any_or<true>(active_e)) {
                            auto [emitted, ds] = sample_emitter(mei, scene, sampler, medium, channel, active_e);
                            auto [phase_val, phase_pdf] = phase->eval_pdf(phase_ctx, mei, ds.d, active_e);
                            auto weight = dr::select(
                                m_use_uni_sampling,
                                mis_weight(ds.pdf, dr::select(ds.delta, 0.f, phase_pdf)),
                                1.0f
                            );
                            dr::masked(result, active_e) += weight * throughput * phase_val * emitted;
                        }
                    }

                    // ------------------ Phase function sampling -----------------
                    dr::masked(phase, !act_medium_scatter) = nullptr;
                    auto [wo, phase_weight, phase_pdf]     = phase->sample(
                        phase_ctx, mei, sampler->next_1d(act_medium_scatter),
                        sampler->next_2d(act_medium_scatter),
                        act_medium_scatter);
                    act_medium_scatter &= phase_pdf > 0.f;
                    Ray3f new_ray                       = mei.spawn_ray(wo);
                    dr::masked(ray, act_medium_scatter) = new_ray;
                    needs_intersection |= act_medium_scatter;
                    dr::masked(last_scatter_direction_pdf, act_medium_scatter) = phase_pdf;
                    dr::masked(throughput, act_medium_scatter) *= phase_weight;
                }
            }

            // --------------------- Surface Interactions ---------------------
            active_surface |= escaped_medium;
            Mask intersect = active_surface && needs_intersection;
            if (dr::any_or<true>(intersect))
                dr::masked(si, intersect) = scene->ray_intersect(ray, intersect);

            if (dr::any_or<true>(active_surface)) {
                // ---------------- Intersection with emitters ----------------
                Mask ray_from_camera = active_surface && dr::eq(depth, 0u);
                Mask count_direct = ray_from_camera || specular_chain;
                EmitterPtr emitter = si.emitter(scene);
                Mask active_e = active_surface && dr::neq(emitter, nullptr)
                                && !(dr::eq(depth, 0u) && m_hide_emitters)
                                && !has_flag(emitter->flags(), EmitterFlags::Medium); // Ignore any medium emitters as this simply looks at surface emitters
                if (dr::any_or<true>(active_e)) {
                    Float emitter_pdf = (m_use_emitter_sampling ? 1.0f : 0.0f);
                    if (m_use_emitter_sampling && dr::any_or<true>(active_e && !count_direct)) {
                        // Get the PDF of sampling this emitter using next event estimation
                        DirectionSample3f ds(scene, si, last_scatter_event);
                        dr::masked(emitter_pdf, active_e && !count_direct) = scene->pdf_emitter_direction(last_scatter_event, ds, active_e);
                    }

                    Spectrum emitted = emitter->eval(si, active_e);
                    Spectrum contrib = dr::select(count_direct, throughput * emitted,
                                                  throughput * mis_weight(last_scatter_direction_pdf, emitter_pdf) * emitted);
                    dr::masked(result, active_e) += contrib;
                }
            }
            active_surface &= si.is_valid();
            if (dr::any_or<true>(active_surface)) {
                BSDFContext ctx;
                BSDFPtr bsdf  = si.bsdf(ray);

                // --------------------- Emitter sampling ---------------------
                if (m_use_emitter_sampling) {
                    Mask active_e = active_surface && has_flag(bsdf->flags(), BSDFFlags::Smooth) && (depth + 1 < (uint32_t) m_max_depth);

                    if (likely(dr::any_or<true>(active_e))) {
                        auto [emitted, ds] = sample_emitter(si, scene, sampler, medium, channel, active_e);

                        // Query the BSDF for that emitter-sampled direction
                        Vector3f wo       = si.to_local(ds.d);
                        Spectrum bsdf_val = bsdf->eval(ctx, si, wo, active_e);
                        bsdf_val = si.to_world_mueller(bsdf_val, -wo, si.wi);

                        // Determine probability of having sampled that same
                        // direction using BSDF sampling.
                        Float bsdf_pdf = bsdf->pdf(ctx, si, wo, active_e);
                        result[active_e] += throughput * bsdf_val * mis_weight(ds.pdf, dr::select(ds.delta, 0.f, bsdf_pdf)) * emitted;
                    }
                }

                // ----------------------- BSDF sampling ----------------------
                auto [bs, bsdf_val] = bsdf->sample(ctx, si, sampler->next_1d(active_surface),
                                                   sampler->next_2d(active_surface), active_surface);
                bsdf_val = si.to_world_mueller(bsdf_val, -bs.wo, si.wi);

                dr::masked(throughput, active_surface) *= bsdf_val;
                dr::masked(eta, active_surface) *= bs.eta;

                Ray3f bsdf_ray                  = si.spawn_ray(si.to_world(bs.wo));
                dr::masked(ray, active_surface) = bsdf_ray;
                needs_intersection |= active_surface;

                Mask non_null_bsdf = active_surface && !has_flag(bs.sampled_type, BSDFFlags::Null);
                dr::masked(depth, non_null_bsdf) += 1;

                // update the last scatter PDF event if we encountered a non-null scatter event
                dr::masked(last_scatter_event, non_null_bsdf) = si;
                dr::masked(last_scatter_direction_pdf, non_null_bsdf) = bs.pdf;

                valid_ray |= non_null_bsdf;
                specular_chain |= non_null_bsdf && has_flag(bs.sampled_type, BSDFFlags::Delta);
                specular_chain &= !(active_surface && has_flag(bs.sampled_type, BSDFFlags::Smooth));
                Mask has_medium_trans                = active_surface && si.is_medium_transition();
                dr::masked(medium, has_medium_trans) = si.target_medium(ray.d);
            }
            active &= (active_surface | active_medium);
        }
        return { result, valid_ray };
    }


    /// Samples an emitter in the scene and evaluates its attenuated contribution
    template <typename Interaction>
    std::tuple<Spectrum, DirectionSample3f>
    sample_emitter(const Interaction &ref_interaction, const Scene *scene,
                   Sampler *sampler, MediumPtr medium,
                   UInt32 channel, Mask active) const {
        Spectrum transmittance(1.0f);

        /// We conservatively assume that there are volume emitters in the scene and sample 3d points instead of 2d
        /// This leads to some inefficiencies due to the fact that an extra random number per is generated and unused.
        auto [ds, emitter_val] = scene->sample_emitter_direction(ref_interaction, sampler->next_3d(active), false, active);
        dr::masked(emitter_val, dr::eq(ds.pdf, 0.f)) = 0.f;
        active &= dr::neq(ds.pdf, 0.f);

        Mask is_medium_emitter = active && has_flag(ds.emitter->flags(), EmitterFlags::Medium);
        dr::masked(emitter_val, is_medium_emitter) = 0.0f;

        if (dr::none_or<false>(active)) {
            return { emitter_val, ds };
        }

        Ray3f ray = ref_interaction.spawn_ray_to(ds.p);
        Float max_dist = ray.maxt;

        // Potentially escaping the medium if this is the current medium's boundary
        if constexpr (std::is_convertible_v<Interaction, SurfaceInteraction3f>)
            dr::masked(medium, ref_interaction.is_medium_transition()) = ref_interaction.target_medium(ray.d);

        Float total_dist = 0.f;
        auto si = dr::zeros<SurfaceInteraction3f>();
        auto mei = dr::zeros<MediumInteraction3f>();
        Mask needs_intersection = true;

        dr::Loop<Mask> loop("Volpath integrator emitter sampling");
        loop.put(active, ray, total_dist, needs_intersection, medium, si,
                 mei, transmittance, emitter_val);
        sampler->loop_put(loop);
        loop.init();
        while (loop(active)) {
            Float remaining_dist = max_dist - total_dist;
            ray.maxt = remaining_dist;
            active &= remaining_dist > 0.f;
            if (dr::none_or<false>(active))
                break;

            Mask escaped_medium = false;
            Mask active_medium  = active && dr::neq(medium, nullptr);
            Mask active_surface = active && !active_medium;

            if (dr::any_or<true>(active_medium)) {
                dr::masked(mei, active_medium) = medium->sample_interaction(ray, sampler->next_1d(active_medium), channel, active_medium);
                dr::masked(ray.maxt, active_medium && medium->is_homogeneous() && mei.is_valid()) = dr::minimum(mei.t, remaining_dist);
                Mask intersect = needs_intersection && active_medium;
                if (dr::any_or<true>(intersect))
                    dr::masked(si, intersect) = scene->ray_intersect(ray, intersect);

                dr::masked(mei.t, active_medium && (si.t < mei.t)) = dr::Infinity<Float>;
                needs_intersection &= !active_medium;

                Mask is_spectral = medium->has_spectral_extinction() && active_medium;
                Mask not_spectral = !is_spectral && active_medium;
                if (dr::any_or<true>(is_spectral)) {
                    Float t      = dr::minimum(remaining_dist, dr::minimum(mei.t, si.t)) - mei.mint;
                    UnpolarizedSpectrum tr  = dr::exp(-t * mei.combined_extinction);
                    UnpolarizedSpectrum free_flight_pdf = dr::select(si.t < mei.t || mei.t > remaining_dist, tr, tr * mei.combined_extinction);
                    Float tr_pdf = index_spectrum(free_flight_pdf, channel);
                    dr::masked(transmittance, is_spectral) *= dr::select(tr_pdf > 0.f, tr / tr_pdf, 0.f);
                }

                // Handle exceeding the maximum distance by medium sampling
                dr::masked(total_dist, active_medium && (mei.t > remaining_dist) && mei.is_valid()) = ds.dist;
                dr::masked(mei.t, active_medium && (mei.t > remaining_dist)) = dr::Infinity<Float>;

                escaped_medium = active_medium && !mei.is_valid();
                active_medium &= mei.is_valid();

                EmitterPtr medium_em = mei.emitter(active_medium);
                Mask is_sampled_medium = active_medium && dr::eq(medium_em, ds.emitter) && is_medium_emitter;
                if (dr::any_or<true>(is_sampled_medium)) {
                    PositionSample3f ps(mei);
                    auto radiance = medium->get_radiance(mei, active_medium);
                    dr::masked(emitter_val, is_sampled_medium) += transmittance * radiance * dr::rcp(ds.pdf);
                }

                dr::masked(mei, escaped_medium) = dr::zeros<MediumInteraction3f>();

                dr::masked(total_dist, active_medium) += mei.t;

                is_spectral  &= active_medium;
                not_spectral &= active_medium;

                if (dr::any_or<true>(active_medium)) {
                    dr::masked(ray.o, active_medium) = mei.p;
                    dr::masked(si.t, active_medium)  = si.t - mei.t;

                    if (dr::any_or<true>(is_spectral))
                        dr::masked(transmittance, is_spectral) *= mei.sigma_n;
                    if (dr::any_or<true>(not_spectral))
                        dr::masked(transmittance, not_spectral) *= mei.sigma_n / mei.combined_extinction;
                }
            }

            // Handle interactions with surfaces
            Mask intersect = active_surface && needs_intersection;
            if (dr::any_or<true>(intersect))
                dr::masked(si, intersect) = scene->ray_intersect(ray, intersect);
            needs_intersection &= !intersect;
            active_surface |= escaped_medium;
            dr::masked(total_dist, active_surface) += si.t;

            active_surface &= si.is_valid() && active && !active_medium;
            if (dr::any_or<true>(active_surface)) {
                auto bsdf         = si.bsdf(ray);
                Spectrum bsdf_val = bsdf->eval_null_transmission(si, active_surface);
                bsdf_val = si.to_world_mueller(bsdf_val, si.wi, si.wi);
                dr::masked(transmittance, active_surface) *= bsdf_val;
            }

            // Update the ray with new origin & t parameter
            dr::masked(ray, active_surface) = si.spawn_ray(ray.d);
            ray.maxt = remaining_dist;
            needs_intersection |= active_surface;

            // Continue tracing through scene if non-zero weights exist
            active &= (active_medium || active_surface) && dr::any(dr::neq(unpolarized_spectrum(transmittance), 0.f));

            // If a medium transition is taking place: Update the medium pointer
            Mask has_medium_trans = active_surface && si.is_medium_transition();
            if (dr::any_or<true>(has_medium_trans)) {
                dr::masked(medium, has_medium_trans) = si.target_medium(ray.d);
            }
        }

        return {dr::select(is_medium_emitter, emitter_val, emitter_val * transmittance), ds };
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
        return dr::select(dr::isfinite(w), w, 0.f);
    };

    MI_DECLARE_CLASS()
protected:
    bool m_use_emitter_sampling, m_use_uni_sampling;
};

MI_IMPLEMENT_CLASS_VARIANT(VolumetricPathIntegrator, MonteCarloIntegrator);
MI_EXPORT_PLUGIN(VolumetricPathIntegrator, "Volumetric Path Tracer integrator");
NAMESPACE_END(mitsuba)
