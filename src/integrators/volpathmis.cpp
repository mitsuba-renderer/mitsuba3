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

.. warning:: This integrator does not support forward-mode differentiation.

.. tabs::
    .. code-tab::  xml

        <integrator type="volpathmis">
            <integer name="max_depth" value="8"/>
        </integrator>

    .. code-tab:: python

        'type': 'volpathmis',
        'max_depth': 8

*/
template <typename Float, typename Spectrum>
class VolumetricMisPathIntegrator final : public MonteCarloIntegrator<Float, Spectrum> {

public:
    MI_IMPORT_BASE(MonteCarloIntegrator, m_max_depth, m_rr_depth, m_hide_emitters)
    MI_IMPORT_TYPES(Scene, Sampler, Emitter, EmitterPtr, BSDF, BSDFPtr,
                     Medium, MediumPtr, PhaseFunctionContext)

    VolumetricMisPathIntegrator(const Properties &props) : Base(props) {
        m_use_spectral_mis = props.get<bool>("use_spectral_mis", true);
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
    MI_DECLARE_CLASS()

protected:
    Properties m_props;
    bool m_use_spectral_mis;
};

template <typename Float, typename Spectrum, bool SpectralMis>
class VolpathMisIntegratorImpl final : public MonteCarloIntegrator<Float, Spectrum> {

public:
    MI_IMPORT_BASE(MonteCarloIntegrator, m_max_depth, m_rr_depth, m_hide_emitters)
    MI_IMPORT_TYPES(Scene, Sampler, Emitter, EmitterPtr, BSDF, BSDFPtr,
                     Medium, MediumPtr, PhaseFunctionContext)

    using WeightMatrix =
        std::conditional_t<SpectralMis, dr::Matrix<Float, dr::size_v<UnpolarizedSpectrum>>,
                           UnpolarizedSpectrum>;

    VolpathMisIntegratorImpl(const Properties &props) : Base(props) {}

    MI_INLINE
    Float index_spectrum(const UnpolarizedSpectrum &spec, const UInt32 &idx) const {
        Float m = spec[0];
        if constexpr (is_rgb_v<Spectrum>) { // Handle RGB rendering
            dr::masked(m, idx == 1u) = spec[1];
            dr::masked(m, idx == 2u) = spec[2];
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
        if constexpr (is_polarized_v<Spectrum>) {
            Throw("This integrator currently does not support polarized mode!");
        }

        // If there is an environment emitter and emitters are visible: all rays will be valid
        // Otherwise, it will depend on whether a valid interaction is sampled
        Mask valid_ray = !m_hide_emitters && (scene->environment() != nullptr);

        // For now, don't use ray differentials
        Ray3f ray = ray_;

        // Tracks radiance scaling due to index of refraction changes
        Float eta(1.f);

        Spectrum result(0.f);

        MediumPtr medium = initial_medium;
        MediumInteraction3f mei = dr::zeros<MediumInteraction3f>();

        Mask specular_chain = active && !m_hide_emitters;
        UInt32 depth = 0;
        WeightMatrix p_over_f = dr::full<WeightMatrix>(1.f);
        WeightMatrix p_over_f_nee = dr::full<WeightMatrix>(1.f);

        UInt32 channel = 0;
        if (is_rgb_v<Spectrum>) {
            uint32_t n_channels = (uint32_t) dr::size_v<Spectrum>;
            channel = (UInt32) dr::minimum(sampler->next_1d(active) * n_channels, n_channels - 1);
        }

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

        Mask needs_intersection = true, last_event_was_null = false;
        Interaction3f last_scatter_event = dr::zeros<Interaction3f>();

        struct LoopState {
            Mask active;
            UInt32 depth;
            Ray3f ray;
            WeightMatrix p_over_f;
            WeightMatrix p_over_f_nee;
            Spectrum result;
            SurfaceInteraction3f si;
            MediumInteraction3f mei;
            MediumPtr medium;
            Float eta;
            Interaction3f last_scatter_event;
            Mask last_event_was_null;
            Mask needs_intersection;
            Mask specular_chain;
            Mask valid_ray;
            Sampler* sampler;

            DRJIT_STRUCT(LoopState, active, depth, ray, p_over_f, \
                p_over_f_nee, result, si, mei, medium, eta, last_scatter_event, \
                last_event_was_null, needs_intersection, specular_chain, \
                valid_ray, sampler)
        } ls = {
            active,
            depth,
            ray,
            p_over_f,
            p_over_f_nee,
            result,
            si,
            mei,
            medium,
            eta,
            last_scatter_event,
            last_event_was_null,
            needs_intersection,
            specular_chain,
            valid_ray,
            sampler
        };

        /* Set up a Dr.Jit loop (optimizes away to a normal loop in scalar mode,
           generates wavefront or megakernel renderer based on configuration).
           Register everything that changes as part of the loop here */
        dr::tie(ls) = dr::while_loop(dr::make_tuple(ls),
            [](const LoopState& ls) { return ls.active; },
            [this, scene, channel](LoopState& ls) {

            Mask& active = ls.active;
            UInt32& depth = ls.depth;
            Ray3f& ray = ls.ray;
            WeightMatrix& p_over_f = ls.p_over_f;
            WeightMatrix& p_over_f_nee = ls.p_over_f_nee;
            Spectrum& result = ls.result;
            SurfaceInteraction3f& si = ls.si;
            MediumInteraction3f& mei = ls.mei;
            MediumPtr& medium = ls.medium;
            Float& eta = ls.eta;
            Interaction3f& last_scatter_event = ls.last_scatter_event;
            Mask& last_event_was_null = ls.last_event_was_null;
            Mask& needs_intersection = ls.needs_intersection;
            Mask& specular_chain = ls.specular_chain;
            Mask& valid_ray = ls.valid_ray;
            Sampler* sampler = ls.sampler;

            // ----------------- Handle termination of paths ------------------

            // Russian roulette: try to keep path weights equal to one, while accounting for the
            // solid angle compression at refractive index boundaries. Stop with at least some
            // probability to avoid  getting stuck (e.g. due to total internal reflection)
            Spectrum mis_throughput = mis_weight(p_over_f);
            Float q = dr::minimum(dr::max(unpolarized_spectrum(mis_throughput)) * dr::square(eta), .95f);
            Mask perform_rr = active && !last_event_was_null && (depth > (uint32_t) m_rr_depth);
            active &= !(sampler->next_1d(active) >= q && perform_rr);
            update_weights(p_over_f, dr::detach(q), 1.0f, channel, perform_rr);

            last_event_was_null = false;

            active &= depth < (uint32_t) m_max_depth;
            active &= dr::any(unpolarized_spectrum(mis_weight(p_over_f)) != 0.f);
            if (dr::none_or<false>(active))
                return;

            // ----------------------- Sampling the RTE -----------------------
            Mask active_medium  = active && (medium != nullptr);
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
                    auto [tr, free_flight_pdf] = medium->transmittance_eval_pdf(mei, si, is_spectral);
                    update_weights(p_over_f, free_flight_pdf, tr, channel, is_spectral);
                    update_weights(p_over_f_nee, free_flight_pdf, tr, channel, is_spectral);
                }
                escaped_medium = active_medium && !mei.is_valid();
                active_medium &= mei.is_valid();
                is_spectral &= active_medium;
                not_spectral &= active_medium;
            }

            if (dr::any_or<true>(active_medium)) {
                Mask null_scatter = sampler->next_1d(active_medium) >= index_spectrum(mei.sigma_t, channel) / index_spectrum(mei.combined_extinction, channel);
                act_null_scatter |= null_scatter && active_medium;
                act_medium_scatter |= !act_null_scatter && active_medium;
                last_event_was_null = act_null_scatter;

                // Count this as a bounce
                dr::masked(depth, act_medium_scatter) += 1;
                dr::masked(last_scatter_event, act_medium_scatter) = mei;
                Mask sample_emitters = mei.medium->use_emitter_sampling();

                active &= depth < (uint32_t) m_max_depth;
                act_medium_scatter &= active;
                specular_chain &= !act_medium_scatter;
                specular_chain |= act_medium_scatter && !sample_emitters;

                if (dr::any_or<true>(act_null_scatter)) {
                    if (dr::any_or<true>(is_spectral)) {
                        update_weights(p_over_f, mei.sigma_n / mei.combined_extinction, mei.sigma_n, channel, is_spectral && act_null_scatter);
                        update_weights(p_over_f_nee, 1.0f, mei.sigma_n, channel, is_spectral && act_null_scatter);
                    }
                    if (dr::any_or<true>(not_spectral)) {
                       update_weights(p_over_f, mei.sigma_n, mei.sigma_n, channel, not_spectral && act_null_scatter);
                       update_weights(p_over_f_nee, 1.0f, mei.sigma_n / mei.combined_extinction, channel, not_spectral && act_null_scatter);
                    }

                    dr::masked(ray.o, act_null_scatter) = mei.p;
                    dr::masked(si.t, act_null_scatter) = si.t - mei.t;
                }

                if (dr::any_or<true>(act_medium_scatter)) {
                    if (dr::any_or<true>(is_spectral))
                        update_weights(p_over_f, mei.sigma_t / mei.combined_extinction, mei.sigma_s, channel, is_spectral && act_medium_scatter);
                    if (dr::any_or<true>(not_spectral))
                        update_weights(p_over_f, mei.sigma_t, mei.sigma_s, channel, not_spectral && act_medium_scatter);

                    PhaseFunctionContext phase_ctx(sampler);
                    auto phase = mei.medium->phase_function();

                    // --------------------- Emitter sampling ---------------------
                    valid_ray |= act_medium_scatter;
                    Mask active_e = act_medium_scatter && sample_emitters;
                    if (dr::any_or<true>(active_e)) {
                        auto [p_over_f_nee_end, p_over_f_end, emitted, ds] =
                            sample_emitter(mei, scene, sampler, medium, p_over_f,
                                           channel, active_e);
                        auto [phase_val, phase_pdf] = phase->eval_pdf(phase_ctx, mei, ds.d, active_e);

                        update_weights(p_over_f_nee_end, 1.0f, unpolarized_spectrum(phase_val), channel, active_e);
                        update_weights(p_over_f_end, dr::select(ds.delta, 0.f, phase_pdf), unpolarized_spectrum(phase_val), channel, active_e);
                        dr::masked(result, active_e) += mis_weight(p_over_f_nee_end, p_over_f_end) * emitted;
                    }

                    // In a real interaction: reset p_over_f_nee
                    dr::masked(p_over_f_nee, act_medium_scatter) = p_over_f;

                    // ------------------ Phase function sampling -----------------
                    dr::masked(phase, !act_medium_scatter) = nullptr;
                    auto [wo, phase_weight, phase_pdf] = phase->sample(phase_ctx, mei,
                        sampler->next_1d(act_medium_scatter),
                        sampler->next_2d(act_medium_scatter),
                        act_medium_scatter);
                    Ray3f new_ray  = mei.spawn_ray(wo);
                    dr::masked(ray, act_medium_scatter) = new_ray;
                    needs_intersection |= act_medium_scatter;

                    update_weights(p_over_f, phase_pdf, unpolarized_spectrum(phase_weight * phase_pdf), channel, act_medium_scatter);
                    update_weights(p_over_f_nee, 1.f, unpolarized_spectrum(phase_weight * phase_pdf), channel, act_medium_scatter);
                }
            }


            // --------------------- Surface Interactions ---------------------
            active_surface |= escaped_medium;
            Mask intersect = active_surface && needs_intersection;
            if (dr::any_or<true>(intersect))
                dr::masked(si, intersect) = scene->ray_intersect(ray, intersect);


            if (dr::any_or<true>(active_surface)) {
                // ---------------- Intersection with emitters ----------------
                Mask ray_from_camera = active_surface && (depth == 0u);
                Mask count_direct = ray_from_camera || specular_chain;
                EmitterPtr emitter = si.emitter(scene);
                Mask active_e = active_surface && (emitter != nullptr) && !((depth == 0u) && m_hide_emitters);
                if (dr::any_or<true>(active_e)) {
                    if (dr::any_or<true>(active_e && !count_direct)) {
                        // Get the PDF of sampling this emitter using next event estimation
                        DirectionSample3f ds(scene, si, last_scatter_event);
                        Float emitter_pdf = scene->pdf_emitter_direction(last_scatter_event, ds, active_e);
                        update_weights(p_over_f_nee, emitter_pdf, 1.f, channel, active_e);
                    }
                    Spectrum emitted = emitter->eval(si, active_e);
                    Spectrum contrib = dr::select(count_direct, mis_weight(p_over_f) * emitted,
                                                            mis_weight(p_over_f, p_over_f_nee) * emitted);
                    dr::masked(result, active_e) += contrib;
                }
            }

            active_surface &= si.is_valid();
            if (dr::any_or<true>(active_surface)) {

                // --------------------- Emitter sampling ---------------------
                BSDFContext ctx;
                BSDFPtr bsdf  = si.bsdf(ray);
                Mask active_e = active_surface && has_flag(bsdf->flags(), BSDFFlags::Smooth) && (depth + 1 < (uint32_t) m_max_depth);
                if (likely(dr::any_or<true>(active_e))) {
                    auto [p_over_f_nee_end, p_over_f_end, emitted, ds] = sample_emitter(si, scene, sampler, medium, p_over_f, channel, active_e);
                    Vector3f wo_local       = si.to_local(ds.d);
                    auto [bsdf_val, bsdf_pdf] = bsdf->eval_pdf(ctx, si, wo_local, active_e);
                    update_weights(p_over_f_nee_end, 1.0f, unpolarized_spectrum(bsdf_val), channel, active_e);
                    update_weights(p_over_f_end, dr::select(ds.delta, 0.f, bsdf_pdf), unpolarized_spectrum(bsdf_val), channel, active_e);
                    dr::masked(result, active_e) += mis_weight(p_over_f_nee_end, p_over_f_end) * emitted;
                }

                // ----------------------- BSDF sampling ----------------------
                auto [bs, bsdf_weight] = bsdf->sample(ctx, si, sampler->next_1d(active_surface),
                                                   sampler->next_2d(active_surface), active_surface);
                Mask invalid_bsdf_sample = active_surface && (bs.pdf == 0.f);
                active_surface &= bs.pdf > 0.f;
                dr::masked(eta, active_surface) *= bs.eta;

                Ray3f bsdf_ray                  = si.spawn_ray(si.to_world(bs.wo));
                dr::masked(ray, active_surface) = bsdf_ray;
                needs_intersection |= active_surface;

                Mask non_null_bsdf = active_surface && !has_flag(bs.sampled_type, BSDFFlags::Null);
                valid_ray |= non_null_bsdf || invalid_bsdf_sample;
                specular_chain |= non_null_bsdf && has_flag(bs.sampled_type, BSDFFlags::Delta);
                specular_chain &= !(active_surface && has_flag(bs.sampled_type, BSDFFlags::Smooth));
                dr::masked(depth, non_null_bsdf) += 1;
                dr::masked(last_scatter_event, non_null_bsdf) = si;

                // Update NEE weights only if the BSDF is not null
                dr::masked(p_over_f_nee, non_null_bsdf) = p_over_f;
                update_weights(p_over_f, bs.pdf, unpolarized_spectrum(bsdf_weight * bs.pdf), channel, active_surface);
                update_weights(p_over_f_nee, 1.f, unpolarized_spectrum(bsdf_weight * bs.pdf), channel, non_null_bsdf);

                Mask has_medium_trans            = active_surface && si.is_medium_transition();
                dr::masked(medium, has_medium_trans) = si.target_medium(ray.d);
            }
            active &= (active_surface | active_medium);
            },
            "Volpath MIS integrator");

        return { ls.result, ls.valid_ray };
    }

    template <typename Interaction>
    std::tuple<WeightMatrix, WeightMatrix, Spectrum, DirectionSample3f>
    sample_emitter(const Interaction &ref_interaction, const Scene *scene,
                   Sampler *sampler, MediumPtr medium,
                   const WeightMatrix &p_over_f, UInt32 channel,
                   Mask active) const {
        WeightMatrix p_over_f_nee = p_over_f, p_over_f_uni = p_over_f;

        auto [ds, emitter_sample_weight] = scene->sample_emitter_direction(ref_interaction, sampler->next_2d(active), false, active);
        Spectrum emitter_val = emitter_sample_weight * ds.pdf;
        dr::masked(emitter_val, ds.pdf == 0.f) = 0.f;
        active &= (ds.pdf != 0.f);
        update_weights(p_over_f_nee, ds.pdf, 1.0f, channel, active);

        if (dr::none_or<false>(active)) {
            return { p_over_f_nee, p_over_f_uni, emitter_val, ds};
        }

        Ray3f ray = ref_interaction.spawn_ray_to(ds.p);
        Float max_dist = ray.maxt;

        // Potentially escaping the medium if this is the current medium's boundary
        if constexpr (std::is_convertible_v<Interaction, SurfaceInteraction3f>)
            dr::masked(medium, ref_interaction.is_medium_transition()) =
                ref_interaction.target_medium(ray.d);

        Float total_dist = 0.f;
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

        Mask needs_intersection = true;
        DirectionSample3f dir_sample = ds;

        struct LoopState {
            Mask active;
            Ray3f ray;
            Float total_dist;
            Mask needs_intersection;
            MediumPtr medium;
            SurfaceInteraction3f si;
            WeightMatrix p_over_f_nee;
            WeightMatrix p_over_f_uni;
            DirectionSample3f dir_sample;
            Sampler* sampler;

            DRJIT_STRUCT(LoopState, active, ray, total_dist, \
                needs_intersection, medium, si, p_over_f_nee, p_over_f_uni, \
                dir_sample, sampler)
        } ls = {
            active,
            ray,
            total_dist,
            needs_intersection,
            medium,
            si,
            p_over_f_nee,
            p_over_f_uni,
            dir_sample,
            sampler
        };

        dr::tie(ls) = dr::while_loop(dr::make_tuple(ls),
            [](const LoopState& ls) { return dr::detach(ls.active); },
            [this, scene, channel, max_dist](LoopState& ls) {

            Mask& active = ls.active;
            Ray3f& ray = ls.ray;
            Float& total_dist = ls.total_dist;
            Mask& needs_intersection = ls.needs_intersection;
            MediumPtr& medium = ls.medium;
            SurfaceInteraction3f& si = ls.si;
            WeightMatrix& p_over_f_nee = ls.p_over_f_nee;
            WeightMatrix& p_over_f_uni = ls.p_over_f_uni;
            DirectionSample3f& dir_sample = ls.dir_sample;
            Sampler* sampler = ls.sampler;

            Float remaining_dist = max_dist - total_dist;
            ray.maxt = remaining_dist;
            active &= remaining_dist > 0.f;
            if (dr::none_or<false>(active))
                return;

            Mask escaped_medium = false;
            Mask active_medium  = active && (medium != nullptr);
            Mask active_surface = active && !active_medium;

            if (dr::any_or<true>(active_medium)) {
                auto mei = medium->sample_interaction(ray, sampler->next_1d(active_medium), channel, active_medium);
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
                    update_weights(p_over_f_nee, free_flight_pdf, tr, channel, is_spectral);
                    update_weights(p_over_f_uni, free_flight_pdf, tr, channel, is_spectral);
                }
                // Handle exceeding the maximum distance by medium sampling
                dr::masked(total_dist, active_medium && (mei.t > remaining_dist) && mei.is_valid()) = dir_sample.dist;
                dr::masked(mei.t, active_medium && (mei.t > remaining_dist)) = dr::Infinity<Float>;

                escaped_medium = active_medium && !mei.is_valid();
                active_medium &= mei.is_valid();
                is_spectral &= active_medium;
                not_spectral &= active_medium;

                dr::masked(total_dist, active_medium) += mei.t;

                if (dr::any_or<true>(active_medium)) {
                    dr::masked(ray.o, active_medium) = mei.p;
                    // Update si.t since we continue the ray into the same direction
                    dr::masked(si.t, active_medium) = si.t - mei.t;
                    if (dr::any_or<true>(is_spectral)) {
                        update_weights(p_over_f_nee, 1.f, mei.sigma_n, channel, is_spectral);
                        update_weights(p_over_f_uni, mei.sigma_n / mei.combined_extinction, mei.sigma_n, channel, is_spectral);
                    }
                    if (dr::any_or<true>(not_spectral)) {
                        update_weights(p_over_f_nee, 1.f, mei.sigma_n / mei.combined_extinction, channel, not_spectral);
                        update_weights(p_over_f_uni, mei.sigma_n, mei.sigma_n, channel, not_spectral);
                    }
                }
            }

            // Handle interactions with surfaces
            Mask intersect = active_surface && needs_intersection;
            if (dr::any_or<true>(intersect))
                dr::masked(si, intersect)    = scene->ray_intersect(ray, intersect);
            active_surface |= escaped_medium;
            dr::masked(total_dist, active_surface) += si.t;

            active_surface &= si.is_valid() && active && !active_medium;
            if (dr::any_or<true>(active_surface)) {
                auto bsdf         = si.bsdf(ray);
                Spectrum bsdf_val = bsdf->eval_null_transmission(si, active_surface);
                update_weights(p_over_f_nee, 1.0f, unpolarized_spectrum(bsdf_val), channel, active_surface);
                update_weights(p_over_f_uni, 1.0f, unpolarized_spectrum(bsdf_val), channel, active_surface);
            }

            dr::masked(ray, active_surface) = si.spawn_ray(ray.d);
            ray.maxt = remaining_dist;
            needs_intersection |= active_surface;

            // Continue tracing through scene if non-zero weights exist
            if constexpr (SpectralMis)
                active &= (active_medium || active_surface) && dr::any(mis_weight(p_over_f_uni) != 0.f);
            else
                active &= (active_medium || active_surface) &&
                          (dr::any(unpolarized_spectrum(p_over_f_uni) != 0.f) ||
                           dr::any(unpolarized_spectrum(p_over_f_nee) != 0.f));

            // If a medium transition is taking place: Update the medium pointer
            Mask has_medium_trans = active_surface && si.is_medium_transition();
            if (dr::any_or<true>(has_medium_trans)) {
                dr::masked(medium, has_medium_trans) = si.target_medium(ray.d);
            }
        },
        "Volpath MIS integrator emitter sampling");

        return { ls.p_over_f_nee, ls.p_over_f_uni, emitter_val, dir_sample};
    }

    MI_INLINE
    void update_weights(WeightMatrix &p_over_f,
                        const UnpolarizedSpectrum &p,
                        const UnpolarizedSpectrum &f,
                        UInt32 channel, Mask active) const {
        // For two spectra p and f, computes all the ratios of the individual
        // components and multiplies them to the current values in p_over_f
        if constexpr (SpectralMis) {
            DRJIT_MARK_USED(channel);
            for (size_t i = 0; i < dr::size_v<Spectrum>; ++i) {
                UnpolarizedSpectrum ratio = p / f.entry(i);
                ratio = dr::select(dr::isfinite(ratio), ratio, 0.f);
                ratio *= p_over_f[i];
                dr::masked(p_over_f[i], active) = dr::select(dr::isnan(ratio), 0.f, ratio);
            }
        } else {
            // If we don't do spectral MIS: We need to use a specific channel of the spectrum "p" as the PDF
            Float pdf = index_spectrum(p, channel);
            auto ratio = p_over_f * (pdf / f);
            dr::masked(p_over_f, active) = dr::select(dr::isfinite(ratio), ratio, 0.f);
        }
    }

    UnpolarizedSpectrum mis_weight(const WeightMatrix& p_over_f) const {
        if constexpr (SpectralMis) {
            constexpr size_t n = dr::size_v<Spectrum>;
            UnpolarizedSpectrum weight(0.0f);
            for (size_t i = 0; i < n; ++i) {
                Float sum = dr::sum(p_over_f[i]);
                weight[i] = dr::select(sum == 0.f, 0.0f, n / sum);
            }
            return weight;
        } else {
            Mask invalid = (min(dr::abs(p_over_f)) == 0.f);
            return dr::select(invalid, 0.f, 1.f / p_over_f);
        }
    }

    // returns MIS'd throughput/pdf of two full paths represented by p_over_f1 and p_over_f2
    UnpolarizedSpectrum mis_weight(const WeightMatrix& p_over_f1, const WeightMatrix& p_over_f2) const {
        UnpolarizedSpectrum weight(0.0f);
        if constexpr (SpectralMis) {
            constexpr size_t n = dr::size_v<Spectrum>;
            auto sum_matrix = p_over_f1 + p_over_f2;
            for (size_t i = 0; i < n; ++i) {
                Float sum = dr::sum(sum_matrix[i]);
                weight[i] = dr::select(sum == 0.f, 0.0f, n / sum);
            }
        } else {
            auto sum = p_over_f1 + p_over_f2;
            weight = dr::select(min(dr::abs(sum)) == 0.f, 0.0f, 1.f / sum);
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

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(VolumetricMisPathIntegrator, MonteCarloIntegrator);
MI_EXPORT_PLUGIN(VolumetricMisPathIntegrator, "Volumetric Path Tracer integrator");

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
