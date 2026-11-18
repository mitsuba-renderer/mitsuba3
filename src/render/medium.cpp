#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/extremum.h>
#include <mitsuba/render/extremum_segment.h>
#include <mitsuba/core/random.h>
#include <drjit/while_loop.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Medium<Float, Spectrum>::Medium()
    : JitObject<Medium>(""),
      m_is_homogeneous(false),
      m_has_spectral_extinction(true) {
}

MI_VARIANT Medium<Float, Spectrum>::Medium(const Properties &props)
    : JitObject<Medium>(props.id()) {
    for (auto &prop : props.objects()) {
        if (PhaseFunction *phase = prop.try_get<PhaseFunction>()) {
            if (m_phase_function)
                Throw("Only a single phase function can be specified per medium");
            m_phase_function = phase;
        }
    }
    if (!m_phase_function) {
        // Create a default isotropic phase function
        m_phase_function =
            PluginManager::instance()->create_object<PhaseFunction>(Properties("isotropic"));
    }

    m_use_rrt = props.get<bool>("use_rrt", true);
    m_rr_threshold = props.get<ScalarFloat>("rr_threshold", 0.f);

    m_sample_emitters = props.get<bool>("sample_emitters", true);
}

MI_VARIANT Medium<Float, Spectrum>::~Medium() { }

MI_VARIANT void Medium<Float, Spectrum>::traverse(TraversalCallback *cb) {
    cb->put("phase_function", m_phase_function, ParamFlags::Differentiable);
    cb->put("extremum", m_extremum, ParamFlags::NonDifferentiable);
}

MI_VARIANT
typename Medium<Float, Spectrum>::MediumInteraction3f
Medium<Float, Spectrum>::sample_interaction(const Ray3f &ray, Float sample,
                                            UInt32 channel, Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::MediumSample, active);

    // initialize basic medium interaction fields
    MediumInteraction3f mei = dr::zeros<MediumInteraction3f>();
    mei.wi          = -ray.d;
    mei.sh_frame    = Frame3f(mei.wi);
    mei.time        = ray.time;
    mei.wavelengths = ray.wavelengths;

    auto [aabb_its, mint, maxt] = intersect_aabb(ray);
    aabb_its &= (dr::isfinite(mint) || dr::isfinite(maxt));
    active &= aabb_its;
    dr::masked(mint, !active) = 0.f;
    dr::masked(maxt, !active) = dr::Infinity<Float>;

    mint = dr::maximum(0.f, mint);
    maxt = dr::minimum(ray.maxt, maxt);

    auto combined_extinction = get_majorant(mei, active);
    Float m                  = combined_extinction[0];
    if constexpr (is_rgb_v<Spectrum>) { // Handle RGB rendering
        dr::masked(m, channel == 1u) = combined_extinction[1];
        dr::masked(m, channel == 2u) = combined_extinction[2];
    } else {
        DRJIT_MARK_USED(channel);
    }

    Float sampled_t = mint + (-dr::log(1 - sample) / m);
    Mask valid_mi   = active && (sampled_t <= maxt);
    mei.t           = dr::select(valid_mi, sampled_t, dr::Infinity<Float>);
    mei.p           = ray(sampled_t);
    mei.medium      = this;
    mei.mint        = mint;

    std::tie(mei.sigma_s, mei.sigma_n, mei.sigma_t) =
        get_scattering_coefficients(mei, valid_mi);
    mei.combined_extinction = combined_extinction;
    return mei;
}

MI_VARIANT
std::pair<typename Medium<Float, Spectrum>::UnpolarizedSpectrum,
          typename Medium<Float, Spectrum>::UnpolarizedSpectrum>
Medium<Float, Spectrum>::transmittance_eval_pdf(const MediumInteraction3f &mi,
                                                const SurfaceInteraction3f &si,
                                                Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);

    Float t      = dr::minimum(mi.t, si.t) - mi.mint;
    UnpolarizedSpectrum tr  = dr::exp(-t * mi.combined_extinction);
    UnpolarizedSpectrum pdf = dr::select(si.t < mi.t, tr, tr * mi.combined_extinction);
    return { tr, pdf };
}

/// Helper function to index the channel of an ``UnpolarizedSpectrum``.
template <typename Float, typename Spectrum>
MI_INLINE Float index_spectrum(const unpolarized_spectrum_t<Spectrum> &spec,
                               const dr::uint32_array_t<Float> &idx) {
    Float m = spec[0];
    if constexpr (is_rgb_v<Spectrum>) {
        dr::masked(m, idx == 1u) = spec[1];
        dr::masked(m, idx == 2u) = spec[2];
    } else {
        DRJIT_MARK_USED(idx);
    }
    return m;
}

/**
 * \brief Result of one tracking traversal.
 *
 * Every event of the traversal (a free flight, a null or a real collision)
 * has a contribution \c f (a spectrum), a sampling density \c p (a spectrum:
 * the density with which each channel's majorants would have sampled it) and
 * a channel-independent probability \c P of the null / real decision. With
 * \c p_c the density of the hero channel, the record holds the products over
 * all events of <tt>f / (p_c P)</tt>, <tt>f / p_c</tt> and <tt>p / p_c</tt>.
 */
MI_VARIANT struct Medium<Float, Spectrum>::TrackingRecord {
    /// Real collision, and product of <tt>p / p_c</tt> as \c pdf_ratio
    MediumSample3f sample;

    /// Product of <tt>f / (p_c P)</tt>: the path throughput weight
    UnpolarizedSpectrum throughput;

    /// Product of <tt>f / p_c</tt>: the transmittance estimate
    UnpolarizedSpectrum transmittance;

    DRJIT_STRUCT(TrackingRecord, sample, throughput, transmittance)
};

MI_VARIANT
std::pair<typename Medium<Float, Spectrum>::MediumSample3f,
          typename Medium<Float, Spectrum>::UnpolarizedSpectrum>
Medium<Float, Spectrum>::sample_real_interaction(const Ray3f &ray, Float sample,
                                                      const UInt32 &channel,
                                                      Mask active) const {
    TrackingRecord rec = track<false>(ray, sample, channel, false, active);
    rec.sample.transmittance = rec.transmittance;
    return { rec.sample, rec.throughput };
}

MI_VARIANT typename Medium<Float, Spectrum>::UnpolarizedSpectrum
Medium<Float, Spectrum>::sample_transmittance(const Ray3f &ray, Float sample,
                                                   const UInt32 &channel,
                                                   Mask active) const {
    return track<true>(ray, sample, channel, m_use_rrt, active).transmittance;
}

MI_VARIANT std::tuple<typename Medium<Float, Spectrum>::UnpolarizedSpectrum,
                      typename Medium<Float, Spectrum>::UnpolarizedSpectrum,
                      typename Medium<Float, Spectrum>::UnpolarizedSpectrum>
Medium<Float, Spectrum>::sample_transmittance_pdf(const Ray3f &ray, Float sample,
                                                       const UInt32 &channel,
                                                       Mask active) const {
    TrackingRecord rec = track<true>(ray, sample, channel, false, active);
    return { rec.transmittance, rec.throughput, rec.sample.pdf_ratio };
}

MI_VARIANT template <bool Ratio>
typename Medium<Float, Spectrum>::TrackingRecord
Medium<Float, Spectrum>::track(const Ray3f &ray, Float sample,
                               const UInt32 &channel, Mask rrt,
                               Mask active) const {
    auto [hit, mint, maxt] = intersect_aabb(ray);
    mint = dr::maximum(0.f, mint);
    maxt = dr::minimum(ray.maxt, maxt);
    active &= hit && mint < maxt;

    TrackingRecord rec         = dr::zeros<TrackingRecord>();
    rec.sample.mei.t           = dr::Infinity<Float>;
    rec.sample.mei.mint        = mint;
    rec.sample.mei.wi          = -ray.d;
    rec.sample.mei.sh_frame    = Frame3f(rec.sample.mei.wi);
    rec.sample.mei.time        = ray.time;
    rec.sample.mei.wavelengths = ray.wavelengths;
    rec.sample.mei.medium      = this;
    rec.sample.pdf_ratio       = 1.f;
    rec.throughput             = 1.f;
    rec.transmittance          = 1.f;

    const Extremum *extremum = m_extremum.get();

    struct LoopState {
        ExtremumState state;
        Vector2f value;
        Float segment_maxt;
        TrackingRecord rec;
        PCG32<UInt32> rng;
        Float target_ot;
        UnpolarizedSpectrum flight_ot;
        UnpolarizedSpectrum control_ot;
        Float t;
        Mask advance;
        Mask active;

        DRJIT_STRUCT(LoopState, state, value, segment_maxt, rec, rng,
                     target_ot, flight_ot, control_ot, t, advance, active)
    };

    LoopState ls = dr::zeros<LoopState>();
    ls.segment_maxt = maxt;
    ls.rec          = rec;
    ls.rng.seed(UInt32(sample * 4294967296.f));
    ls.target_ot    = -dr::log(1.f - sample);
    ls.t            = mint;
    ls.advance      = active;
    ls.active       = active;
    ls.state        = extremum->begin(ray, mint, maxt, active);

    auto step =
    [this, extremum, &ray, &channel, &rrt, maxt = maxt]
    (LoopState &ls) {

        TrackingRecord& rec = ls.rec;

        // copy active state
        Mask active = ls.active;

        // Get next structure segment.
        if (dr::any_or<true>(ls.advance)) {
            Mask advance         = ls.advance;
            ExtremumState &state = ls.state;

            Mask stale   = ls.t >= state.mint;
            Mask more    = state.mint < state.maxt;
            Mask refresh = stale && more && advance;

            if (dr::any_or<true>(refresh))
                dr::masked(ls.value, refresh) =
                    extremum->next(state, ray, refresh).value;

            Mask spent = stale && !more && advance;
            dr::masked(ls.value, spent) = 0.f;
            Float seg_maxt = dr::minimum(
                maxt, dr::select(spent, maxt, state.mint));
            dr::masked(ls.segment_maxt, advance) = dr::maximum(seg_maxt, ls.t);
        }

        UnpolarizedSpectrum control  = ls.value.x();
        UnpolarizedSpectrum majorant = ls.value.y();
        UnpolarizedSpectrum residual = majorant - dr::select(rrt, control, 0.f);

        // Sample distance in current segment.
        Float residual_c = index_spectrum<Float, Spectrum>(residual, channel);
        Float segment_ot = (ls.segment_maxt - ls.t) * residual_c;
        Mask sampled     = (ls.target_ot < segment_ot) && active;
        Float t_next     = ls.segment_maxt;

        Float event_prob = 1.f;
        UnpolarizedSpectrum p(1.f), f(1.f);

        if (dr::any_or<true>(sampled)) {
            dr::masked(t_next, sampled) =
                ls.t + ls.target_ot / dr::maximum(residual_c, dr::Epsilon<Float>);

            // Retrieve optical properties at sampled distance
            MediumInteraction3f mei_c = rec.sample.mei;
            mei_c.t = t_next;
            mei_c.p = ray(t_next);
            auto [sigma_s, sigma_n_, sigma_t] =
                get_scattering_coefficients(mei_c, sampled);
            DRJIT_MARK_USED(sigma_n_);

            UnpolarizedSpectrum sigma_n = majorant - sigma_t;
            Float null_prob = dr::mean(
                dr::select(residual > 0.f, sigma_n / residual, 0.f));

            dr::masked(p, sampled) *= residual;

            if constexpr (Ratio) {
                dr::masked(f, sampled)         *= sigma_n;
                dr::masked(event_prob, sampled) = null_prob;
            } else {
                // Event sampling
                Float u = ls.rng.template next_float<Float>(sampled);
                Mask null_scatter = sampled &&  u < null_prob;
                Mask real_scatter = sampled && !null_scatter;

                if (dr::any_or<true>(null_scatter)) {
                    dr::masked(f, null_scatter)         *= sigma_n;
                    dr::masked(event_prob, null_scatter) = null_prob;
                }

                if (dr::any_or<true>(real_scatter)) {
                    dr::masked(f, real_scatter)         *= sigma_s;
                    dr::masked(event_prob, real_scatter) = 1.f - null_prob;
                    dr::masked(rec.sample.mei, real_scatter) = mei_c;
                }

                ls.active &= !real_scatter;
            }

            dr::masked(ls.target_ot, sampled) =
                -dr::log(1.f - ls.rng.template next_float<Float>(sampled));
        }

        Float dt = t_next - ls.t;
        // Accumulate residual excess optical thickness over the sampled channel
        dr::masked(ls.flight_ot, active) += dt * (residual - residual_c);
        // Accumulate the control optical thickness
        if constexpr (Ratio)
            dr::masked(ls.control_ot, active) += dt * control;

        Float p_c     = index_spectrum<Float, Spectrum>(p, channel);
        Float inv_p   = dr::select(p_c > 0.f, dr::rcp(p_c), 0.f);
        Float inv_evt = dr::select(event_prob != 0.f, dr::rcp(event_prob), 0.f);
        dr::masked(rec.transmittance, active) *= f * inv_p;
        dr::masked(rec.throughput, active)    *= f * inv_p * inv_evt;
        dr::masked(rec.sample.pdf_ratio, active) *= p * inv_p;

        if constexpr (Ratio) {
            // Russian roulette based on transmittance weight
            if (m_rr_threshold > 0.f) {
                UnpolarizedSpectrum control_ot = dr::select(rrt, ls.control_ot, 0.f);
                UnpolarizedSpectrum estimate   =
                    rec.transmittance * dr::exp(-(control_ot + ls.flight_ot));

                // Detached: survivors must keep the gradient of their estimate
                Float q = dr::max(dr::detach(estimate)) / m_rr_threshold;
                Mask roulette = active && q < 1.f;

                if (dr::any_or<true>(roulette)) {
                    Mask survive = ls.rng.template next_float<Float>(roulette) < q;
                    Float scale  = dr::select(survive, dr::rcp(q), 0.f);
                    dr::masked(rec.transmittance, roulette) *= scale;
                    dr::masked(rec.throughput, roulette)    *= scale;
                }
            }
        }

        dr::masked(ls.target_ot, !sampled && active) -= segment_ot;
        dr::masked(ls.t, active) = t_next;
        ls.advance = !sampled && active;
        ls.active &= p_c > 0.f && (!ls.advance || (ls.t < maxt));

        if constexpr (Ratio)
            ls.active &= dr::any(rec.transmittance > 0.f);
    };

    dr::tie(ls) = dr::while_loop(
        dr::make_tuple(ls), [](const LoopState &ls) { return ls.active; },
        step, "Medium::track");

    if constexpr (Ratio) {
        UnpolarizedSpectrum control_tr =
            dr::exp(-dr::select(rrt, ls.control_ot, 0.f));
        ls.rec.transmittance *= control_tr;
        ls.rec.throughput    *= control_tr;
    }

    UnpolarizedSpectrum flight_tr = dr::exp(-ls.flight_ot);
    ls.rec.transmittance    *= flight_tr;
    ls.rec.throughput       *= flight_tr;
    ls.rec.sample.pdf_ratio *= flight_tr;
    return ls.rec;
}

MI_IMPLEMENT_TRAVERSE_CB(Medium, Object)
MI_INSTANTIATE_CLASS(Medium)
NAMESPACE_END(mitsuba)
