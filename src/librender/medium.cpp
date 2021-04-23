#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT Medium<Float, Spectrum>::Medium() : m_is_homogeneous(false), m_has_spectral_extinction(true) {}

MTS_VARIANT Medium<Float, Spectrum>::Medium(const Properties &props) : m_id(props.id()) {

    for (auto &[name, obj] : props.objects(false)) {
        auto *phase = dynamic_cast<PhaseFunction *>(obj.get());
        if (phase) {
            if (m_phase_function)
                Throw("Only a single phase function can be specified per medium");
            m_phase_function = phase;
            props.mark_queried(name);
        }
    }
    if (!m_phase_function) {
        // Create a default isotropic phase function
        m_phase_function =
            PluginManager::instance()->create_object<PhaseFunction>(Properties("isotropic"));
    }

    m_sample_emitters = props.get<bool>("sample_emitters", true);
    ek::set_attr(this, "use_emitter_sampling", m_sample_emitters);
    ek::set_attr(this, "phase_function", m_phase_function.get());
}

MTS_VARIANT Medium<Float, Spectrum>::~Medium() {}

MTS_VARIANT
typename Medium<Float, Spectrum>::MediumInteraction3f
Medium<Float, Spectrum>::sample_interaction(const Ray3f &ray, Float sample,
                                            UInt32 channel, Mask _active) const {
    MTS_MASKED_FUNCTION(ProfilerPhase::MediumSample, _active);

    auto [mi, mint, maxt, active] = prepare_interaction_sampling(ray, _active);

    auto combined_extinction = get_combined_extinction(mi, active);
    Float m                  = extract_channel(combined_extinction, channel);

    Float sampled_t = mint + (-ek::log(1 - sample) / m);
    Mask valid_mi   = active && (sampled_t <= maxt);
    mi.t            = ek::select(valid_mi, sampled_t, ek::Infinity<Float>);
    mi.p            = ray(sampled_t);

    std::tie(mi.sigma_s, mi.sigma_n, mi.sigma_t) =
        get_scattering_coefficients(mi, valid_mi);
    mi.combined_extinction = combined_extinction;
    return mi;
}

MTS_VARIANT
std::pair<typename Medium<Float, Spectrum>::MediumInteraction3f, Spectrum>
Medium<Float, Spectrum>::sample_interaction_real(const Ray3f &ray,
                                                 Sampler *sampler,
                                                 UInt32 channel,
                                                 Mask _active) const {
    MTS_MASKED_FUNCTION(ProfilerPhase::MediumSample, _active);

    auto [mi, mint, maxt, active] = prepare_interaction_sampling(ray, _active);
    // Get majorant
    auto combined_extinction = get_combined_extinction(mi, active);
    Float m                  = extract_channel(combined_extinction, channel);
    mi.combined_extinction   = combined_extinction;

    MediumInteraction3f mi_next = mi;
    Mask escaped = !active;
    Spectrum weight = ek::full<Spectrum>(1.f, ek::width(ray));
    ek::Loop<ek::detached_t<Mask>> loop("Medium::sample_interaction");
    loop.put(active, mi, mi_next, escaped, weight);
    sampler->loop_register(loop);
    loop.init();

    while (loop(ek::detach(active))) {
        // Repeatedly sample from homogenized medium
        Float sampled_t = mi_next.mint + (-ek::log(1 - sampler->next_1d(active)) / m);
        Mask valid_mi = active && (sampled_t < maxt);
        mi_next.t     = sampled_t;
        mi_next.p     = ray(sampled_t);
        std::tie(mi_next.sigma_s, mi_next.sigma_n, mi_next.sigma_t) =
            get_scattering_coefficients(mi_next, valid_mi);

        // Determine whether it was a real or null interaction
        Float r = extract_channel(mi_next.sigma_t, channel) / m;
        Mask did_scatter = valid_mi && (sampler->next_1d(valid_mi) < r);
        mi[did_scatter] = mi_next;

        Spectrum event_pdf = mi_next.sigma_t / m;
        event_pdf = ek::select(did_scatter, event_pdf, 1.f - event_pdf);
        weight[active] *= event_pdf / ek::detach(event_pdf);

        mi_next.mint = sampled_t;
        escaped |= active && (mi_next.mint >= maxt);
        active &= !did_scatter && !escaped;
    }

    ek::masked(mi.t, escaped) = ek::Infinity<Float>;
    mi.p                      = ray(mi.t);

    return { mi, weight };
}

MTS_VARIANT
std::pair<typename Medium<Float, Spectrum>::MediumInteraction3f, Spectrum>
Medium<Float, Spectrum>::sample_interaction_drt(const Ray3f &ray,
                                                Sampler *sampler,
                                                UInt32 channel,
                                                Mask _active) const {
    MTS_MASKED_FUNCTION(ProfilerPhase::MediumSample, _active);

    auto [mi, mint, maxt, active] = prepare_interaction_sampling(ray, _active);
    // Get majorant
    auto combined_extinction = get_combined_extinction(mi, active);
    Float m                  = extract_channel(combined_extinction, channel);

    // Sample proportional to transmittance only using reservoir sampling
    MediumInteraction3f mi_sub = mi;
    Float transmittance = 1.f;
    Float running_t = mint;
    Float acc_weight = 0.f;
    Float sampled_t = ek::NaN<Float>;
    Float sampled_t_step = ek::NaN<Float>;
    Float sampling_weight = ek::NaN<Float>;

    ek::Loop<ek::detached_t<Mask>> loop("Medium::sample_interaction_drt");
    loop.put(active, acc_weight, sampled_t, sampled_t_step, sampling_weight,
             running_t, mi_sub, transmittance);
    sampler->loop_register(loop);
    loop.init();
    while (loop(ek::detach(active))) {
        Float dt = -ek::log(1 - sampler->next_1d(active)) / m;
        Float dt_clamped = ek::min(dt, maxt - mint);

        // Reservoir sampling with replacement
        Float current_weight = transmittance * dt_clamped;
        acc_weight += current_weight;

        // Note: this will always trigger at the first step
        Mask did_interact =
            (sampler->next_1d(active) * acc_weight) < current_weight;
        // Adopt step with replacement
        ek::masked(sampled_t, active & did_interact) = running_t;
        ek::masked(sampled_t_step, active & did_interact) = dt_clamped;
        ek::masked(sampling_weight, active) = acc_weight;

        // Continue stepping
        running_t += dt;

        mi_sub.t = running_t;
        mi_sub.p = ray(running_t);
        auto [_1, _2, current_sigma_t] = get_scattering_coefficients(mi_sub, active);
        Float s = extract_channel(current_sigma_t, channel);
        transmittance *= (1.f - (s / m));
        // Recall that replacement is possible in this loop.
        active &= (running_t < maxt);
    }

    sampled_t = sampled_t + sampler->next_1d() * sampled_t_step;

    Mask valid_mi   = (sampled_t <= maxt);
    mi.t            = ek::select(valid_mi, sampled_t, ek::Infinity<Float>);
    mi.p            = ray(sampled_t);
    std::tie(mi.sigma_s, mi.sigma_n, mi.sigma_t) =
        get_scattering_coefficients(mi, valid_mi);
    mi.combined_extinction = combined_extinction;

    return { mi, sampling_weight };
}

MTS_VARIANT
std::pair<typename Medium<Float, Spectrum>::UnpolarizedSpectrum,
          typename Medium<Float, Spectrum>::UnpolarizedSpectrum>
Medium<Float, Spectrum>::eval_tr_and_pdf(const MediumInteraction3f &mi,
                                         const SurfaceInteraction3f &si,
                                         Mask active) const {
    MTS_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);

    Float t      = ek::min(mi.t, si.t) - mi.mint;
    UnpolarizedSpectrum tr  = ek::exp(-t * mi.combined_extinction);
    UnpolarizedSpectrum pdf = ek::select(si.t < mi.t, tr, tr * mi.combined_extinction);
    return { tr, pdf };
}

MTS_VARIANT
MTS_INLINE auto
Medium<Float, Spectrum>::prepare_interaction_sampling(const Ray3f &ray,
                                                      Mask active) const {
    // Initialize basic medium interaction fields
    MediumInteraction3f mi = ek::zero<MediumInteraction3f>();
    mi.sh_frame    = Frame3f(ray.d);
    mi.wi          = -ray.d;
    mi.time        = ray.time;
    mi.wavelengths = ray.wavelengths;
    mi.medium      = this;

    auto [aabb_its, mint, maxt] = intersect_aabb(ray);
    aabb_its &= (ek::isfinite(mint) || ek::isfinite(maxt));
    active &= aabb_its;
    ek::masked(mint, !active) = 0.f;
    ek::masked(maxt, !active) = ek::Infinity<Float>;

    mint = ek::max(ray.mint, mint);
    maxt = ek::min(ray.maxt, maxt);
    mi.mint   = mint;

    return std::make_tuple(mi, mint, maxt, active);
}

MTS_VARIANT
MTS_INLINE Float
Medium<Float, Spectrum>::extract_channel(Spectrum value, UInt32 channel) const {
    Float result = value[0];
    if constexpr (is_rgb_v<Spectrum>) { // Handle RGB rendering
        ek::masked(result, ek::eq(channel, 1u)) = value[1];
        ek::masked(result, ek::eq(channel, 2u)) = value[2];
    } else {
        ENOKI_MARK_USED(channel);
    }
    return result;
}

MTS_IMPLEMENT_CLASS_VARIANT(Medium, Object, "medium")
MTS_INSTANTIATE_CLASS(Medium)
NAMESPACE_END(mitsuba)
