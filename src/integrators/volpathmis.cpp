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

// Forward declaration of specialized integrator
template <typename Float, typename Spectrum, bool SpectralMis>
class VolpathMisIntegratorImpl;

/**!

.. _integrator-volpathmis:

Volumetric path tracer with spectral MIS (:monosp:`volpathmis`)
---------------------------------------------------------------

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
of the radiative transfer equation. Its implementation performs MIS both for directional sampling
as well as free-flight distance sampling. In particular, this integrator is well suited
to render media with a spectrally varying extinction coefficient.
The implementation is based on the method proposed by Miller et al. :cite:`Miller19null`
and is only marginally slower than the :ref:`simple volumetric path tracer <integrator-volpath>`.

Similar to the simple volumetric path tracer, this integrator has special
support for index-matched transmission events.

*/
template <typename Float, typename Spectrum>
class VolumetricMisPathIntegrator final : public MonteCarloIntegrator<Float, Spectrum> {

public:
    MTS_IMPORT_BASE(MonteCarloIntegrator, m_max_depth, m_rr_depth, m_hide_emitters)
    MTS_IMPORT_TYPES(Scene, Sampler, Emitter, EmitterPtr, BSDF, BSDFPtr,
                     Medium, MediumPtr, PhaseFunctionContext)

    VolumetricMisPathIntegrator(const Properties &props) : Base(props) {
        m_use_spectral_mis = props.bool_("use_spectral_mis", true);
        m_props = props;
    }

    template <bool SpectralMis>
    using Impl = VolpathMisIntegratorImpl<Float, Spectrum, SpectralMis>;

    std::vector<ref<Object>> expand() const override {
        ref<Object> result;
        if (m_use_spectral_mis)
            result = (Object *) new Impl<true>(m_props);
        else
            result = (Object *) new Impl<false>(m_props);
        return { result };
    }
    MTS_DECLARE_CLASS()

protected:
    Properties m_props;
    bool m_use_spectral_mis;
};

template <typename Float, typename Spectrum, bool SpectralMis>
class VolpathMisIntegratorImpl final : public MonteCarloIntegrator<Float, Spectrum> {

public:
    MTS_IMPORT_BASE(MonteCarloIntegrator, m_max_depth, m_rr_depth, m_hide_emitters)
    MTS_IMPORT_TYPES(Scene, Sampler, Emitter, EmitterPtr, BSDF, BSDFPtr,
                     Medium, MediumPtr, PhaseFunctionContext)

    using WeightMatrix =
        std::conditional_t<SpectralMis, ek::Matrix<Float, ek::array_size_v<UnpolarizedSpectrum>>,
                           UnpolarizedSpectrum>;

    VolpathMisIntegratorImpl(const Properties &props) : Base(props) {}

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
        if constexpr (is_polarized_v<Spectrum>) {
            Throw("This integrator currently does not support polarized mode!");
        }

        // If there is an environment emitter and emitters are visible: all rays will be valid
        // Otherwise, it will depend on whether a valid interaction is sampled
        Mask valid_ray = !m_hide_emitters && ek::neq(scene->environment(), nullptr);

        // For now, don't use ray differentials
        Ray3f ray = ray_;

        // Tracks radiance scaling due to index of refraction changes
        Float eta(1.f);

        Spectrum result(0.f);

        MediumPtr medium = initial_medium;
        MediumInteraction3f mi = ek::zero<MediumInteraction3f>();

        Mask specular_chain = active && !m_hide_emitters;
        UInt32 depth = 0;
        WeightMatrix p_over_f = ek::full<WeightMatrix>(1.f);
        WeightMatrix p_over_f_nee = ek::full<WeightMatrix>(1.f);

        UInt32 channel = 0;
        if (is_rgb_v<Spectrum>) {
            uint32_t n_channels = (uint32_t) ek::array_size_v<Spectrum>;
            channel = (UInt32) ek::min(sampler->next_1d(active) * n_channels, n_channels - 1);
        }

        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();

        Mask needs_intersection = true, last_event_was_null = false;
        Interaction3f last_scatter_event = ek::zero<Interaction3f>();

        ek::Loop<Mask> loop("Volpath MIS integrator");
        loop.put(active, depth, ray, p_over_f, p_over_f_nee, result, si, mi,
                 medium, eta, last_scatter_event, needs_intersection,
                 specular_chain, valid_ray);
        sampler->loop_register(loop);
        loop.init();
        while (loop(ek::detach(active))) {
            // ----------------- Handle termination of paths ------------------

            // Russian roulette: try to keep path weights equal to one, while accounting for the
            // solid angle compression at refractive index boundaries. Stop with at least some
            // probability to avoid  getting stuck (e.g. due to total internal reflection)
            Spectrum mis_throughput = mis_weight(p_over_f);
            Float q = ek::min(ek::hmax(unpolarized_spectrum(mis_throughput)) * ek::sqr(eta), .95f);
            Mask perform_rr = active && !last_event_was_null && (depth > (uint32_t) m_rr_depth);
            active &= !(sampler->next_1d(active) >= q && perform_rr);
            update_weights(p_over_f, ek::detach(q), 1.0f, channel, perform_rr);

            last_event_was_null = false;

            active &= depth < (uint32_t) m_max_depth;
            active &= ek::any(ek::neq(unpolarized_spectrum(mis_weight(p_over_f)), 0.f));
            if (ek::none_or<false>(active))
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
                    update_weights(p_over_f, free_flight_pdf, tr, channel, is_spectral);
                    update_weights(p_over_f_nee, free_flight_pdf, tr, channel, is_spectral);
                }
                escaped_medium = active_medium && !mi.is_valid();
                active_medium &= mi.is_valid();
                is_spectral &= active_medium;
                not_spectral &= active_medium;
            }

            if (ek::any_or<true>(active_medium)) {
                Mask null_scatter = sampler->next_1d(active_medium) >= index_spectrum(mi.sigma_t, channel) / index_spectrum(mi.combined_extinction, channel);
                act_null_scatter |= null_scatter && active_medium;
                act_medium_scatter |= !act_null_scatter && active_medium;
                last_event_was_null = act_null_scatter;

                // Count this as a bounce
                ek::masked(depth, act_medium_scatter) += 1;
                ek::masked(last_scatter_event, act_medium_scatter) = mi;
                Mask sample_emitters = mi.medium->use_emitter_sampling();

                active &= depth < (uint32_t) m_max_depth;
                act_medium_scatter &= active;
                specular_chain &= !act_medium_scatter;
                specular_chain |= act_medium_scatter && !sample_emitters;

                if (ek::any_or<true>(act_null_scatter)) {
                    if (ek::any_or<true>(is_spectral)) {
                        update_weights(p_over_f, mi.sigma_n / mi.combined_extinction, mi.sigma_n, channel, is_spectral && act_null_scatter);
                        update_weights(p_over_f_nee, 1.0f, mi.sigma_n, channel, is_spectral && act_null_scatter);
                    }
                    if (ek::any_or<true>(not_spectral)) {
                       update_weights(p_over_f, mi.sigma_n, mi.sigma_n, channel, not_spectral && act_null_scatter);
                       update_weights(p_over_f_nee, 1.0f, mi.sigma_n / mi.combined_extinction, channel, not_spectral && act_null_scatter);
                    }

                    ek::masked(ray.o, act_null_scatter) = mi.p;
                    ek::masked(si.t, act_null_scatter) = si.t - mi.t;
                }

                if (ek::any_or<true>(act_medium_scatter)) {
                    if (ek::any_or<true>(is_spectral))
                        update_weights(p_over_f, mi.sigma_t / mi.combined_extinction, mi.sigma_s, channel, is_spectral && act_medium_scatter);
                    if (ek::any_or<true>(not_spectral))
                        update_weights(p_over_f, mi.sigma_t, mi.sigma_s, channel, not_spectral && act_medium_scatter);

                    PhaseFunctionContext phase_ctx(sampler);
                    auto phase = mi.medium->phase_function();

                    // --------------------- Emitter sampling ---------------------
                    valid_ray |= act_medium_scatter;
                    Mask active_e = act_medium_scatter && sample_emitters;
                    if (ek::any_or<true>(active_e)) {
                        auto [p_over_f_nee_end, p_over_f_end, emitted, ds] =
                            sample_emitter(mi, scene, sampler, medium, p_over_f,
                                           channel, active_e);
                        Float phase_val = phase->eval(phase_ctx, mi, ds.d, active_e);
                        update_weights(p_over_f_nee_end, 1.0f, phase_val, channel, active_e);
                        update_weights(p_over_f_end, ek::select(ds.delta, 0.f, phase_val), phase_val, channel, active_e);
                        ek::masked(result, active_e) += mis_weight(p_over_f_nee_end, p_over_f_end) * emitted;
                    }

                    // In a real interaction: reset p_over_f_nee
                    ek::masked(p_over_f_nee, act_medium_scatter) = p_over_f;

                    // ------------------ Phase function sampling -----------------
                    ek::masked(phase, !act_medium_scatter) = nullptr;
                    auto [wo, phase_pdf] = phase->sample(phase_ctx, mi,
                        sampler->next_1d(act_medium_scatter),
                        sampler->next_2d(act_medium_scatter),
                        act_medium_scatter);
                    Ray3f new_ray  = mi.spawn_ray(wo);
                    ek::masked(ray, act_medium_scatter) = new_ray;
                    needs_intersection |= act_medium_scatter;

                    update_weights(p_over_f, phase_pdf, phase_pdf, channel, act_medium_scatter);
                    update_weights(p_over_f_nee, 1.f, phase_pdf, channel, act_medium_scatter);
                }
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
                Mask active_e = active_surface && ek::neq(emitter, nullptr) && !(ek::eq(depth, 0u) && m_hide_emitters);
                if (ek::any_or<true>(active_e)) {
                    if (ek::any_or<true>(active_e && !count_direct)) {
                        // Get the PDF of sampling this emitter using next event estimation
                        DirectionSample3f ds(scene, si, last_scatter_event);
                        Float emitter_pdf = scene->pdf_emitter_direction(last_scatter_event, ds, active_e);
                        update_weights(p_over_f_nee, emitter_pdf, 1.f, channel, active_e);
                    }
                    Spectrum emitted = emitter->eval(si, active_e);
                    Spectrum contrib = ek::select(count_direct, mis_weight(p_over_f) * emitted,
                                                            mis_weight(p_over_f, p_over_f_nee) * emitted);
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
                    auto [p_over_f_nee_end, p_over_f_end, emitted, ds] = sample_emitter(si, scene, sampler, medium, p_over_f, channel, active_e);
                    Vector3f wo_local       = si.to_local(ds.d);
                    Spectrum bsdf_val = bsdf->eval(ctx, si, wo_local, active_e);
                    Float bsdf_pdf =  bsdf->pdf(ctx, si, wo_local, active_e);
                    update_weights(p_over_f_nee_end, 1.0f, unpolarized_spectrum(bsdf_val), channel, active_e);
                    update_weights(p_over_f_end, ek::select(ds.delta, 0.f, bsdf_pdf), unpolarized_spectrum(bsdf_val), channel, active_e);
                    ek::masked(result, active_e) += mis_weight(p_over_f_nee_end, p_over_f_end) * emitted;
                }

                // ----------------------- BSDF sampling ----------------------
                auto [bs, bsdf_weight] = bsdf->sample(ctx, si, sampler->next_1d(active_surface),
                                                   sampler->next_2d(active_surface), active_surface);
                Mask invalid_bsdf_sample = active_surface && ek::eq(bs.pdf, 0.f);
                active_surface &= bs.pdf > 0.f;
                ek::masked(eta, active_surface) *= bs.eta;

                Ray bsdf_ray                = si.spawn_ray(si.to_world(bs.wo));
                ek::masked(ray, active_surface) = bsdf_ray;
                needs_intersection |= active_surface;

                Mask non_null_bsdf = active_surface && !has_flag(bs.sampled_type, BSDFFlags::Null);
                valid_ray |= non_null_bsdf || invalid_bsdf_sample;
                specular_chain |= non_null_bsdf && has_flag(bs.sampled_type, BSDFFlags::Delta);
                specular_chain &= !(active_surface && has_flag(bs.sampled_type, BSDFFlags::Smooth));
                ek::masked(depth, non_null_bsdf) += 1;
                ek::masked(last_scatter_event, non_null_bsdf) = si;

                // Update NEE weights only if the BSDF is not null
                ek::masked(p_over_f_nee, non_null_bsdf) = p_over_f;
                update_weights(p_over_f, bs.pdf, unpolarized_spectrum(bsdf_weight * bs.pdf), channel, active_surface);
                update_weights(p_over_f_nee, 1.f, unpolarized_spectrum(bsdf_weight * bs.pdf), channel, non_null_bsdf);

                Mask has_medium_trans            = active_surface && si.is_medium_transition();
                ek::masked(medium, has_medium_trans) = si.target_medium(ray.d);
            }
            active &= (active_surface | active_medium);
        }

        return { result, valid_ray };
    }

    std::tuple<WeightMatrix, WeightMatrix, Spectrum, DirectionSample3f>
    sample_emitter(const Interaction3f &ref_interaction, const Scene *scene,
                   Sampler *sampler, MediumPtr medium,
                   const WeightMatrix &p_over_f, UInt32 channel,
                   Mask active) const {
        WeightMatrix p_over_f_nee = p_over_f, p_over_f_uni = p_over_f;

        auto [ds, emitter_sample_weight] = scene->sample_emitter_direction(ref_interaction, sampler->next_2d(active), false, active);
        Spectrum emitter_val = emitter_sample_weight * ds.pdf;
        ek::masked(emitter_val, ek::eq(ds.pdf, 0.f)) = 0.f;
        active &= ek::neq(ds.pdf, 0.f);
        update_weights(p_over_f_nee, ds.pdf, 1.0f, channel, active);

        if (ek::none_or<false>(active)) {
            return { p_over_f_nee, p_over_f_uni, emitter_val, ds};
        }

        Ray3f ray = ref_interaction.spawn_ray(ds.d);

        Float total_dist = 0.f;
        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();

        Mask needs_intersection = true;
        ek::Loop<Mask> loop("Volpath MIS integrator emitter sampling");
        loop.put(active, ray, total_dist, needs_intersection, medium, si,
                 p_over_f_nee, p_over_f_uni);
        sampler->loop_register(loop);
        loop.init();
        while (loop(ek::detach(active))) {

            Float remaining_dist = ds.dist * (1.f - math::ShadowEpsilon<Float>) - total_dist;
            ray.maxt = remaining_dist;
            active &= remaining_dist > 0.f;
            if (ek::none_or<false>(active))
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
                    update_weights(p_over_f_nee, free_flight_pdf, tr, channel, is_spectral);
                    update_weights(p_over_f_uni, free_flight_pdf, tr, channel, is_spectral);
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
                    ek::masked(ray.o, active_medium) = mi.p;
                    // Update si.t since we continue the ray into the same direction
                    ek::masked(si.t, active_medium) = si.t - mi.t;
                    if (ek::any_or<true>(is_spectral)) {
                        update_weights(p_over_f_nee, 1.f, mi.sigma_n, channel, is_spectral);
                        update_weights(p_over_f_uni, mi.sigma_n / mi.combined_extinction, mi.sigma_n, channel, is_spectral);
                    }
                    if (ek::any_or<true>(not_spectral)) {
                        update_weights(p_over_f_nee, 1.f, mi.sigma_n / mi.combined_extinction, channel, not_spectral);
                        update_weights(p_over_f_uni, mi.sigma_n, mi.sigma_n, channel, not_spectral);
                    }
                }
            }

            // Handle interactions with surfaces
            Mask intersect = active_surface && needs_intersection;
            if (ek::any_or<true>(intersect))
                ek::masked(si, intersect)    = scene->ray_intersect(ray, intersect);
            active_surface |= escaped_medium;
            ek::masked(total_dist, active_surface) += si.t;

            active_surface &= si.is_valid() && active && !active_medium;
            if (ek::any_or<true>(active_surface)) {
                auto bsdf         = si.bsdf(ray);
                Spectrum bsdf_val = bsdf->eval_null_transmission(si, active_surface);
                update_weights(p_over_f_nee, 1.0f, unpolarized_spectrum(bsdf_val), channel, active_surface);
                update_weights(p_over_f_uni, 1.0f, unpolarized_spectrum(bsdf_val), channel, active_surface);
            }

            ek::masked(ray, active_surface) = si.spawn_ray(ray.d);
            ray.maxt = remaining_dist;
            needs_intersection |= active_surface;

            // Continue tracing through scene if non-zero weights exist
            if constexpr (SpectralMis)
                active &= (active_medium || active_surface) && ek::any(ek::neq(mis_weight(p_over_f_uni), 0.f));
            else
                active &= (active_medium || active_surface) &&
                      (ek::any(ek::neq(unpolarized_spectrum(p_over_f_uni), 0.f)) || ek::any(ek::neq(unpolarized_spectrum(p_over_f_nee), 0.f)) );

            // If a medium transition is taking place: Update the medium pointer
            Mask has_medium_trans = active_surface && si.is_medium_transition();
            if (ek::any_or<true>(has_medium_trans)) {
                ek::masked(medium, has_medium_trans) = si.target_medium(ray.d);
            }
        }

        return { p_over_f_nee, p_over_f_uni, emitter_val, ds};
    }

    MTS_INLINE
    void update_weights(WeightMatrix &p_over_f,
                        const UnpolarizedSpectrum &p,
                        const UnpolarizedSpectrum &f,
                        UInt32 channel, Mask active) const {
        // For two spectra p and f, computes all the ratios of the individual
        // components and multiplies them to the current values in p_over_f
        if constexpr (SpectralMis) {
            ENOKI_MARK_USED(channel);
            for (size_t i = 0; i < ek::array_size_v<Spectrum>; ++i) {
                UnpolarizedSpectrum ratio = p / f.entry(i);
                ratio = ek::select(ek::isfinite(ratio), ratio, 0.f);
                ratio *= p_over_f[i];
                ek::masked(p_over_f[i], active) = ek::select(ek::neq(ratio, ratio), 0.f, ratio);
            }
        } else {
            // If we don't do spectral MIS: We need to use a specific channel of the spectrum "p" as the PDF
            Float pdf = index_spectrum(p, channel);
            auto ratio = p_over_f * (pdf / f);
            ek::masked(p_over_f, active) = ek::select(ek::isfinite(ratio), ratio, 0.f);
        }
    }

    UnpolarizedSpectrum mis_weight(const WeightMatrix& p_over_f) const {
        if constexpr (SpectralMis) {
            constexpr size_t n = ek::array_size_v<Spectrum>;
            UnpolarizedSpectrum weight(0.0f);
            for (size_t i = 0; i < n; ++i) {
                Float sum = ek::hsum(p_over_f[i]);
                weight[i] = ek::select(ek::eq(sum, 0.f), 0.0f, n / sum);
            }
            return weight;
        } else {
            Mask invalid = ek::eq(hmin(ek::abs(p_over_f)), 0.f);
            return ek::select(invalid, 0.f, 1.f / p_over_f);
        }
    }

    // returns MIS'd throughput/pdf of two full paths represented by p_over_f1 and p_over_f2
    UnpolarizedSpectrum mis_weight(const WeightMatrix& p_over_f1, const WeightMatrix& p_over_f2) const {
        UnpolarizedSpectrum weight(0.0f);
        if constexpr (SpectralMis) {
            constexpr size_t n = ek::array_size_v<Spectrum>;
            auto sum_matrix = p_over_f1 + p_over_f2;
            for (size_t i = 0; i < n; ++i) {
                Float sum = ek::hsum(sum_matrix[i]);
                weight[i] = ek::select(ek::eq(sum, 0.f), 0.0f, n / sum);
            }
        } else {
            auto sum = p_over_f1 + p_over_f2;
            weight = ek::select(ek::eq(hmin(ek::abs(sum)), 0.f), 0.0f, 1.f / sum);
        }
        return weight;
    }

    //! @}
    // =============================================================

    std::string to_string() const override {
        return tfm::format("VolumetricMisPathIntegrator[\n"
                           "  max_depth = %i,\n"
                           "  rr_depth = %i\n"
                           "]",
                           m_max_depth, m_rr_depth);
    }

    MTS_DECLARE_CLASS()
};

MTS_IMPLEMENT_CLASS_VARIANT(VolumetricMisPathIntegrator, MonteCarloIntegrator);
MTS_EXPORT_PLUGIN(VolumetricMisPathIntegrator, "Volumetric Path Tracer integrator");

NAMESPACE_BEGIN(detail)
template <bool SpectralMis>
constexpr const char * volpath_class_name() {
    if constexpr (SpectralMis) {
        return "Volpath_spectral_mis";
    } else {
        return "Volpath_no_spectral_mis";
    }
}
NAMESPACE_END(detail)

template <typename Float, typename Spectrum, bool SpectralMis>
Class *VolpathMisIntegratorImpl<Float, Spectrum, SpectralMis>::m_class
    = new Class(detail::volpath_class_name<SpectralMis>(), "MonteCarloIntegrator",
                ::mitsuba::detail::get_variant<Float, Spectrum>(), nullptr, nullptr);

template <typename Float, typename Spectrum, bool SpectralMis>
const Class* VolpathMisIntegratorImpl<Float, Spectrum, SpectralMis>::class_() const {
    return m_class;
}

NAMESPACE_END(mitsuba)
