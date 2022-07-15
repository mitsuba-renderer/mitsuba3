#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/volume.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Medium<Float, Spectrum>::Medium(const Properties &props)
    : m_majorant_grid(nullptr), m_id(props.id()) {

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

    m_majorant_factor = props.get<ScalarFloat>("majorant_factor", 1.01);
    m_majorant_resolution_factor = props.get<size_t>("majorant_resolution_factor", 0);
    m_control_density = dr::NaN<ScalarFloat>;

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

    const Float desired_tau = -dr::log(1 - sample);
    Float sampled_t;
    if (m_majorant_grid) {
        // --- Spatially-variying majorant (supergrid).
        // 1. Prepare for DDA traversal
        // Adapted from: https://github.com/francisengelmann/fast_voxel_traversal/blob/9664f0bde1943e69dbd1942f95efc31901fbbd42/main.cpp
        // TODO: allow precomputing all this (but be careful when ray origin is updated)
        auto [dda_t, dda_tmax, dda_tdelta] = prepare_dda_traversal(
            m_majorant_grid.get(), ray, mint, maxt, active);

        // 2. Traverse the medium with DDA until we reach the desired
        // optical depth.
        Mask active_dda = active;
        Mask reached = false;
        Float tau_acc = 0.f;
        dr::Loop<Mask> dda_loop("Medium::sample_interaction_dda");
        dda_loop.put(active_dda, reached, dda_t, dda_tmax, tau_acc, mei);
        dda_loop.init();
        while (dda_loop(dr::detach(active_dda))) {
            // Figure out which axis we hit first.
            // `t_next` is the ray's `t` parameter when hitting that axis.
            Float t_next = dr::min(dda_tmax);
            Vector3f tmax_update;
            Mask got_assigned = false;
            for (size_t k = 0; k < 3; ++k) {
                Mask active_k = dr::eq(dda_tmax[k], t_next);
                tmax_update[k] = dr::select(!got_assigned && active_k, dda_tdelta[k], 0);
                got_assigned |= active_k;
            }

            // Lookup and accumulate majorant in current cell.
            dr::masked(mei.t, active_dda) = 0.5f * (dda_t + t_next);
            dr::masked(mei.p, active_dda) = ray(mei.t);
            // TODO: avoid this vcall, could lookup directly from the array
            // of floats (but we still need to account for the bbox, etc).
            Float majorant = m_majorant_grid->eval_1(mei, active_dda);
            Float tau_next = tau_acc + majorant * (t_next - dda_t);

            // For rays that will stop within this cell, figure out
            // the precise `t` parameter where `desired_tau` is reached.
            Float t_precise = dda_t + (desired_tau - tau_acc) / majorant;
            reached |= active_dda && (majorant > 0) && (t_precise < maxt) && (tau_next >= desired_tau);
            dr::masked(dda_t, active_dda) = dr::select(reached, t_precise, t_next);

            // Prepare for next iteration
            active_dda &= !reached && (t_next < maxt);
            dr::masked(dda_tmax, active_dda) = dda_tmax + tmax_update;
            dr::masked(tau_acc, active_dda) = tau_next;
        }

        // Adopt the stopping location, making sure to convert to the main
        // ray's parametrization.
        sampled_t = dr::select(reached, dda_t, dr::Infinity<Float>);
    } else {
        // --- A single majorant for the whole volume.
        mei.combined_extinction = dr::detach(get_majorant(mei, active));
        Float m                = extract_channel(mei.combined_extinction, channel);
        sampled_t              = mint + (desired_tau / m);
    }

    Mask valid_mei = active && (sampled_t <= maxt);
    mei.t          = dr::select(valid_mei, sampled_t, dr::Infinity<Float>);
    mei.p          = ray(sampled_t);

    if (m_majorant_grid) {
        // Otherwise it was already looked up above
        mei.combined_extinction = dr::detach(m_majorant_grid->eval_1(mei, valid_mei));
    }
    std::tie(mei.sigma_s, mei.sigma_n, mei.sigma_t) =
        get_scattering_coefficients(mei, valid_mei);
    return mei;
}

MI_VARIANT
std::pair<typename Medium<Float, Spectrum>::MediumInteraction3f, Spectrum>
Medium<Float, Spectrum>::sample_interaction_real(const Ray3f &ray,
                                                 Sampler *sampler,
                                                 UInt32 channel,
                                                 Mask _active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::MediumSample, _active);
    // TODO: optimize for RGB by making `channel` a scalar index?
    if (m_majorant_grid)
        NotImplementedError("sample_interaction_real with majorant supergrid");

    auto [mei, mint, maxt, active] = prepare_interaction_sampling(ray, _active);

    MediumInteraction3f mei_next = mei;
    Mask escaped = !active;
    Spectrum weight = dr::full<Spectrum>(1.f, dr::width(ray));
    dr::Loop<Mask> loop("Medium::sample_interaction_real");


    // Get global majorant once and for all
    auto combined_extinction = get_majorant(mei, active);
    mei.combined_extinction   = combined_extinction;
    Float global_majorant = extract_channel(combined_extinction, channel);

    loop.put(active, mei, mei_next, escaped, weight);
    sampler->loop_put(loop);
    loop.init();

    while (loop(active)) {
        // Repeatedly sample from homogenized medium
        Float desired_tau = -dr::log(1 - sampler->next_1d(active));
        Float sampled_t = mei_next.mint + desired_tau / global_majorant;

        Mask valid_mei = active && (sampled_t < maxt);
        mei_next.t     = sampled_t;
        mei_next.p     = ray(sampled_t);
        std::tie(mei_next.sigma_s, mei_next.sigma_n, mei_next.sigma_t) =
            get_scattering_coefficients(mei_next, valid_mei);

        // Determine whether it was a real or null interaction
        Float r = extract_channel(mei_next.sigma_t, channel) / global_majorant;
        Mask did_scatter = valid_mei && (sampler->next_1d(valid_mei) < r);
        mei[did_scatter] = mei_next;

        Spectrum event_pdf = mei_next.sigma_t / combined_extinction;
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
    const bool has_supergrid = (bool) m_majorant_grid;

    // Sample proportional to transmittance only using reservoir sampling
    MediumInteraction3f mei_sub = mei;
    Float transmittance         = 1.f;
    Float running_t             = mint;
    Float acc_weight            = 0.f;
    Float sampled_t             = dr::NaN<Float>;
    Float sampled_t_step        = dr::NaN<Float>;
    Float sampling_weight       = dr::NaN<Float>;

    dr::Loop<Mask> loop("Medium::sample_interaction_drt");
    loop.put(active, sampler, acc_weight, sampled_t, sampled_t_step,
             sampling_weight, running_t, mei_sub, transmittance);
    Float global_majorant;
    Float dda_t, desired_tau, tau_acc;
    Vector3f dda_tmax, dda_tdelta;
    Mask needs_new_target;
    if (has_supergrid) {
        // Prepare for DDA traversal
        std::tie(dda_t, dda_tmax, dda_tdelta) = prepare_dda_traversal(
            m_majorant_grid.get(), ray, mint, maxt, active);
        desired_tau = 0.f;
        tau_acc = 0.f;
        needs_new_target = true;
        global_majorant = dr::NaN<Float>;

        loop.put(dda_t, dda_tmax, needs_new_target, desired_tau, tau_acc);
    } else {
        // Get global majorant
        global_majorant =
            extract_channel(get_majorant(mei, active), channel);
    }
    loop.init();

    while (loop(active)) {
        // --- Mixed DRT / DDA loop:
        // - If the majorant is spatially varying, one iteration of the loop
        //   will both progress traversal of the majorant supergrid with DDA
        //   and update the running (t, weight) results of DRT sampling.
        //   The latter update only happens when a valid free-flight distance
        //   has been sampled with DDA.
        //   Moreover, we must be extra-careful because DRT allows sampling with
        //   replacement, so updates keep taking place throughout traversal.
        // - If the majorant is constant over the whole volume, there's
        //   no need to perform DDA, we can sample a distance in closed form.

        Float dt, local_majorant;
        Mask active_drt;
        if (has_supergrid) {
            dr::masked(desired_tau, active && needs_new_target) =
                -dr::log(1 - sampler->next_1d(active));
            dr::masked(tau_acc, active && needs_new_target) = 0.f;

            // -- One step of DDA
            // Only some of the lanes will reach `desired_tau` at this iteration
            // and be able to proceed with a DRT step.
            Mask active_dda = active;

            // Figure out which axis we hit first.
            // `dda_t_next` is the ray's `t` parameter when hitting that axis.
            Float dda_t_next = dr::min(dda_tmax);
            // TODO: this selection scheme looks dangerous in case of exact equality
            //       with more than one component.
            Vector3f tmax_update;
            for (size_t k = 0; k < 3; ++k) {
                tmax_update[k] = dr::select(dr::eq(dda_tmax[k], dda_t_next),
                                            dda_tdelta[k], 0);
            }

            // Lookup and accumulate majorant in current cell.
            dr::masked(mei_sub.t, active_dda) = 0.5f * (dda_t + dda_t_next);
            dr::masked(mei_sub.p, active_dda) = ray(mei_sub.t);
            Float majorant = m_majorant_grid->eval_1(mei_sub, active_dda);
            Float tau_next = tau_acc + majorant * (dda_t_next - dda_t);

            // For rays that will stop within this cell, figure out
            // the precise `t` parameter where `desired_tau` is reached.
            Float t_precise = dda_t + (desired_tau - tau_acc) / majorant;
            Mask reached = active_dda && (t_precise < maxt) && (tau_next >= desired_tau);
            Mask escaped = active_dda && (dda_t_next >= maxt);

            // Ensure that `dt` breaks out in case of escape, rather than "just reaching" maxt
            dr::masked(dda_t, active_dda) = dr::select(
                reached, t_precise,
                dr::select(escaped, dr::Infinity<Float>, dda_t_next));

            // It's important that DRT can run on the last DDA step,
            // even if it fails to reach `desired_tau`. The DRT step
            // taken will be clamped to the bounds of the medium.
            active_drt = active && (reached || escaped);

            // Prepare for next DDA iteration. Lanes that reached their
            // target optical depth did *not* move to the next cell yet.
            dr::masked(dda_tmax, active_dda && !reached) = dda_tmax + tmax_update;
            dr::masked(tau_acc, active_dda && !reached) = tau_next;
            // -- DDA step done.

            // We'll need to sample a new `desired_tau` in these lanes
            // at the next iteration.
            needs_new_target = active_drt;
            // Output the step needed to reach `desired_tau`, as determined by DDA.
            // If DDA didn't reach `desired_tau` in this iteration, none of the lanes
            // should receive updates below.
            dt = dr::select(active_drt, dda_t - running_t, dr::NaN<Float>);
            local_majorant = dr::select(active_drt, majorant, dr::NaN<Float>);
        } else {
            // Closed form free-flight distance sampling
            local_majorant = global_majorant;
            dt = -dr::log(1 - sampler->next_1d(active)) / local_majorant;
            active_drt = active;
        }
        Float dt_clamped = dr::minimum(dt, maxt - running_t);

        // Reservoir sampling with replacement
        Float current_weight = transmittance * dt_clamped;
        dr::masked(acc_weight, active_drt) = acc_weight + current_weight;

        /* Note: this should always trigger at the first step */ {
            Mask did_interact =
                active_drt &
                ((sampler->next_1d(active) * acc_weight) < current_weight);
            // Adopt step with replacement
            dr::masked(sampled_t, did_interact) = running_t;
            dr::masked(sampled_t_step, did_interact) = dt_clamped;
        }
        dr::masked(sampling_weight, active_drt) = acc_weight;

        // Continue DRT stepping
        dr::masked(running_t, active_drt) = running_t + dt;

        // Recall that replacement is possible in this loop.
        dr::masked(active, active_drt) = active && (running_t < maxt);
        active_drt &= active;

        // The transmittance update below will only be used in
        // subsequent loop iterations, for lanes that are still valid.
        dr::masked(mei_sub.t, active_drt) = running_t;
        dr::masked(mei_sub.p, active_drt) = ray(running_t);
        auto [_1, _2, current_sigma_t] =
            get_scattering_coefficients(mei_sub, active_drt);
        dr::masked(transmittance, active_drt) = dr::detach(
            transmittance * (1.f - (extract_channel(current_sigma_t, channel) /
                                    local_majorant)));
    }

    sampled_t = sampled_t + sampler->next_1d(did_traverse) * sampled_t_step;

    Mask valid_mei = (sampled_t <= maxt);
    mei.t          = dr::select(valid_mei, sampled_t, dr::Infinity<Float>);
    mei.p          = ray(sampled_t);
    // Note: not looking up the sigma_t, majorant, etc to fill-in the medium
    // interaction. This is so that the caller can decide whether to do an
    // attached or detached lookup.

    return { mei, sampling_weight };
}


MI_VARIANT
std::pair<typename Medium<Float, Spectrum>::MediumInteraction3f, Spectrum>
Medium<Float, Spectrum>::sample_interaction_drrt(const Ray3f &ray,
                                                 Sampler *sampler,
                                                 UInt32 channel, Mask _active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::MediumSample, _active);
    if (m_majorant_grid)
        NotImplementedError("sample_interaction_drrt with majorant supergrid");

    auto [mei, mint, maxt, active] = prepare_interaction_sampling(ray, _active);

    // Get global majorant
    // TODO: update to support DDA-based sampling (majorant supergrid).
    auto combined_extinction = get_majorant(mei, active);
    Float m = extract_channel(combined_extinction, channel);
    if (!dr::isfinite(m_control_density))
        Throw("Invalid medium control density: %s", m_control_density);
    Float control           = dr::opaque<Float>(m_control_density);
    const Mask did_traverse = active;

    // Sample proportional to transmittance only using reservoir sampling
    MediumInteraction3f mei_sub = mei;
    Float transmittance = 1.f;
    Float running_t = mint;
    Float acc_weight = 0.f;
    Float sampled_t = dr::NaN<Float>;
    Float sampled_t_step = dr::NaN<Float>;
    Float sampling_weight = dr::NaN<Float>;

    dr::Loop<Mask> loop("Medium::sample_interaction_drrt", active, acc_weight,
                        sampled_t, sampled_t_step, sampling_weight, running_t,
                        mei_sub, transmittance, sampler);
    while (loop(active)) {
        Float dt = -dr::log(1 - sampler->next_1d(active)) / m;
        Float dt_clamped = dr::minimum(dt, maxt - running_t);

        // Reservoir sampling with replacement
        Float current_weight =
            transmittance * (1.f - dr::exp(-control * dt_clamped)) / control;
        acc_weight += current_weight;

        // Note: this will always trigger at the first step
        Mask did_interact =
            (sampler->next_1d(active) * acc_weight) < current_weight;
        // Adopt step with replacement
        dr::masked(sampled_t, active && did_interact) = running_t;
        dr::masked(sampled_t_step, active && did_interact) = dt_clamped;
        dr::masked(sampling_weight, active) = acc_weight;

        // Continue stepping
        running_t += dt;

        dr::masked(mei_sub.t, active) = running_t;
        dr::masked(mei_sub.p, active) = ray(running_t);
        auto [_1, _2, current_sigma_t] =
            get_scattering_coefficients(mei_sub, active);
        Float s = extract_channel(current_sigma_t, channel);
        transmittance *=
            dr::detach((1.f - (s - control) / m) * dr::exp(-control * dt));
        // Recall that replacement is possible in this loop.
        active &= (running_t < maxt);
    }

    Float scale = 1 - dr::exp(-control * sampled_t_step);
    sampled_t = sampled_t - dr::log(1.f - sampler->next_1d(did_traverse) * scale) / control;

    Mask valid_mei   = (sampled_t <= maxt);
    mei.t            = dr::select(valid_mei, sampled_t, dr::Infinity<Float>);
    mei.p            = ray(sampled_t);
    // Note: not looking up the sigma_t, majorant, etc to fill-in the medium
    // interaction. This is so that the caller can decide whether to do an
    // attached or detached lookup.

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
    mei.wi                  = -ray.d;
    mei.sh_frame            = Frame3f(mei.wi);
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
MI_INLINE
std::tuple<Float, typename Medium<Float, Spectrum>::Vector3f,
           typename Medium<Float, Spectrum>::Vector3f>
Medium<Float, Spectrum>::prepare_dda_traversal(const Volume *majorant_grid,
                                               const Ray3f &ray, Float mint,
                                               Float maxt, Mask /*active*/) const {
    const auto &bbox   = majorant_grid->bbox();
    const auto extents = bbox.extents();
    Ray3f local_ray(
        /* o */ (ray.o - bbox.min) / extents,
        /* d */ ray.d / extents, ray.time, ray.wavelengths);
    const ScalarVector3i res  = majorant_grid->resolution();
    Vector3f local_voxel_size = 1.f / res;

    // The id of the first and last voxels hit by the ray
    Vector3i current_voxel(dr::floor(local_ray(mint) / local_voxel_size));
    Vector3i last_voxel(dr::floor(local_ray(maxt) / local_voxel_size));
    // By definition, current and last voxels should be valid voxel indices.
    current_voxel = dr::clamp(current_voxel, 0, res - 1);
    last_voxel    = dr::clamp(last_voxel, 0, res - 1);

    // Increment (in number of voxels) to take at each step
    Vector3i step = dr::select(local_ray.d >= 0, 1, -1);

    // Distance along the ray to the next voxel border from the current position
    Vector3f next_voxel_boundary = (current_voxel + step) * local_voxel_size;
    next_voxel_boundary += dr::select(
        dr::neq(current_voxel, last_voxel) && (local_ray.d < 0), local_voxel_size, 0);

    // Value of ray parameter until next intersection with voxel-border along each axis
    auto ray_nonzero = dr::neq(local_ray.d, 0);
    Vector3f dda_tmax =
        dr::select(ray_nonzero, (next_voxel_boundary - local_ray.o) / local_ray.d,
                   dr::Infinity<Float>);

    // How far along each component of the ray we must move to move by one voxel
    Vector3f dda_tdelta = dr::select(
        ray_nonzero, step * local_voxel_size / local_ray.d, dr::Infinity<Float>);

    // Current ray parameter throughout DDA traversal
    Float dda_t = mint;

    // Note: `t` parameters on the reparametrized ray yield locations on the
    // normalized majorant supergrid in [0, 1]^3. But they are also directly
    // valid parameters on the original ray, yielding positions in the
    // bbox-aligned supergrid.
    return { dda_t, dda_tmax, dda_tdelta };
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
