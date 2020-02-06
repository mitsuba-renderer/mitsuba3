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
class VolumetricNullPathIntegrator : public MonteCarloIntegrator<Float, Spectrum> {

public:
    MTS_IMPORT_BASE(MonteCarloIntegrator, m_max_depth, m_rr_depth, m_hide_emitters)
    MTS_IMPORT_TYPES(Scene, Sampler, Emitter, EmitterPtr, BSDF, BSDFPtr,
                     Medium, MediumPtr, PhaseFunctionContext)

    using WeightMatrix = enoki::Array<Float, array_size_v<Spectrum> * array_size_v<Spectrum>>;

    VolumetricNullPathIntegrator(const Properties &props) : Base(props) {
        m_mis = true;
    }

    MTS_INLINE
    Float index_spectrum(const UnpolarizedSpectrum &spec, const UInt32 &idx) const {
        Float m = spec[0];
        if constexpr (is_rgb_v<Spectrum>) { // Handle RGB rendering
            masked(m, eq(idx, 1u)) = spec[1];
            masked(m, eq(idx, 2u)) = spec[2];
        }
        return m;
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

        Spectrum result(0.f);

        MediumPtr medium = nullptr;
        MediumInteraction3f mi;

        Mask specular_chain = active && !m_hide_emitters;
        UInt32 depth = 0;
        WeightMatrix p_over_f = zero<WeightMatrix>() + 1.f;
        WeightMatrix p_over_f_nee = zero<WeightMatrix>() + 1.f;

        UInt32 channel = 0;
        if (is_rgb_v<Spectrum>) {
            uint32_t n_channels = (uint32_t) array_size_v<Spectrum>;
            channel = min(sampler->next_1d(active) * n_channels, n_channels - 1);
        }

        for (int bounce = 0;; ++bounce) {
            // ----------------- Handle termination of paths ------------------

            // Russian roulette: try to keep path weights equal to one, while accounting for the
            // solid angle compression at refractive index boundaries. Stop with at least some
            // probability to avoid  getting stuck (e.g. due to total internal reflection)
            // Spectrum mis_throughput = mis_weight(p_over_f);
            // active &= any(neq(depolarize(mis_throughput), 0.f));
            // Float q = min(hmax(depolarize(mis_throughput)) * sqr(eta), .95f);
            // Mask perform_rr = (depth > (uint32_t) m_rr_depth);
            // active &= !(sampler->next_1d(active) >= q && perform_rr);
            // update_weights(p_over_f, detach(q), 1.0f, perform_rr);

            Mask exceeded_max_depth = depth >= (uint32_t) m_max_depth;
            active &= !exceeded_max_depth;
            active &= any(neq(depolarize(mis_weight(p_over_f)), 0.f));

            if (none(active))
                break;


            // ----------------------- Sampling the RTE -----------------------
            Mask active_medium  = active && neq(medium, nullptr);
            Mask active_surface = active && !active_medium;
            Mask act_null_scatter = false;
            Mask act_medium_scatter = false;
            Mask escaped_medium = false;
            SurfaceInteraction3f si_medium;
            Interaction3f last_scatter_event;

            if (any_or<true>(active_medium)) {
                Spectrum tr;
                std::tie(si_medium, mi, tr) = medium->sample_interaction(
                    scene, ray, sampler->next_1d(active_medium), channel, active_medium);
                Spectrum free_flight_pdf = select(active_medium && mi.is_valid(), mi.combined_extinction * tr, tr);
                update_weights(p_over_f, free_flight_pdf, tr, active_medium);
                update_weights(p_over_f_nee, free_flight_pdf, tr, active_medium);

                escaped_medium = active_medium && si_medium.is_valid() && !mi.is_valid();
                active_medium &= mi.is_valid();
            }

            if (any_or<true>(active_medium)) {
                Mask null_scatter = sampler->next_1d(active_medium) >= index_spectrum(mi.sigma_t, channel) / index_spectrum(mi.combined_extinction, channel);
                act_null_scatter |= null_scatter && active_medium;
                act_medium_scatter |= !act_null_scatter && active_medium;

                // Count this as a bounce
                masked(depth, act_medium_scatter) += 1;
                masked(last_scatter_event, act_medium_scatter) = mi;

                active &= depth < (uint32_t) m_max_depth;
                act_medium_scatter &= active;
                specular_chain = specular_chain && !act_medium_scatter;


                if (any_or<true>(act_null_scatter)) {
                    update_weights(p_over_f, mi.sigma_n / mi.combined_extinction, mi.sigma_n, act_null_scatter);
                    update_weights(p_over_f_nee, 1.0f, mi.sigma_n, act_null_scatter);

                    masked(ray.o, act_null_scatter) = mi.p;
                    masked(ray.mint, act_null_scatter) = 0.f;
                }

                if (any_or<true>(act_medium_scatter)) {
                    update_weights(p_over_f, mi.sigma_t / mi.combined_extinction, mi.sigma_s, act_medium_scatter);

                    PhaseFunctionContext phase_ctx(sampler);
                    auto phase = mi.medium->phase_function();

                    // --------------------- Emitter sampling ---------------------
                    Mask sample_emitters = mi.medium->use_emitter_sampling();
                    valid_ray |= act_medium_scatter;
                    Mask active_e = act_medium_scatter && sample_emitters;
                    if (any_or<true>(active_e)) {
                        auto [p_over_f_nee_end, p_over_f_end, emitted, wo] = sample_emitter(mi, true, scene, sampler, medium, p_over_f, channel, active_e);
                        Float phase_val = phase->eval(phase_ctx, mi, wo, active_e);
                        update_weights(p_over_f_nee_end, 1.0f, phase_val, active_e);
                        update_weights(p_over_f_end, phase_val, phase_val, active_e);
                        masked(result, active_e) += mis_weight(p_over_f_nee_end, p_over_f_end) * emitted;
                    }

                    // In a real interaction: reset p_over_f_nee
                    set_weights(p_over_f_nee, p_over_f, act_medium_scatter);

                    // ------------------ Phase function sampling -----------------
                    masked(phase, !act_medium_scatter) = nullptr;
                    auto [wo, phase_pdf] = phase->sample(phase_ctx, mi, sampler->next_2d(act_medium_scatter), act_medium_scatter);
                    Ray3f new_ray  = mi.spawn_ray(wo);
                    new_ray.mint = 0.0f;
                    masked(ray, act_medium_scatter) = new_ray;

                    update_weights(p_over_f, phase_pdf, phase_pdf, act_medium_scatter);
                    update_weights(p_over_f_nee, 1.f, phase_pdf, act_medium_scatter);
                }
            }


            // --------------------- Surface Interactions ---------------------
            SurfaceInteraction3f si     = scene->ray_intersect(ray, active_surface);
            masked(si, escaped_medium) = si_medium;
            active_surface |= escaped_medium;

            if (any_or<true>(active_surface)) {
                // ---------------- Intersection with emitters ----------------
                Mask ray_from_camera = active_surface && eq(depth, 0);
                Mask count_direct = ray_from_camera || specular_chain;
                EmitterPtr emitter = si.emitter(scene);
                Mask active_e = active_surface && neq(emitter, nullptr) && !(eq(depth, 0) && m_hide_emitters);
                if (any_or<true>(active_e)) {
                    if (any_or<true>(active_e && !count_direct)) {
                        // Get the PDF of sampling this emitter using next event estimation
                        DirectionSample3f ds(si, last_scatter_event);
                        ds.object         = emitter;
                        Float emitter_pdf = scene->pdf_emitter_direction(last_scatter_event, ds, active_e);
                        update_weights(p_over_f_nee, emitter_pdf, 1.f, active_e);
                    }
                    Spectrum emitted = emitter->eval(si, active_e);
                    Spectrum contrib = select(count_direct, mis_weight(p_over_f) * emitted,
                                                            mis_weight(p_over_f, p_over_f_nee) * emitted);
                    masked(result, active_e) += contrib;
                }
            }

            active_surface &= si.is_valid();
            if (any_or<true>(active_surface)) {

                // --------------------- Emitter sampling ---------------------
                BSDFContext ctx;
                BSDFPtr bsdf  = si.bsdf(ray);
                Mask active_e = active_surface && has_flag(bsdf->flags(), BSDFFlags::Smooth) && (depth + 1 < (uint32_t) m_max_depth);
                if (likely(any_or<true>(active_e))) {
                    auto [p_over_f_nee_end, p_over_f_end, emitted, wo] = sample_emitter(si, false, scene, sampler, medium, p_over_f, channel, active_e);
                    Vector3f wo_local       = si.to_local(wo);
                    Spectrum bsdf_val = bsdf->eval(ctx, si, wo_local, active_e);
                    Float bsdf_pdf =  bsdf->pdf(ctx, si, wo_local, active_e);
                    update_weights(p_over_f_nee_end, 1.0f, bsdf_val, active_e);
                    update_weights(p_over_f_end, bsdf_pdf, bsdf_val, active_e);
                    masked(result, active_e) += mis_weight(p_over_f_nee_end, p_over_f_end) * emitted;
                }

                // ----------------------- BSDF sampling ----------------------
                auto [bs, bsdf_weight] = bsdf->sample(ctx, si, sampler->next_1d(active_surface),
                                                   sampler->next_2d(active_surface), active_surface);
                active_surface &= bs.pdf > 0.f;
                masked(eta, active_surface) *= bs.eta;

                Ray bsdf_ray                = si.spawn_ray(si.to_world(bs.wo));
                masked(ray, active_surface) = bsdf_ray;

                Mask non_null_bsdf = active_surface && !has_flag(bs.sampled_type, BSDFFlags::Null);
                valid_ray |= non_null_bsdf;
                specular_chain |= non_null_bsdf && has_flag(bs.sampled_type, BSDFFlags::Delta);
                specular_chain &= !(active_surface && has_flag(bs.sampled_type, BSDFFlags::Smooth));
                masked(depth, non_null_bsdf) += 1;
                masked(last_scatter_event, non_null_bsdf) = si;

                // Update NEE weights only if the BSDF is not null
                set_weights(p_over_f_nee, p_over_f, non_null_bsdf);
                update_weights(p_over_f, bs.pdf, bsdf_weight * bs.pdf, active_surface);
                update_weights(p_over_f_nee, 1.f, bsdf_weight * bs.pdf, non_null_bsdf);

                Mask has_medium_trans            = si.is_valid() && si.is_medium_transition();
                masked(medium, has_medium_trans) = si.target_medium(ray.d);
            }
            active &= (active_surface | active_medium);
        }

        return { result, valid_ray };
    }


    std::tuple<WeightMatrix, WeightMatrix, Spectrum, Vector3f> sample_emitter(const Interaction3f &ref_interaction, Mask is_medium_interaction,
                const Scene *scene, Sampler *sampler,  MediumPtr medium, const WeightMatrix &p_over_f, UInt32 channel, Mask active) const {
        using EmitterPtr = replace_scalar_t<Float, const Emitter *>;

        auto [ds, em] = scene->sample_emitter_direction(ref_interaction, sampler->next_2d(active), false, active);
        Float dist = select(enoki::isfinite(ds.dist), ds.dist, -1.f);
        dist = ds.dist;
        active &= neq(ds.pdf, 0.f);

        if (none_or<false>(active)) {
            return {WeightMatrix(1.f), WeightMatrix(1.f), Spectrum(0.f), Vector3f(1.f,0.f,0.f)};
        }

        Ray3f ray = ref_interaction.spawn_ray(ds.d);
        masked(ray.mint, is_medium_interaction) = 0.f;

        Spectrum emitter_val(0.f);
        WeightMatrix p_over_f_nee = p_over_f, p_over_f_uni = p_over_f;
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
                    scene, ray, sampler->next_1d(active_medium), channel, active_medium);

                Spectrum free_flight_pdf = select(mi.is_valid(), mi.combined_extinction * tr, tr);
                update_weights(p_over_f_nee, free_flight_pdf, tr, active_medium);
                update_weights(p_over_f_uni, free_flight_pdf, tr, active_medium);

                escaped_medium = active_medium && si_medium.is_valid() && !mi.is_valid();
                active_medium &= mi.is_valid();
                masked(total_dist, active_medium) += mi.t;

                if (any_or<true>(active_medium)) {
                    masked(ray.o, active_medium)    = mi.p;
                    masked(ray.mint, active_medium) = 0.f;
                    update_weights(p_over_f_nee, 1.f, mi.sigma_n, active_medium);
                    update_weights(p_over_f_uni, mi.sigma_n / mi.combined_extinction, mi.sigma_n, active_medium);
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
                             (total_dist >= dist - math::RayEpsilon<Float>) &&
                             (total_dist <= dist + math::RayEpsilon<Float>));
            if (any_or<true>(emitter_hit)) {
                DirectionSample3f ds(si, ref_interaction);
                ds.object                        = emitter;
                masked(emitter_val, emitter_hit) = emitter->eval(si, emitter_hit);
                Float emitter_pdf = scene->pdf_emitter_direction(ref_interaction, ds, emitter_hit);
                update_weights(p_over_f_nee, emitter_pdf, 1.0f, emitter_hit);
                active &= !emitter_hit; // disable lanes which found an emitter
            }

            active_surface &= si.is_valid() && active;
            if (any_or<true>(active_surface)) {
                auto bsdf         = si.bsdf(ray);
                Spectrum bsdf_val = bsdf->eval_null_transmission(si, active_surface);
                update_weights(p_over_f_nee, 1.0f, bsdf_val, active_surface);
                update_weights(p_over_f_uni, 1.0f, bsdf_val, active_surface);
            }

            masked(ray, active_surface) = si.spawn_ray(ray.d);

            // Continue tracing through scene if non-zero weights exist
            active &= (active_medium || active_surface) &&
                      (any(neq(p_over_f_uni, 0.f)) || any(neq(p_over_f_nee, 0.f)));

            // If a medium transition is taking place: Update the medium pointer
            Mask has_medium_trans = active_surface && si.is_medium_transition();
            if (any_or<true>(has_medium_trans)) {
                masked(medium, has_medium_trans) = si.target_medium(ray.d);
            }

        }
        return { p_over_f_nee, p_over_f_uni, emitter_val, ray.d};
    }

    MTS_INLINE
    void update_weights(WeightMatrix& p_over_f, const Spectrum &p, const Spectrum &f, Mask active) const {
        // For two spectra p and f, computes all the ratios of the individual components
        // and multiplies them to the current values in p_over_f
        constexpr size_t n = array_size_v<Spectrum>;
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                Float f_i = depolarize(f).coeff(i);
                Float p_j = depolarize(p).coeff(j);
                Mask invalid = eq(f_i, 0.f) || enoki::isinf(f_i) || enoki::isinf(p_j);
                Float value = p_over_f[i * n + j] * select(invalid, 0.f, p_j / f_i);
                // if p >> f, then a) p/f becomes inf and b) sample has such a small contribution that we can set it to zero
                masked(value, enoki::isinf(value)) = 0.f;
                masked(p_over_f[i * n + j], active) = value;
            }
        }
    }


    MTS_INLINE
    void set_weights(WeightMatrix& target, const WeightMatrix &source, Mask active) const {
        constexpr size_t n = array_size_v<Spectrum>;
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                masked(target[i * n + j], active) = source[i * n + j];
            }
        }
    }

    Spectrum mis_weight(const WeightMatrix& p_over_f) const {
        constexpr size_t n = array_size_v<Spectrum>;
        UnpolarizedSpectrum weight(0.0f);
        for (size_t i = 0; i < n; ++i) {
            Float sum = 0.0f;
            for (size_t j = 0; j < n; ++j)
                sum += p_over_f[i * n + j];
            weight[i] = select(eq(sum, 0.f), 0.0f, n / sum);
        }
        return weight;
    }

    // returns MIS'd throughput/pdf of two full paths represented by p_over_f1 and p_over_f2
    Spectrum mis_weight(const WeightMatrix& p_over_f1, const WeightMatrix& p_over_f2) const {
        constexpr size_t n = array_size_v<Spectrum>;
        UnpolarizedSpectrum weight(0.0f);
        for (size_t i = 0; i < n; ++i) {
            Float sum = 0.0f;
            for (size_t j = 0; j < n; ++j)
                sum += p_over_f1[i * n + j] + p_over_f2[i * n + j];
            weight[i] = select(eq(sum, 0.f), 0.0f, n / sum);
        }
        return weight;
    }

    //! @}
    // =============================================================

    std::string to_string() const override {
        return tfm::format("VolumetricNullPathIntegrator[\n"
                           "  max_depth = %i,\n"
                           "  rr_depth = %i\n"
                           "]",
                           m_max_depth, m_rr_depth);
    }

    MTS_DECLARE_CLASS()
protected:
    bool m_mis;

};

MTS_IMPLEMENT_CLASS_VARIANT(VolumetricNullPathIntegrator, MonteCarloIntegrator);
MTS_EXPORT_PLUGIN(VolumetricNullPathIntegrator, "Volumetric Path Tracer integrator");
NAMESPACE_END(mitsuba)
