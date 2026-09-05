#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/random.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/volume.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Medium<Float, Spectrum>::Medium()
    : JitObject<Medium>(""),
      m_is_homogeneous(false),
      m_has_spectral_extinction(true),
      m_majorant_resolution_factor(0),
      m_majorant_factor(1.2f),
      m_majorant_grid_res(0u) {
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

    m_sample_emitters = props.get<bool>("sample_emitters", true);

    /* Majorant supergrid: 0 disables it (single global majorant). Values > 0
       coarsen the extinction volume's native resolution by this factor.
       On by default -- a spatially varying majorant is a large win on any
       volume with empty space, and degrades to the global majorant when the
       extinction is uniform. */
    m_majorant_resolution_factor =
        (uint32_t) props.get<int>("majorant_resolution_factor", 8);
    m_majorant_factor   = props.get<ScalarFloat>("majorant_factor", 1.2f);
    m_majorant_grid_res = ScalarVector3u(0u);
}

MI_VARIANT Medium<Float, Spectrum>::~Medium() { }

MI_VARIANT void Medium<Float, Spectrum>::traverse(TraversalCallback *cb) {
    cb->put("phase_function", m_phase_function, ParamFlags::Differentiable);
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

    UnpolarizedSpectrum combined_extinction;
    Float sampled_t;
    Mask valid_mi;
    if (has_majorant_grid()) {
        /* Spatially-varying majorant: sample against the piecewise-constant
           supergrid with a DDA traversal. The local majorant of the cell
           containing the sample is reported as `combined_extinction`; the
           tr/pdf *ratio* returned by transmittance_eval_pdf() remains exact
           under this convention (the accumulated-optical-depth exponentials
           cancel), which is the only way existing integrators consume it. */
        auto [dda_t, local_majorant, dda_valid] =
            sample_interaction_dda(ray, mint, maxt, sample, active);
        combined_extinction = UnpolarizedSpectrum(local_majorant);
        sampled_t           = dda_t;
        valid_mi            = dda_valid && (sampled_t <= maxt);
        DRJIT_MARK_USED(channel);
    } else {
        combined_extinction = get_majorant(mei, active);
        Float m             = combined_extinction[0];
        if constexpr (is_rgb_v<Spectrum>) { // Handle RGB rendering
            dr::masked(m, channel == 1u) = combined_extinction[1];
            dr::masked(m, channel == 2u) = combined_extinction[2];
        } else {
            DRJIT_MARK_USED(channel);
        }
        sampled_t = mint + (-dr::log(1 - sample) / m);
        valid_mi  = active && (sampled_t <= maxt);
    }
    mei.t           = dr::select(valid_mi, sampled_t, dr::Infinity<Float>);
    mei.p           = ray(sampled_t);
    mei.medium      = this;
    mei.mint        = mint;

    std::tie(mei.sigma_s, mei.sigma_n, mei.sigma_t) =
        get_scattering_coefficients(mei, valid_mi);
    mei.combined_extinction = combined_extinction;
    /* Re-derive the null component against the majorant that was actually
       used for sampling. get_scattering_coefficients() evaluates it against
       a position lookup, which can disagree with the sampling cell's
       majorant when the sampled point lands a floating-point epsilon across
       a supergrid cell face. An inconsistent (sigma_t, sigma_n, kappa)
       triple breaks the delta-tracking invariant sigma_n = kappa - sigma_t
       that integrators rely on: volpath's null-interaction probability
       mean(sigma_n / kappa) evaluates to 0 at such points (where sigma_t is
       also 0), sending the lane into the real-scattering branch and
       producing sigma_s / sigma_t = 0/0 = NaN. With a global majorant the
       two lookups agree and this line is a no-op. */
    mei.sigma_n = dr::maximum(combined_extinction - mei.sigma_t, 0.f);
    return mei;
}

MI_VARIANT void
Medium<Float, Spectrum>::update_majorant_grid(const Volume *volume,
                                              ScalarFloat scale) {
    if (m_majorant_resolution_factor == 0)
        return;

    ScalarVector3u res;
    DynamicBuffer<Float> cells =
        volume->local_majorants(m_majorant_resolution_factor, res);
    m_majorant_grid = cells * (scale * m_majorant_factor);
    dr::eval(m_majorant_grid);
    m_majorant_grid_res = res;
    m_majorant_to_local = volume->to_local();
}

MI_VARIANT Float
Medium<Float, Spectrum>::majorant_grid_eval(const Point3f &p,
                                            Mask active) const {
    Point3f pl = m_majorant_to_local * p;
    Vector3f g = pl * Vector3f(ScalarVector3f(m_majorant_grid_res));
    Vector3i cell =
        dr::clip(Vector3i(dr::floor(g)), 0,
                 Vector3i(ScalarVector3i(m_majorant_grid_res)) - 1);
    UInt32 idx = (UInt32(cell.z()) * m_majorant_grid_res.y() +
                  UInt32(cell.y())) * m_majorant_grid_res.x() +
                 UInt32(cell.x());
    return dr::gather<Float>(m_majorant_grid, idx, active);
}

MI_VARIANT std::tuple<Float, Float, typename Medium<Float, Spectrum>::Mask>
Medium<Float, Spectrum>::sample_interaction_dda(const Ray3f &ray, Float mint,
                                                Float maxt, Float sample,
                                                Mask active) const {
    ScalarVector3f res_f(m_majorant_grid_res);
    ScalarVector3i res_i(m_majorant_grid_res);
    uint32_t rx = m_majorant_grid_res.x(), ry = m_majorant_grid_res.y();

    // Reparameterize the ray segment [mint, maxt] into supergrid cell space
    Point3f  o_g = (m_majorant_to_local * ray(mint)) * Vector3f(res_f);
    Vector3f d_g = (m_majorant_to_local * ray.d) * Vector3f(res_f);

    Float t_end      = maxt - mint;
    Float tau_target = -dr::log(1.f - sample);

    Vector3i step    = dr::select(d_g >= 0.f, 1, -1);
    Vector3f t_delta = dr::rcp(dr::abs(d_g)); // world-t per cell crossing
    Vector3f next_b  = Vector3f(dr::clip(Vector3i(dr::floor(o_g)), 0,
                                         res_i - 1) +
                                dr::select(d_g >= 0.f, Vector3i(1),
                                           Vector3i(0)));
    Vector3f t_max   = dr::select(d_g != 0.f, (next_b - o_g) / d_g,
                                  dr::Infinity<Float>);

    struct DDAState {
        Mask active;
        Vector3i cell;
        Vector3f t_max;
        Float t_cur;
        Float tau_acc;
        Float t_hit;
        Float majorant;

        DRJIT_STRUCT(DDAState, active, cell, t_max, t_cur, tau_acc, t_hit,
                     majorant)
    } ls = {
        active && (t_end > 0.f),
        dr::clip(Vector3i(dr::floor(o_g)), 0, res_i - 1),
        t_max,
        Float(0.f),
        Float(0.f),
        dr::Infinity<Float>,
        Float(0.f)
    };

    dr::tie(ls) = dr::while_loop(dr::make_tuple(ls),
        [](const DDAState &ls) { return ls.active; },
        [this, rx, ry, res_i, t_delta, step, t_end,
         tau_target](DDAState &ls) {
            UInt32 idx = (UInt32(ls.cell.z()) * ry + UInt32(ls.cell.y())) *
                             rx + UInt32(ls.cell.x());
            Float sigma = dr::gather<Float>(m_majorant_grid, idx, ls.active);

            Float t_next = dr::minimum(dr::min(ls.t_max), t_end);
            Float dtau   = sigma * (t_next - ls.t_cur);

            // Sample lands inside the current cell?
            Mask hit = ls.active && (sigma > 0.f) &&
                       (ls.tau_acc + dtau >= tau_target);
            dr::masked(ls.t_hit, hit) =
                ls.t_cur + (tau_target - ls.tau_acc) / sigma;
            dr::masked(ls.majorant, hit) = sigma;
            ls.active &= !hit;

            // Otherwise, accumulate and advance to the neighboring cell
            dr::masked(ls.tau_acc, ls.active) = ls.tau_acc + dtau;
            dr::masked(ls.t_cur, ls.active)   = t_next;
            ls.active &= t_next < t_end;

            Mask ax_x = ls.active && (ls.t_max.x() <= ls.t_max.y()) &&
                        (ls.t_max.x() <= ls.t_max.z());
            Mask ax_y = ls.active && !ax_x && (ls.t_max.y() <= ls.t_max.z());
            Mask ax_z = ls.active && !ax_x && !ax_y;

            dr::masked(ls.cell.x(), ax_x)  = ls.cell.x() + step.x();
            dr::masked(ls.cell.y(), ax_y)  = ls.cell.y() + step.y();
            dr::masked(ls.cell.z(), ax_z)  = ls.cell.z() + step.z();
            dr::masked(ls.t_max.x(), ax_x) = ls.t_max.x() + t_delta.x();
            dr::masked(ls.t_max.y(), ax_y) = ls.t_max.y() + t_delta.y();
            dr::masked(ls.t_max.z(), ax_z) = ls.t_max.z() + t_delta.z();

            // Leaving the supergrid also terminates the walk
            ls.active &= dr::all((ls.cell >= 0) && (ls.cell < res_i));
        });

    Mask valid = active && (ls.majorant > 0.f);
    return { mint + ls.t_hit, ls.majorant, valid };
}

MI_VARIANT
std::tuple<typename Medium<Float, Spectrum>::MediumInteraction3f,
           typename Medium<Float, Spectrum>::UnpolarizedSpectrum, Float>
Medium<Float, Spectrum>::sample_real_interaction_global(const Ray3f &ray,
                                                        Float maxt, UInt32 seed,
                                                        UInt32 channel,
                                                        Mask _active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::MediumSample, _active);

    using PCG32 = mitsuba::PCG32<UInt32>;
    size_t n = dr::width(ray.o);

    struct WalkState {
        Mask active;
        Ray3f ray;
        Float t_acc;
        Float maxt_rem;
        MediumInteraction3f mei;
        UnpolarizedSpectrum weight;
        Float p_scatter;
        PCG32 rng;
        DRJIT_STRUCT(WalkState, active, ray, t_acc, maxt_rem, mei, weight,
                     p_scatter, rng)
    } ls = { _active,
             ray,
             Float(0.f),
             maxt,
             dr::zeros<MediumInteraction3f>(n),
             UnpolarizedSpectrum(1.f),
             Float(1.f),
             PCG32(n, dr::uint64_array_t<Float>(seed)) };
    /* Escaped lanes keep this initial interaction: it must carry valid
       medium/wavelength/time fields (like sample_interaction's miss case)
       so that downstream consumers -- e.g. gradient probes placed on the
       escaping segment -- still evaluate the medium correctly. */
    ls.mei.medium      = this;
    ls.mei.wi          = -ray.d;
    ls.mei.sh_frame    = Frame3f(ls.mei.wi);
    ls.mei.wavelengths = ray.wavelengths;
    ls.mei.time        = ray.time;
    ls.mei.combined_extinction = get_majorant(ls.mei, _active);
    ls.mei.t = dr::Infinity<Float>;

    dr::tie(ls) = dr::while_loop(dr::make_tuple(ls),
        [](const WalkState &ls) { return ls.active; },
        [this, channel](WalkState &ls) {
            Float u1 = ls.rng.next_float32(ls.active);
            MediumInteraction3f mc =
                sample_interaction(ls.ray, u1, channel, ls.active);

            Mask escaped =
                ls.active && (!mc.is_valid() || (mc.t > ls.maxt_rem));

            UnpolarizedSpectrum kappa = mc.combined_extinction;
            Float p = dr::mean(mc.sigma_t / kappa);
            Float u2 = ls.rng.next_float32(ls.active);
            Mask real   = ls.active && !escaped && (u2 < p);
            Mask isnull = ls.active && !escaped && !real;

            // Accept: keep the collision; t measures the total distance
            // walked from the *original* ray origin.
            dr::masked(ls.mei, real)       = mc;
            dr::masked(ls.mei.t, real)     = ls.t_acc + mc.t;
            dr::masked(ls.p_scatter, real) = p;
            dr::masked(ls.weight, real)    = ls.weight * dr::rcp(kappa);

            // Reject: accumulate the null-hop weight and advance the walk
            dr::masked(ls.weight, isnull) =
                ls.weight * mc.sigma_n / (kappa * (1.f - p));
            dr::masked(ls.ray.o, isnull)    = mc.p;
            dr::masked(ls.t_acc, isnull)    = ls.t_acc + mc.t;
            dr::masked(ls.maxt_rem, isnull) = ls.maxt_rem - mc.t;

            ls.active = isnull;
        },
        "Medium::sample_real_interaction_global");

    return { ls.mei, ls.weight, ls.p_scatter };
}

MI_VARIANT
std::tuple<typename Medium<Float, Spectrum>::MediumInteraction3f,
           typename Medium<Float, Spectrum>::UnpolarizedSpectrum, Float>
Medium<Float, Spectrum>::sample_real_interaction(const Ray3f &ray,
                                                       Float maxt_, UInt32 seed,
                                                       UInt32 channel,
                                                       Mask _active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::MediumSample, _active);

    /* The fusion only concerns the majorant supergrid DDA; with a global
       majorant the plain walk is already a single loop. */
    if (!has_majorant_grid())
        return sample_real_interaction_global(ray, maxt_, seed, channel,
                                             _active);
    DRJIT_MARK_USED(channel);

    using PCG32 = mitsuba::PCG32<UInt32>;
    size_t n = dr::width(ray.o);

    auto [aabb_its, mint, maxt] = intersect_aabb(ray);
    aabb_its &= (dr::isfinite(mint) || dr::isfinite(maxt));
    Mask active = _active && aabb_its;
    dr::masked(mint, !active) = 0.f;
    dr::masked(maxt, !active) = ray.maxt;
    mint = dr::maximum(0.f, mint);
    maxt = dr::minimum(maxt_, dr::minimum(ray.maxt, maxt));
    Float t_end = maxt - mint;

    /* Escaped lanes keep this interaction: valid medium/wavelength/time
       fields, t = inf (mirrors sample_interaction's miss convention). */
    MediumInteraction3f mei = dr::zeros<MediumInteraction3f>(n);
    mei.medium      = this;
    mei.wi          = -ray.d;
    mei.sh_frame    = Frame3f(mei.wi);
    mei.wavelengths = ray.wavelengths;
    mei.time        = ray.time;
    mei.mint        = mint;
    mei.combined_extinction = get_majorant(mei, _active);
    mei.t = dr::Infinity<Float>;

    ScalarVector3f res_f(m_majorant_grid_res);
    ScalarVector3i res_i(m_majorant_grid_res);
    uint32_t rx = m_majorant_grid_res.x(), ry = m_majorant_grid_res.y();

    Point3f  o_g = (m_majorant_to_local * ray(mint)) * Vector3f(res_f);
    Vector3f d_g = (m_majorant_to_local * ray.d) * Vector3f(res_f);
    Vector3i step    = dr::select(d_g >= 0.f, 1, -1);
    Vector3f t_delta = dr::rcp(dr::abs(d_g));
    Vector3f next_b  = Vector3f(dr::clip(Vector3i(dr::floor(o_g)), 0,
                                         res_i - 1) +
                                dr::select(d_g >= 0.f, Vector3i(1),
                                           Vector3i(0)));
    Vector3f t_max0  = dr::select(d_g != 0.f, (next_b - o_g) / d_g,
                                  dr::Infinity<Float>);

    struct FusedState {
        Mask active;
        Mask needs_tau;
        Vector3i cell;
        Vector3f t_max;
        Float t_cur;
        Float tau_target;
        Float tau_acc;
        UnpolarizedSpectrum weight;
        Float p_scatter;
        Float t_hit;
        Float kappa_hit;
        UnpolarizedSpectrum ss, sn, st;
        Mask found;
        PCG32 rng;
        DRJIT_STRUCT(FusedState, active, needs_tau, cell, t_max, t_cur,
                     tau_target, tau_acc, weight, p_scatter, t_hit, kappa_hit,
                     ss, sn, st, found, rng)
    } ls = { active && (t_end > 0.f),
             Mask(true),
             dr::clip(Vector3i(dr::floor(o_g)), 0, res_i - 1),
             t_max0,
             Float(0.f),
             Float(0.f),
             Float(0.f),
             UnpolarizedSpectrum(1.f),
             Float(1.f),
             dr::Infinity<Float>,
             Float(0.f),
             UnpolarizedSpectrum(0.f), UnpolarizedSpectrum(0.f),
             UnpolarizedSpectrum(0.f),
             Mask(false),
             PCG32(n, dr::uint64_array_t<Float>(seed)) };

    Ray3f ray_l(ray);
    Float mint_l(mint);

    dr::tie(ls) = dr::while_loop(dr::make_tuple(ls),
        [](const FusedState &ls) { return ls.active; },
        [this, rx, ry, res_i, step, t_delta, t_end, ray_l,
         mint_l](FusedState &ls) {
            // (1) Draw a fresh optical-depth target where needed
            Float u_tau = ls.rng.next_float32(ls.active);
            Mask fresh = ls.active && ls.needs_tau;
            dr::masked(ls.tau_target, fresh) = -dr::log(1.f - u_tau);
            dr::masked(ls.tau_acc, fresh)    = 0.f;
            dr::masked(ls.needs_tau, ls.active) = false;

            // (2) Extent and majorant of the current cell
            UInt32 idx = (UInt32(ls.cell.z()) * ry + UInt32(ls.cell.y())) *
                             rx + UInt32(ls.cell.x());
            Float sigma  = dr::gather<Float>(m_majorant_grid, idx, ls.active);
            Float t_next = dr::minimum(dr::min(ls.t_max), t_end);
            Float dtau   = sigma * (t_next - ls.t_cur);
            Mask reach   = ls.active && (sigma > 0.f) &&
                           (ls.tau_acc + dtau >= ls.tau_target);

            // (3) Tracking event inside the current cell: accept or reject
            //     in place (the DDA cursor does not restart on rejection).
            Float t_evt = ls.t_cur + (ls.tau_target - ls.tau_acc) / sigma;
            MediumInteraction3f mc = dr::zeros<MediumInteraction3f>();
            mc.medium      = this;
            mc.wavelengths = ray_l.wavelengths;
            mc.time        = ray_l.time;
            mc.p           = ray_l(mint_l + t_evt);
            auto [ss, sn, st] = get_scattering_coefficients(mc, reach);
            UnpolarizedSpectrum kappa(sigma);
            /* Keep the delta-tracking invariant against the majorant that was
               actually used, as sample_interaction() does. */
            sn          = dr::maximum(kappa - st, 0.f);
            Float p     = dr::mean(st / kappa);
            Float u_acc = ls.rng.next_float32(ls.active);
            Mask real   = reach && (u_acc < p);
            Mask isnull = reach && !real;

            dr::masked(ls.found, real)      = true;
            dr::masked(ls.t_hit, real)      = t_evt;
            dr::masked(ls.kappa_hit, real)  = sigma;
            dr::masked(ls.p_scatter, real)  = p;
            dr::masked(ls.weight, real)     = ls.weight * dr::rcp(kappa);
            dr::masked(ls.ss, real) = ss;
            dr::masked(ls.sn, real) = sn;
            dr::masked(ls.st, real) = st;

            dr::masked(ls.weight, isnull) =
                ls.weight * sn / (kappa * (1.f - p));
            dr::masked(ls.t_cur, isnull)     = t_evt;
            dr::masked(ls.needs_tau, isnull) = true;

            // (4) No event in this cell: accumulate and advance the DDA
            Mask adv = ls.active && !reach;
            dr::masked(ls.tau_acc, adv) = ls.tau_acc + dtau;
            dr::masked(ls.t_cur, adv)   = t_next;
            Mask ax_x = adv && (ls.t_max.x() <= ls.t_max.y()) &&
                        (ls.t_max.x() <= ls.t_max.z());
            Mask ax_y = adv && !ax_x && (ls.t_max.y() <= ls.t_max.z());
            Mask ax_z = adv && !ax_x && !ax_y;
            dr::masked(ls.cell.x(), ax_x)  = ls.cell.x() + step.x();
            dr::masked(ls.cell.y(), ax_y)  = ls.cell.y() + step.y();
            dr::masked(ls.cell.z(), ax_z)  = ls.cell.z() + step.z();
            dr::masked(ls.t_max.x(), ax_x) = ls.t_max.x() + t_delta.x();
            dr::masked(ls.t_max.y(), ax_y) = ls.t_max.y() + t_delta.y();
            dr::masked(ls.t_max.z(), ax_z) = ls.t_max.z() + t_delta.z();

            Mask out = adv && ((t_next >= t_end) ||
                               !dr::all((ls.cell >= 0) && (ls.cell < res_i)));
            ls.active = ls.active && !real && !out;
        },
        "Medium::sample_real_interaction");

    dr::masked(mei.t, ls.found) = mint + ls.t_hit;
    dr::masked(mei.p, ls.found) = ray(mint + ls.t_hit);
    dr::masked(mei.combined_extinction, ls.found) =
        UnpolarizedSpectrum(ls.kappa_hit);
    dr::masked(mei.sigma_s, ls.found) = ls.ss;
    dr::masked(mei.sigma_n, ls.found) = ls.sn;
    dr::masked(mei.sigma_t, ls.found) = ls.st;

    return { mei, ls.weight, ls.p_scatter };
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

MI_VARIANT
typename Medium<Float, Spectrum>::UnpolarizedSpectrum
Medium<Float, Spectrum>::get_albedo(const MediumInteraction3f &mi,
                                    Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);

    /* Fallback only — returns 0 where sigma_t = 0, which is generally NOT
       the medium's albedo there. Differentiable integrators that attach the
       scatter term through this value lose d(sigma_t*albedo)/dsigma_t =
       albedo in empty regions (see prbvolpath_sm). Media with an explicit
       albedo (heterogeneous, homogeneous) override this with a direct
       volume/value lookup; new Medium subclasses should do the same. */
    auto [sigma_s, sigma_n, sigma_t] = get_scattering_coefficients(mi, active);
    return dr::select(sigma_t > 0.f, sigma_s / sigma_t, 0.f);
}

MI_IMPLEMENT_TRAVERSE_CB(Medium, Object)
MI_INSTANTIATE_CLASS(Medium)
NAMESPACE_END(mitsuba)
