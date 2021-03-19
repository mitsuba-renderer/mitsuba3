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

    m_sample_emitters = props.bool_("sample_emitters", true);
    ek::set_attr(this, "use_emitter_sampling", m_sample_emitters);
    ek::set_attr(this, "phase_function", m_phase_function.get());
}

MTS_VARIANT Medium<Float, Spectrum>::~Medium() {}

MTS_VARIANT
typename Medium<Float, Spectrum>::MediumInteraction3f
Medium<Float, Spectrum>::sample_interaction(const Ray3f &ray, Float sample,
                                            UInt32 channel, Mask active) const {
    MTS_MASKED_FUNCTION(ProfilerPhase::MediumSample, active);

    // initialize basic medium interaction fields
    MediumInteraction3f mi;
    mi.wi          = -ray.d;
    mi.sh_frame    = Frame3f(mi.wi);
    mi.time        = ray.time;
    mi.wavelengths = ray.wavelengths;

    auto [aabb_its, mint, maxt] = intersect_aabb(ray);
    aabb_its &= (ek::isfinite(mint) || ek::isfinite(maxt));
    active &= aabb_its;
    ek::masked(mint, !active) = 0.f;
    ek::masked(maxt, !active) = ek::Infinity<Float>;

    mint = ek::max(ray.mint, mint);
    maxt = ek::min(ray.maxt, maxt);

    auto combined_extinction = get_combined_extinction(mi, active);
    Float m                  = combined_extinction[0];
    if constexpr (is_rgb_v<Spectrum>) { // Handle RGB rendering
        ek::masked(m, ek::eq(channel, 1u)) = combined_extinction[1];
        ek::masked(m, ek::eq(channel, 2u)) = combined_extinction[2];
    } else {
        ENOKI_MARK_USED(channel);
    }

    Float sampled_t = mint + (-ek::log(1 - sample) / m);
    Mask valid_mi   = active && (sampled_t <= maxt);
    mi.t            = ek::select(valid_mi, sampled_t, ek::Infinity<Float>);
    mi.p            = ray(sampled_t);
    mi.medium       = this;
    mi.mint         = mint;
    std::tie(mi.sigma_s, mi.sigma_n, mi.sigma_t) =
        get_scattering_coefficients(mi, valid_mi);
    mi.combined_extinction = combined_extinction;
    return mi;
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

MTS_IMPLEMENT_CLASS_VARIANT(Medium, Object, "medium")
MTS_INSTANTIATE_CLASS(Medium)
NAMESPACE_END(mitsuba)
