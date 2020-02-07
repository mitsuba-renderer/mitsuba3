#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT Medium<Float, Spectrum>::Medium() {}

MTS_VARIANT Medium<Float, Spectrum>::Medium(const Properties &props) : m_id(props.id()) {

    for (auto &kv : props.objects()) {
        auto *phase = dynamic_cast<PhaseFunction *>(kv.second.get());
        if (phase) {
            m_phase_function = phase;
        }
    }
    if (!m_phase_function) {
        // Create a default isotropic phase function
        m_phase_function =
            PluginManager::instance()->create_object<PhaseFunction>(Properties("isotropic"));
    }

    m_sample_emitters = props.bool_("sample_emitters", true);
}

MTS_VARIANT Medium<Float, Spectrum>::~Medium() {}

MTS_VARIANT
typename Medium<Float, Spectrum>::MediumInteraction3f
Medium<Float, Spectrum>::sample_interaction(const Ray3f &ray, Float sample,
                                            UInt32 channel, Mask active) const {
    // initialize basic medium interaction fields
    MediumInteraction3f mi;
    mi.t           = math::Infinity<Float>;
    mi.p           = Point3f(0.0f, 0.0f, 0.0f);
    mi.sh_frame    = Frame3f(ray.d);
    mi.wi          = -ray.d;
    mi.time        = ray.time;
    mi.wavelengths = ray.wavelengths;
    mi.medium      = nullptr;
    mi.mint        = 0.f;

    auto [aabb_its, mint, maxt] = intersect_aabb(ray);
    active &= aabb_its;

    mint = max(ray.mint, mint);
    maxt = min(ray.maxt, maxt);

    auto combined_extinction = get_combined_extinction(mi, active);
    Float m                  = combined_extinction[0];
    if constexpr (is_rgb_v<Spectrum>) { // Handle RGB rendering
        masked(m, eq(channel, 1u)) = combined_extinction[1];
        masked(m, eq(channel, 2u)) = combined_extinction[2];
    }

    Float sampled_t        = mint + (-enoki::log(1 - sample) / m);
    Mask valid_mi          = active && (sampled_t <= maxt);
    masked(mi.t, valid_mi) = sampled_t;
    mi.p                   = ray(sampled_t);
    mi.medium              = this;
    mi.mint                = mint;
    std::tie(mi.sigma_s, mi.sigma_n, mi.sigma_t) =
        get_scattering_coefficients(mi, valid_mi);
    mi.combined_extinction = combined_extinction;
    return mi;
}

MTS_VARIANT
std::pair<Spectrum, Spectrum>
Medium<Float, Spectrum>::eval_tr_and_pdf(const MediumInteraction3f &mi,
                                         const SurfaceInteraction3f &si,
                                         Mask active) const {
    Float t      = min(mi.t, si.t) - mi.mint;
    Spectrum tr  = exp(-t * mi.combined_extinction);
    Spectrum pdf = select(si.t < mi.t, tr, tr * mi.combined_extinction);
    return { tr, pdf };
}

MTS_IMPLEMENT_CLASS_VARIANT(Medium, Object, "medium")
MTS_INSTANTIATE_CLASS(Medium)
NAMESPACE_END(mitsuba)
