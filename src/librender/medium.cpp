#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/volume.h>

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT Medium<Float, Spectrum>::Medium(const Properties &props)
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

    const Float desired_tau = -ek::log(1 - sample);
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
        ek::Loop<Mask> dda_loop("Medium::sample_interaction_dda");
        dda_loop.put(active_dda, reached, dda_t, dda_tmax, tau_acc, mi);
        dda_loop.init();
        while (dda_loop(ek::detach(active_dda))) {
            // Figure out which axis we hit first.
            // `t_next` is the ray's `t` parameter when hitting that axis.
            Float t_next = ek::hmin(dda_tmax);
            Vector3f tmax_update;
            Mask got_assigned = false;
            for (size_t k = 0; k < 3; ++k) {
                Mask active_k = ek::eq(dda_tmax[k], t_next);
                tmax_update[k] = ek::select(!got_assigned && active_k, dda_tdelta[k], 0);
                got_assigned |= active_k;
            }

            // Lookup and accumulate majorant in current cell.
            ek::masked(mi.t, active_dda) = 0.5f * (dda_t + t_next);
            ek::masked(mi.p, active_dda) = ray(mi.t);
            // TODO: avoid this vcall, could lookup directly from the array
            // of floats (but we still need to account for the bbox, etc).
            Float majorant = m_majorant_grid->eval_1(mi, active_dda);
            Float tau_next = tau_acc + majorant * (t_next - dda_t);

            // For rays that will stop within this cell, figure out
            // the precise `t` parameter where `desired_tau` is reached.
            Float t_precise = dda_t + (desired_tau - tau_acc) / majorant;
            reached |= active_dda && (majorant > 0) && (t_precise < maxt) && (tau_next >= desired_tau);
            ek::masked(dda_t, active_dda) = ek::select(reached, t_precise, t_next);

            // Prepare for next iteration
            active_dda &= !reached && (t_next < maxt);
            ek::masked(dda_tmax, active_dda) = dda_tmax + tmax_update;
            ek::masked(tau_acc, active_dda) = tau_next;
        }

        // Adopt the stopping location, making sure to convert to the main
        // ray's parametrization.
        sampled_t = ek::select(reached, dda_t, ek::Infinity<Float>);
    } else {
        // --- A single majorant for the whole volume.
        mi.combined_extinction = ek::detach(get_combined_extinction(mi, active));
        Float m                = extract_channel(mi.combined_extinction, channel);
        sampled_t              = mint + (desired_tau / m);
    }

    Mask valid_mi = active && (sampled_t <= maxt);
    mi.t          = ek::select(valid_mi, sampled_t, ek::Infinity<Float>);
    mi.p          = ray(sampled_t);

    if (m_majorant_grid) {
        // Otherwise it was already looked up above
        mi.combined_extinction = ek::detach(m_majorant_grid->eval_1(mi, valid_mi));
    }
    std::tie(mi.sigma_s, mi.sigma_n, mi.sigma_t) =
        get_scattering_coefficients(mi, valid_mi);
    return mi;
}

MTS_VARIANT
std::pair<typename Medium<Float, Spectrum>::MediumInteraction3f, Spectrum>
Medium<Float, Spectrum>::sample_interaction_real(const Ray3f &ray,
                                                 Sampler *sampler,
                                                 UInt32 channel,
                                                 Mask _active) const {
    MTS_MASKED_FUNCTION(ProfilerPhase::MediumSample, _active);
    // TODO: optimize for RGB by making `channel` a scalar index?
    if (m_majorant_grid)
        NotImplementedError("sample_interaction_real with majorant supergrid");

    auto [mi, mint, maxt, active] = prepare_interaction_sampling(ray, _active);

    MediumInteraction3f mi_next = mi;
    Mask escaped = !active;
    Spectrum weight = ek::full<Spectrum>(1.f, ek::width(ray));
    ek::Loop<Mask> loop("Medium::sample_interaction_real");


    // Get global majorant once and for all
    auto combined_extinction = get_combined_extinction(mi, active);
    mi.combined_extinction   = combined_extinction;
    Float global_majorant = extract_channel(combined_extinction, channel);

    loop.put(active, mi, mi_next, escaped, weight);
    sampler->loop_register(loop);
    loop.init();

    while (loop(ek::detach(active))) {
        // Repeatedly sample from homogenized medium
        Float desired_tau = -ek::log(1 - sampler->next_1d(active));
        Float sampled_t = mi_next.mint + desired_tau / global_majorant;

        Mask valid_mi = active && (sampled_t < maxt);
        mi_next.t     = sampled_t;
        mi_next.p     = ray(sampled_t);
        std::tie(mi_next.sigma_s, mi_next.sigma_n, mi_next.sigma_t) =
            get_scattering_coefficients(mi_next, valid_mi);

        // Determine whether it was a real or null interaction
        Float r = extract_channel(mi_next.sigma_t, channel) / global_majorant;
        Mask did_scatter = valid_mi && (sampler->next_1d(valid_mi) < r);
        mi[did_scatter] = mi_next;

        Spectrum event_pdf = mi_next.sigma_t / combined_extinction;
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
    const Mask did_traverse = active;
    const bool has_supergrid = (bool) m_majorant_grid;

    // Sample proportional to transmittance only using reservoir sampling
    MediumInteraction3f mi_sub  = mi;
    Float transmittance         = 1.f;
    Float running_t             = mint;
    Float acc_weight            = 0.f;
    Float sampled_t             = ek::NaN<Float>;
    Float sampled_t_step        = ek::NaN<Float>;
    Float sampling_weight       = ek::NaN<Float>;

    ek::Loop<Mask> loop("Medium::sample_interaction_drt");
    loop.put(active, acc_weight, sampled_t, sampled_t_step, sampling_weight,
             running_t, mi_sub, transmittance);
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
        global_majorant = ek::NaN<Float>;

        loop.put(dda_t, dda_tmax, needs_new_target, desired_tau, tau_acc);
    } else {
        // Get global majorant
        global_majorant =
            extract_channel(get_combined_extinction(mi, active), channel);
    }

    sampler->loop_register(loop);
    loop.init();
    while (loop(ek::detach(active))) {
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
            ek::masked(desired_tau, active && needs_new_target) =
                -ek::log(1 - sampler->next_1d(active));
            ek::masked(tau_acc, active && needs_new_target) = 0.f;

            // -- One step of DDA
            // Only some of the lanes will reach `desired_tau` at this iteration
            // and be able to proceed with a DRT step.
            Mask active_dda = active;

            // Figure out which axis we hit first.
            // `dda_t_next` is the ray's `t` parameter when hitting that axis.
            Float dda_t_next = ek::hmin(dda_tmax);
            // TODO: this selection scheme looks dangerous in case of exact equality
            //       with more than one component.
            Vector3f tmax_update;
            for (size_t k = 0; k < 3; ++k) {
                tmax_update[k] = ek::select(ek::eq(dda_tmax[k], dda_t_next),
                                            dda_tdelta[k], 0);
            }

            // Lookup and accumulate majorant in current cell.
            ek::masked(mi_sub.t, active_dda) = 0.5f * (dda_t + dda_t_next);
            ek::masked(mi_sub.p, active_dda) = ray(mi_sub.t);
            Float majorant = m_majorant_grid->eval_1(mi_sub, active_dda);
            Float tau_next = tau_acc + majorant * (dda_t_next - dda_t);

            // For rays that will stop within this cell, figure out
            // the precise `t` parameter where `desired_tau` is reached.
            Float t_precise = dda_t + (desired_tau - tau_acc) / majorant;
            Mask reached = active_dda && (t_precise < maxt) && (tau_next >= desired_tau);
            Mask escaped = active_dda && (dda_t_next >= maxt);

            // Ensure that `dt` breaks out in case of escape, rather than "just reaching" maxt
            ek::masked(dda_t, active_dda) = ek::select(
                reached, t_precise,
                ek::select(escaped, ek::Infinity<Float>, dda_t_next));

            // It's important that DRT can run on the last DDA step,
            // even if it fails to reach `desired_tau`. The DRT step
            // taken will be clamped to the bounds of the medium.
            active_drt = active && (reached || escaped);

            // Prepare for next DDA iteration. Lanes that reached their
            // target optical depth did *not* move to the next cell yet.
            ek::masked(dda_tmax, active_dda && !reached) = dda_tmax + tmax_update;
            ek::masked(tau_acc, active_dda && !reached) = tau_next;
            // -- DDA step done.

            // We'll need to sample a new `desired_tau` in these lanes
            // at the next iteration.
            needs_new_target = active_drt;
            // Output the step needed to reach `desired_tau`, as determined by DDA.
            // If DDA didn't reach `desired_tau` in this iteration, none of the lanes
            // should receive updates below.
            dt = ek::select(active_drt, dda_t - running_t, ek::NaN<Float>);
            local_majorant = ek::select(active_drt, majorant, ek::NaN<Float>);
        } else {
            // Closed form free-flight distance sampling
            local_majorant = global_majorant;
            dt = -ek::log(1 - sampler->next_1d(active)) / local_majorant;
            active_drt = active;
        }
        Float dt_clamped = ek::min(dt, maxt - running_t);

        // Reservoir sampling with replacement
        Float current_weight = transmittance * dt_clamped;
        ek::masked(acc_weight, active_drt) = acc_weight + current_weight;

        /* Note: this should always trigger at the first step */ {
            Mask did_interact =
                active_drt &
                ((sampler->next_1d(active) * acc_weight) < current_weight);
            // Adopt step with replacement
            ek::masked(sampled_t, did_interact) = running_t;
            ek::masked(sampled_t_step, did_interact) = dt_clamped;
        }
        ek::masked(sampling_weight, active_drt) = acc_weight;

        // Continue DRT stepping
        ek::masked(running_t, active_drt) = running_t + dt;

        // Recall that replacement is possible in this loop.
        ek::masked(active, active_drt) = active && (running_t < maxt);
        active_drt &= active;

        // The transmittance update below will only be used in
        // subsequent loop iterations, for lanes that are still valid.
        ek::masked(mi_sub.t, active_drt) = running_t;
        ek::masked(mi_sub.p, active_drt) = ray(running_t);
        auto [_1, _2, current_sigma_t] =
            get_scattering_coefficients(mi_sub, active_drt);
        ek::masked(transmittance, active_drt) = ek::detach(
            transmittance * (1.f - (extract_channel(current_sigma_t, channel) /
                                    local_majorant)));
    }
    sampled_t = sampled_t + sampler->next_1d(did_traverse) * sampled_t_step;

    // We expect this to be generally true
    Mask valid_mi   = (sampled_t <= maxt);
    mi.t            = ek::select(valid_mi, sampled_t, ek::Infinity<Float>);
    mi.p            = ray(sampled_t);
    std::tie(mi.sigma_s, mi.sigma_n, mi.sigma_t) =
        get_scattering_coefficients(mi, valid_mi);
    mi.combined_extinction = get_combined_extinction(mi, valid_mi);

    return { mi, sampling_weight };
}

MTS_VARIANT
std::pair<typename Medium<Float, Spectrum>::MediumInteraction3f, Spectrum>
Medium<Float, Spectrum>::sample_interaction_drrt(const Ray3f &ray,
                                                 Sampler *sampler,
                                                 UInt32 channel, Mask _active) const {
    MTS_MASKED_FUNCTION(ProfilerPhase::MediumSample, _active);
    if (m_majorant_grid)
        NotImplementedError("sample_interaction_drrt with majorant supergrid");

    auto [mi, mint, maxt, active] = prepare_interaction_sampling(ray, _active);

    // Get global majorant
    // TODO: update to support DDA-based sampling (majorant supergrid).
    auto combined_extinction = get_combined_extinction(mi, active);
    Float m = extract_channel(combined_extinction, channel);
    // TODO: need to tweak this?
    Float control           = 0.5f * m;
    const Mask did_traverse = active;

    // Sample proportional to transmittance only using reservoir sampling
    MediumInteraction3f mi_sub = mi;
    Float transmittance = 1.f;
    Float running_t = mint;
    Float acc_weight = 0.f;
    Float sampled_t = ek::NaN<Float>;
    Float sampled_t_step = ek::NaN<Float>;
    Float sampling_weight = ek::NaN<Float>;

    ek::Loop<Mask> loop("Medium::sample_interaction_drrt");
    loop.put(active, acc_weight, sampled_t, sampled_t_step, sampling_weight,
             running_t, mi_sub, transmittance);
    sampler->loop_register(loop);
    loop.init();
    while (loop(ek::detach(active))) {
        Float dt = -ek::log(1 - sampler->next_1d(active)) / m;
        Float dt_clamped = ek::min(dt, maxt - running_t);

        // Reservoir sampling with replacement
        Float current_weight =
            transmittance * (1.f - ek::exp(-control * dt_clamped)) / control;
        acc_weight += current_weight;

        // Note: this will always trigger at the first step
        Mask did_interact =
            (sampler->next_1d(active) * acc_weight) < current_weight;
        // Adopt step with replacement
        ek::masked(sampled_t, active && did_interact) = running_t;
        ek::masked(sampled_t_step, active && did_interact) = dt_clamped;
        ek::masked(sampling_weight, active) = acc_weight;

        // Continue stepping
        running_t += dt;

        ek::masked(mi_sub.t, active) = running_t;
        ek::masked(mi_sub.p, active) = ray(running_t);
        auto [_1, _2, current_sigma_t] =
            get_scattering_coefficients(mi_sub, active);
        Float s = extract_channel(current_sigma_t, channel);
        transmittance *=
            ek::detach((1.f - (s - control) / m) * ek::exp(-control * dt));
        // Recall that replacement is possible in this loop.
        active &= (running_t < maxt);
    }

    Float scale = 1 - ek::exp(-control * sampled_t_step);
    sampled_t   = sampled_t -
                ek::log(1.f - sampler->next_1d(did_traverse) * scale) / control;

    Mask valid_mi = (sampled_t <= maxt);
    mi.t          = ek::select(valid_mi, sampled_t, ek::Infinity<Float>);
    mi.p          = ray(sampled_t);
    std::tie(mi.sigma_s, mi.sigma_n, mi.sigma_t) =
        get_scattering_coefficients(mi, valid_mi);
    mi.combined_extinction = get_combined_extinction(mi, valid_mi);

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
MTS_INLINE
std::tuple<typename Medium<Float, Spectrum>::MediumInteraction3f, Float, Float,
           typename Medium<Float, Spectrum>::Mask>
Medium<Float, Spectrum>::prepare_interaction_sampling(const Ray3f &ray,
                                                      Mask active) const {
    // Initialize basic medium interaction fields
    MediumInteraction3f mi = ek::zero<MediumInteraction3f>();
    mi.wi          = -ray.d;
    mi.sh_frame    = Frame3f(mi.wi);
    mi.time        = ray.time;
    mi.wavelengths = ray.wavelengths;
    mi.medium      = this;

    auto [aabb_its, mint, maxt] = intersect_aabb(ray);
    aabb_its &= (ek::isfinite(mint) || ek::isfinite(maxt));
    active &= aabb_its;
    ek::masked(mint, !active) = 0.f;
    ek::masked(maxt, !active) = ek::Infinity<Float>;

    mint    = ek::max(0.f, mint);
    maxt    = ek::min(ray.maxt, maxt);
    mi.mint = mint;

    return std::make_tuple(mi, mint, maxt, active);
}

MTS_VARIANT
MTS_INLINE
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
    Vector3i current_voxel(ek::floor(local_ray(mint) / local_voxel_size));
    Vector3i last_voxel(ek::floor(local_ray(maxt) / local_voxel_size));
    // By definition, current and last voxels should be valid voxel indices.
    current_voxel = ek::clamp(current_voxel, 0, res - 1);
    last_voxel    = ek::clamp(last_voxel, 0, res - 1);

    // Increment (in number of voxels) to take at each step
    Vector3i step = ek::select(local_ray.d >= 0, 1, -1);

    // Distance along the ray to the next voxel border from the current position
    Vector3f next_voxel_boundary = (current_voxel + step) * local_voxel_size;
    next_voxel_boundary += ek::select(
        ek::neq(current_voxel, last_voxel) && (local_ray.d < 0), local_voxel_size, 0);

    // Value of ray parameter until next intersection with voxel-border along each axis
    auto ray_nonzero = ek::neq(local_ray.d, 0);
    Vector3f dda_tmax =
        ek::select(ray_nonzero, (next_voxel_boundary - local_ray.o) / local_ray.d,
                   ek::Infinity<Float>);

    // How far along each component of the ray we must move to move by one voxel
    Vector3f dda_tdelta = ek::select(
        ray_nonzero, step * local_voxel_size / local_ray.d, ek::Infinity<Float>);

    // Current ray parameter throughout DDA traversal
    Float dda_t = mint;

    // Note: `t` parameters on the reparametrized ray yield locations on the
    // normalized majorant supergrid in [0, 1]^3. But they are also directly
    // valid parameters on the original ray, yielding positions in the
    // bbox-aligned supergrid.
    return { dda_t, dda_tmax, dda_tdelta };
}

MTS_VARIANT
MTS_INLINE Float
Medium<Float, Spectrum>::extract_channel(Spectrum value, UInt32 channel) {
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
