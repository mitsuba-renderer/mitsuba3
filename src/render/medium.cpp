#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Medium<Float, Spectrum>::Medium() : 
    m_is_homogeneous(false), 
    m_has_spectral_extinction(true) {

    if constexpr (dr::is_jit_v<Float>)
        jit_registry_put(dr::backend_v<Float>, "mitsuba::Medium", this);
}

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

    if constexpr (dr::is_jit_v<Float>)
        jit_registry_put(dr::backend_v<Float>, "mitsuba::Medium", this);
}

MI_VARIANT Medium<Float, Spectrum>::~Medium() {
    if constexpr (dr::is_jit_v<Float>)
        jit_registry_remove(this);
}

MI_VARIANT void Medium<Float, Spectrum>::traverse(TraversalCallback *callback) {
    callback->put_object("phase_function", m_phase_function.get(), +ParamFlags::Differentiable);
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

MI_IMPLEMENT_CLASS_VARIANT(Medium, Object, "medium")
MI_INSTANTIATE_CLASS(Medium)
NAMESPACE_END(mitsuba)
