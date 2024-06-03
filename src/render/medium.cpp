#include <mutex>

#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/volume.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Medium<Float, Spectrum>::Medium() : m_is_homogeneous(false), m_has_spectral_extinction(true) {}

MI_VARIANT Medium<Float, Spectrum>::Medium(const Properties &props) : m_id(props.id()) {
    for (auto &[name, obj] : props.objects(false)) {
        auto *phase = dynamic_cast<PhaseFunction *>(obj.get());
        if (phase) {
            if (m_phase_function)
                Throw("Only a single phase function can be specified per medium");
            m_phase_function = phase;
            props.mark_queried(name);
        }
        auto *emitter = dynamic_cast<Emitter *>(obj.get());
        if (emitter) {
            if (m_emitter)
                Throw("Only a single emitter can be specified per medium");
            m_emitter = emitter;
            props.mark_queried(name);
        }
    }
    if (!m_phase_function) {
        // Create a default isotropic phase function
        m_phase_function =
            PluginManager::instance()->create_object<PhaseFunction>(Properties("isotropic"));
    }
    
    std::string sampling_mode = props.string("medium_sampling_mode", "analogue");
    if (sampling_mode == "analogue") {
        m_medium_sampling_mode = MediumEventSamplingMode::Analogue;
    } else if (sampling_mode == "maximum") {
        m_medium_sampling_mode = MediumEventSamplingMode::Maximum;
    } else if (sampling_mode == "mean") {
        m_medium_sampling_mode = MediumEventSamplingMode::Mean;
    } else {
        Log(Warn, "Event Sampling Mode \"%s\" not recognised, defaulting to \"analogue\" sampling", sampling_mode);
        m_medium_sampling_mode = MediumEventSamplingMode::Analogue;
    }

    m_sample_emitters = props.get<bool>("sample_emitters", true);
    dr::set_attr(this, "use_emitter_sampling", m_sample_emitters);
    dr::set_attr(this, "medium_sampling_mode", m_medium_sampling_mode);
    dr::set_attr(this, "phase_function", m_phase_function.get());
    dr::set_attr(this, "emitter", m_emitter.get());
}

MI_VARIANT Medium<Float, Spectrum>::~Medium() {}

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
        dr::masked(m, dr::eq(channel, 1u)) = combined_extinction[1];
        dr::masked(m, dr::eq(channel, 2u)) = combined_extinction[2];
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

MI_VARIANT
typename Medium<Float, Spectrum>::UnpolarizedSpectrum
Medium<Float, Spectrum>::get_radiance(const MediumInteraction3f &mi,
                                      Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::MediumEvaluate, active);
    if (!is_emitter())
        return dr::zeros<UnpolarizedSpectrum>();
    auto si = dr::zeros<SurfaceInteraction3f>();

    si.p = mi.p;
    si.n = mi.n;
    si.sh_frame = mi.sh_frame;
    si.uv = dr::zeros<Point2f>();
    si.time = mi.time;
    si.t = mi.t;
    si.wavelengths = mi.wavelengths;

    return unpolarized_spectrum(m_emitter->eval(si, active));
}

MI_VARIANT
std::pair<std::pair<typename Medium<Float, Spectrum>::UnpolarizedSpectrum,
                     typename Medium<Float, Spectrum>::UnpolarizedSpectrum>,
           std::pair<typename Medium<Float, Spectrum>::UnpolarizedSpectrum,
                     typename Medium<Float, Spectrum>::UnpolarizedSpectrum>>
Medium<Float, Spectrum>::get_interaction_probabilities(const Spectrum &radiance,
                                                  const MediumInteraction3f &mei,
                                                  const Spectrum &throughput) const {
    UnpolarizedSpectrum prob_scatter, prob_null, c;
    UnpolarizedSpectrum weight_scatter(0.0f), weight_null(0.0f);

    if (m_medium_sampling_mode == MediumEventSamplingMode::Analogue) {
        std::tie(prob_scatter, prob_null) =
            medium_probabilities_analog(unpolarized_spectrum(radiance), mei);
    } else if (m_medium_sampling_mode == MediumEventSamplingMode::Maximum) {
        std::tie(prob_scatter, prob_null) =
            medium_probabilities_max(unpolarized_spectrum(radiance), mei,
                                     unpolarized_spectrum(throughput));
    } else {
        std::tie(prob_scatter, prob_null) =
            medium_probabilities_mean(unpolarized_spectrum(radiance), mei,
                                      unpolarized_spectrum(throughput));
    }

    c                             = prob_scatter + prob_null;
    dr::masked(c, dr::eq(c, 0.f)) = 1.0f;
    prob_scatter /= c;
    prob_null /= c;

    dr::masked(weight_null, prob_null > 0.f)       = dr::rcp(prob_null);
    dr::masked(weight_scatter, prob_scatter > 0.f) = dr::rcp(prob_scatter);

    dr::masked(weight_null,
               dr::neq(weight_null, weight_null) ||
                   !(dr::abs(weight_null) < dr::Infinity<Float>) )    = 0.f;
    dr::masked(weight_scatter,
               dr::neq(weight_scatter, weight_scatter) ||
                   !(dr::abs(weight_scatter) < dr::Infinity<Float>) ) = 0.f;

    return std::make_pair(std::make_pair(prob_scatter, prob_null),
                          std::make_pair(weight_scatter, weight_null));
}

static std::mutex set_dependency_lock_medium;

MI_VARIANT void Medium<Float, Spectrum>::set_emitter(Emitter* emitter) {
    std::unique_lock<std::mutex> guard(set_dependency_lock_medium);
    if (m_emitter)
        Throw("A medium can only have one emitter attached");

    m_emitter = emitter;
    dr::set_attr(this, "emitter", m_emitter.get());
}

MI_IMPLEMENT_CLASS_VARIANT(Medium, Object, "medium")
MI_INSTANTIATE_CLASS(Medium)
NAMESPACE_END(mitsuba)
