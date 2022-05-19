#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Medium<Float, Spectrum>::Medium(const Properties &props) : m_id(props.id()) {

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
    dr::set_attr(this, "use_emitter_sampling", m_sample_emitters);
    dr::set_attr(this, "phase_function", m_phase_function.get());
}

MI_VARIANT Medium<Float, Spectrum>::~Medium() {}

MI_VARIANT
typename Medium<Float, Spectrum>::MediumInteraction3f
Medium<Float, Spectrum>::sample_interaction(const Ray3f &ray, Float sample,
                                            UInt32 channel, Mask _active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::MediumSample, _active);

    auto [mei, mint, maxt, active] = prepare_interaction_sampling(ray, _active);

    auto combined_extinction = get_majorant(mei, active);
    Float m                  = extract_channel(combined_extinction, channel);

    Float sampled_t = mint + (-dr::log(1 - sample) / m);
    Mask valid_mei   = active && (sampled_t <= maxt);
    mei.t            = dr::select(valid_mei, sampled_t, dr::Infinity<Float>);
    mei.p            = ray(sampled_t);

    std::tie(mei.sigma_s, mei.sigma_n, mei.sigma_t) =
        get_scattering_coefficients(mei, valid_mei);
    mei.combined_extinction = combined_extinction;
    return mei;
}

MI_VARIANT
std::pair<typename Medium<Float, Spectrum>::MediumInteraction3f, Spectrum>
Medium<Float, Spectrum>::sample_interaction_real(const Ray3f &ray,
                                                 Sampler *sampler,
                                                 UInt32 channel,
                                                 Mask _active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::MediumSample, _active);

    auto [mei, mint, maxt, active] = prepare_interaction_sampling(ray, _active);
    // Get majorant
    auto combined_extinction = get_majorant(mei, active);
    Float m                  = extract_channel(combined_extinction, channel);
    mei.combined_extinction   = combined_extinction;

    MediumInteraction3f mei_next = mei;
    Mask escaped = !active;
    Spectrum weight = dr::full<Spectrum>(1.f, dr::width(ray));
    dr::Loop<Mask> loop("Medium::sample_interaction");
    loop.put(active, mei, mei_next, escaped, weight);
    sampler->loop_put(loop);
    loop.init();

    while (loop(active)) {
        // Repeatedly sample from homogenized medium
        Float sampled_t  = mei_next.mint + (-dr::log(1 - sampler->next_1d(active)) / m);
        Mask valid_mei   = active && (sampled_t < maxt);
        mei_next.t       = sampled_t;
        mei_next.p       = ray(sampled_t);
        std::tie(mei_next.sigma_s, mei_next.sigma_n, mei_next.sigma_t) =
            get_scattering_coefficients(mei_next, valid_mei);

        // Determine whether it was a real or null interaction
        Float r = extract_channel(mei_next.sigma_t, channel) / m;
        Mask did_scatter = valid_mei && (sampler->next_1d(valid_mei) < r);
        mei[did_scatter] = mei_next;

        Spectrum event_pdf = mei_next.sigma_t / m;
        event_pdf = dr::select(did_scatter, event_pdf, 1.f - event_pdf);
        weight[active] *= event_pdf / dr::detach(event_pdf);

        mei_next.mint = sampled_t;
        escaped |= active && (mei_next.mint >= maxt);
        active &= !did_scatter && !escaped;
    }

    dr::masked(mei.t, escaped) = dr::Infinity<Float>;
    mei.p                      = ray(mei.t);

    return { mei, weight };
}

MI_VARIANT
std::pair<typename Medium<Float, Spectrum>::MediumInteraction3f, Spectrum>
Medium<Float, Spectrum>::sample_interaction_drt(const Ray3f &ray,
                                                Sampler *sampler,
                                                UInt32 channel,
                                                Mask _active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::MediumSample, _active);

    auto [mei, mint, maxt, active] = prepare_interaction_sampling(ray, _active);
    const Mask did_traverse = active;

    // Get majorant
    auto combined_extinction = get_majorant(mei, active);
    Float m                  = extract_channel(combined_extinction, channel);

    // Sample proportional to transmittance only using reservoir sampling
    MediumInteraction3f mei_sub = mei;
    Float transmittance = 1.f;
    Float running_t = mint;
    Float acc_weight = 0.f;
    Float sampled_t = dr::NaN<Float>;
    Float sampled_t_step = dr::NaN<Float>;
    Float sampling_weight = dr::NaN<Float>;

    dr::Loop<Mask> loop("Medium::sample_interaction_drt");
    loop.put(active, acc_weight, sampled_t, sampled_t_step, sampling_weight,
             running_t, mei_sub, transmittance);
    sampler->loop_put(loop);
    loop.init();
    while (loop(active)) {
        Float dt = -dr::log(1 - sampler->next_1d(active)) / m;
        Float dt_clamped = dr::min(dt, maxt - running_t);

        // Reservoir sampling with replacement
        Float current_weight = transmittance * dt_clamped;
        acc_weight += current_weight;

        // Note: this will always trigger at the first step
        Mask did_interact =
            (sampler->next_1d(active) * acc_weight) < current_weight;
        // Adopt step with replacement
        dr::masked(sampled_t, active & did_interact) = running_t;
        dr::masked(sampled_t_step, active & did_interact) = dt_clamped;
        dr::masked(sampling_weight, active) = acc_weight;

        // Continue stepping
        running_t += dt;

        dr::masked(mei_sub.t, active) = running_t;
        dr::masked(mei_sub.p, active) = ray(running_t);
        auto [_1, _2, current_sigma_t] = get_scattering_coefficients(mei_sub, active);
        Float s = extract_channel(current_sigma_t, channel);
        transmittance *= (1.f - (s / m));
        // Recall that replacement is possible in this loop.
        active &= (running_t < maxt);
    }

    sampled_t = sampled_t + sampler->next_1d(did_traverse) * sampled_t_step;

    Mask valid_mei   = (sampled_t <= maxt);
    mei.t            = dr::select(valid_mei, sampled_t, dr::Infinity<Float>);
    mei.p            = ray(sampled_t);
    std::tie(mei.sigma_s, mei.sigma_n, mei.sigma_t) =
        get_scattering_coefficients(mei, valid_mei);
    mei.combined_extinction = get_majorant(mei, valid_mei);

    return { mei, sampling_weight };
}


MI_VARIANT
std::pair<typename Medium<Float, Spectrum>::MediumInteraction3f, Spectrum>
Medium<Float, Spectrum>::sample_interaction_drrt(const Ray3f &ray,
                                                 Sampler *sampler,
                                                 UInt32 channel,
                                                 Mask _active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::MediumSample, _active);

    auto [mei, mint, maxt, active] = prepare_interaction_sampling(ray, _active);

    // Get majorant
    auto combined_extinction = get_majorant(mei, active);
    Float m = extract_channel(combined_extinction, channel);
    // TODO: need to tweak this?
    Float control = 0.5f * m;
    const Mask did_traverse = active;

    // Sample proportional to transmittance only using reservoir sampling
    MediumInteraction3f mei_sub = mei;
    Float transmittance = 1.f;
    Float running_t = mint;
    Float acc_weight = 0.f;
    Float sampled_t = dr::NaN<Float>;
    Float sampled_t_step = dr::NaN<Float>;
    Float sampling_weight = dr::NaN<Float>;

    dr::Loop<Mask> loop("Medium::sample_interaction_drrt");
    loop.put(active, acc_weight, sampled_t, sampled_t_step, sampling_weight,
             running_t, mei_sub, transmittance);
    sampler->loop_put(loop);
    loop.init();
    while (loop(active)) {
        Float dt = -dr::log(1 - sampler->next_1d(active)) / m;
        Float dt_clamped = dr::min(dt, maxt - running_t);

        // Reservoir sampling with replacement
        Float current_weight =
            transmittance * (1.f - dr::exp(-control * dt_clamped)) / control;
        acc_weight += current_weight;

        // Note: this will always trigger at the first step
        Mask did_interact =
            (sampler->next_1d(active) * acc_weight) < current_weight;
        // Adopt step with replacement
        dr::masked(sampled_t, active & did_interact) = running_t;
        dr::masked(sampled_t_step, active & did_interact) = dt_clamped;
        dr::masked(sampling_weight, active) = acc_weight;

        // Continue stepping
        running_t += dt;

        dr::masked(mei_sub.t, active) = running_t;
        dr::masked(mei_sub.p, active) = ray(running_t);
        auto [_1, _2, current_sigma_t] =
            get_scattering_coefficients(mei_sub, active);
        Float s = Medium::extract_channel(current_sigma_t, channel);
        transmittance *= (1.f - (s - control) / m) * dr::exp(-control * dt);
        // Recall that replacement is possible in this loop.
        active &= (running_t < maxt);
    }

    Float scale = 1 - dr::exp(-control * sampled_t_step);
    sampled_t = sampled_t - dr::log(1.f - sampler->next_1d(did_traverse) * scale) / control;

    Mask valid_mei   = (sampled_t <= maxt);
    mei.t            = dr::select(valid_mei, sampled_t, dr::Infinity<Float>);
    mei.p            = ray(sampled_t);
    std::tie(mei.sigma_s, mei.sigma_n, mei.sigma_t) =
        get_scattering_coefficients(mei, valid_mei);
    mei.combined_extinction = get_majorant(mei, valid_mei);

    return { mei, sampling_weight };
}

MI_VARIANT
std::pair<typename Medium<Float, Spectrum>::UnpolarizedSpectrum,
          typename Medium<Float, Spectrum>::UnpolarizedSpectrum>
Medium<Float, Spectrum>::eval_tr_and_pdf(const MediumInteraction3f &mei,
                                         const SurfaceInteraction3f &si,
                                         Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);

    Float t                 = dr::minimum(mei.t, si.t) - mei.mint;
    UnpolarizedSpectrum tr  = dr::exp(-t * mei.combined_extinction);
    UnpolarizedSpectrum pdf = dr::select(si.t < mei.t, tr, tr * mei.combined_extinction);
    return { tr, pdf };
}

MI_VARIANT
MI_INLINE
std::tuple<typename Medium<Float, Spectrum>::MediumInteraction3f, Float, Float,
           typename Medium<Float, Spectrum>::Mask>
Medium<Float, Spectrum>::prepare_interaction_sampling(const Ray3f &ray,
                                                      Mask active) const {
    // Initialize basic medium interaction fields
    MediumInteraction3f mei = dr::zeros<MediumInteraction3f>();
    mei.sh_frame            = Frame3f(ray.d);
    mei.wi                  = -ray.d;
    mei.time                = ray.time;
    mei.wavelengths         = ray.wavelengths;
    mei.medium              = this;

    auto [aabb_its, mint, maxt] = intersect_aabb(ray);
    aabb_its &= (dr::isfinite(mint) || dr::isfinite(maxt));
    active &= aabb_its;
    dr::masked(mint, !active) = 0.f;
    dr::masked(maxt, !active) = dr::Infinity<Float>;

    mint = dr::maximum(0.f, mint);
    maxt = dr::minimum(ray.maxt, maxt);
    mei.mint   = mint;

    return std::make_tuple(mei, mint, maxt, active);
}

MI_VARIANT
MI_INLINE Float
Medium<Float, Spectrum>::extract_channel(Spectrum value, UInt32 channel) {
    Float result = value[0];
    if constexpr (is_rgb_v<Spectrum>) { // Handle RGB rendering
        dr::masked(result, dr::eq(channel, 1u)) = value[1];
        dr::masked(result, dr::eq(channel, 2u)) = value[2];
    } else {
        DRJIT_MARK_USED(channel);
    }
    return result;
}

MI_IMPLEMENT_CLASS_VARIANT(Medium, Object, "medium")
MI_INSTANTIATE_CLASS(Medium)
NAMESPACE_END(mitsuba)
