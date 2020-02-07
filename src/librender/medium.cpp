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
std::tuple<typename Medium<Float, Spectrum>::SurfaceInteraction3f,
           typename Medium<Float, Spectrum>::MediumInteraction3f, Spectrum>
Medium<Float, Spectrum>::sample_interaction(const Scene *scene,
                                            const Ray3f &ray, Float sample,
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

    Spectrum transmittance(1.0f);

    // make sure that the ray doesn't miss the medium (via m_density_aabb)
    auto [aabb_its, mint, maxt] = intersect_aabb(ray);

    SurfaceInteraction3f si;
    Mask missed = active && !aabb_its;

    // TODO: Group all ray intersect calls
    // If medium missed: still compute surface interaction (for now)
    masked(si, missed) = scene->ray_intersect(ray, missed);
    active &= aabb_its;

    mint = max(ray.mint, mint);
    maxt = min(ray.maxt, maxt);

    // Sample distance to next scattering event
    auto combined_extinction = get_combined_extinction(mi, active);
    Float m = combined_extinction[0];
    if constexpr (is_rgb_v<Spectrum>) { // Handle RGB rendering
        masked(m, eq(channel, 1u)) = combined_extinction[1];
        masked(m, eq(channel, 2u)) = combined_extinction[2];
    }

    Float sampled_t = mint + (-enoki::log(1 - sample) / m);

    Ray ray_its = ray;
    Mask inside_bbox = active && (sampled_t <= maxt);
    // restrict ray maxt if inside the medium's bounding box
    masked(ray_its.maxt, inside_bbox) = sampled_t;
    masked(si, active) = scene->ray_intersect(ray_its, active);
    Float t = min(si.t, maxt) - mint;

    // Escaped medium
    masked(t, inside_bbox && si.is_valid()) = si.t - mint;

    // Fill MediumInteraction if we sampled a vaid medium interaction
    Mask valid_mi = inside_bbox && !si.is_valid();
    masked(t, valid_mi) = sampled_t - mint;
    masked(mi.t, valid_mi) = sampled_t;
    mi.p = ray(sampled_t);
    mi.medium = this;
    std::tie(mi.sigma_s, mi.sigma_n, mi.sigma_t) = get_scattering_coefficients(mi, valid_mi);
    mi.combined_extinction = combined_extinction;

    masked(transmittance, active) *= exp(-t * combined_extinction);
    return std::make_tuple(si, mi, transmittance);
}

MTS_IMPLEMENT_CLASS_VARIANT(Medium, Object, "medium")
MTS_INSTANTIATE_CLASS(Medium)
NAMESPACE_END(mitsuba)
