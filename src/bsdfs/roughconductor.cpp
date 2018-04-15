#include <mitsuba/core/string.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/reflection.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>

NAMESPACE_BEGIN(mitsuba)

class RoughConductor : public BSDF {
public:
    RoughConductor(const Properties &props) {
        Properties props2("uniform");
        props2.set_float("value", 1.f / lookup_ior(props, "ext_eta", "air"));
        m_inv_ext_eta = (ContinuousSpectrum *) PluginManager::instance()
            ->create_object<ContinuousSpectrum>(props2).get();

        m_eta = props.spectrum("eta", 0.f);
        m_k   = props.spectrum("k", 1.f);

        MicrofacetDistribution<Float> distr(props);
        m_type = distr.type();
        m_sample_visible = distr.sample_visible();

        m_flags = (EGlossyReflection | EFrontSide);

        m_alpha_u = distr.alpha_u();
        if (distr.alpha_u() == distr.alpha_v())
            m_alpha_v = m_alpha_u;
        else {
            m_alpha_v = distr.alpha_v();
            m_flags |= EAnisotropic;
        }

        m_components.push_back(m_flags);
        m_needs_differentials = false;
    }

    template <typename SurfaceInteraction,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Value      = typename SurfaceInteraction::Value,
              typename Point2     = typename SurfaceInteraction::Point2,
              typename Spectrum   = Spectrum<Value>>
    std::pair<BSDFSample, Spectrum> sample_impl(
            const SurfaceInteraction &si, const BSDFContext &ctx,
            Value /*sample1*/, const Point2 &sample2, mask_t<Value> active) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Normal3 = typename BSDFSample::Normal3;
        using Frame = mitsuba::Frame<Vector3>;

        BSDFSample bs;
        Value n_dot_wi = Frame::cos_theta(si.wi);
        active = active && (n_dot_wi > 0.0f);

        if (!ctx.is_enabled(EGlossyReflection) || none(active))
            return { bs, 0.0f };

        /* Construct the microfacet distribution matching the roughness
           values at the current surface position. */
        MicrofacetDistribution<Value> distr(m_type, m_alpha_u, m_alpha_v,
                                            m_sample_visible);

        // Sample M, the microfacet normal
        Normal3 m;
        std::tie(m, bs.pdf) = distr.sample(si.wi, sample2, active);

        active = active && neq(bs.pdf, 0.0f);

        // Perfect specular reflection based on the microfacet normal
        bs.wi = si.wi;
        bs.wo = reflect(si.wi, m);
        bs.eta = 1.0f;
        bs.sampled_component = 0;
        bs.sampled_type = EGlossyReflection;

        // Side check
        active = active && (Frame::cos_theta(bs.wo) > 0.0f);

        if (none(active))
            return { bs, 0.0f };

        // Fresnel factor
        Spectrum eta = m_eta->eval(si.wavelengths, active);
        Spectrum inv_ext_eta = m_inv_ext_eta->eval(si.wavelengths, active);
        Spectrum k = m_k->eval(si.wavelengths);

        const Spectrum F = fresnel_conductor_exact(
            dot(si.wi, m), eta * inv_ext_eta, k * inv_ext_eta
        );

        Value weight = 0.0f;
        if (m_sample_visible)
            masked(weight, active) = distr.smith_g1(bs.wo, m, active);
        else
            masked(weight, active) = distr.eval(m, active)
                                     * distr.G(si.wi, bs.wo, m, active)
                                     * dot(si.wi, m) / (bs.pdf * n_dot_wi);

        // Jacobian of the half-direction mapping
        bs.pdf /= 4.0f * dot(bs.wo, m);

        return { bs, F * weight };
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3,
              typename Spectrum = Spectrum<Value>>
        Spectrum eval_impl(const SurfaceInteraction &si, const BSDFContext &ctx,
                           const Vector3 &wo, mask_t<Value> active) const {
        using Frame = mitsuba::Frame<Vector3>;

        if (!ctx.is_enabled(EGlossyReflection))
            return 0.0f;

        Value n_dot_wi = Frame::cos_theta(si.wi);
        Value n_dot_wo = Frame::cos_theta(wo);

        active = active && ((n_dot_wi > 0.0f) && (n_dot_wo > 0.0f));

        if (none(active))
            return 0.0f;

        // Calculate the reflection half-vector
        Vector3 H = normalize(wo + si.wi);

        // Construct the microfacet distribution matching the
        // roughness values at the current surface position.
        MicrofacetDistribution<Value> distr(m_type, m_alpha_u, m_alpha_v,
                                            m_sample_visible);

        // Evaluate the microfacet normal distribution
        const Value D = distr.eval(H, active);

        active = active && neq(D, 0.0f);

        if (none(active))
            return 0.0f;

        // Fresnel factor
        Spectrum eta = m_eta->eval(si.wavelengths, active);
        Spectrum inv_ext_eta = m_inv_ext_eta->eval(si.wavelengths, active);
        Spectrum k = m_k->eval(si.wavelengths);
        const Spectrum F = fresnel_conductor_exact(
            dot(si.wi, H), eta * inv_ext_eta, k * inv_ext_eta
        );

        // Smith's shadow-masking function
        const Value G = distr.G(si.wi, wo, H, active);

        // Calculate the total amount of reflection
        Value model = D * G / (4.0f * n_dot_wi);

        Spectrum result(0.f);
        masked(result, active) = F * model;

        return result;
    }

    template <typename SurfaceInteraction,
              typename Value   = typename SurfaceInteraction::Value,
              typename Vector3 = typename SurfaceInteraction::Vector3>
    Value pdf_impl(const SurfaceInteraction &si, const BSDFContext &ctx,
                   const Vector3 &wo, mask_t<Value> active) const {
        using Frame = mitsuba::Frame<Vector3>;

        if (!ctx.is_enabled(EGlossyReflection))
            return 0.0f;

        Value n_dot_wi = Frame::cos_theta(si.wi);
        Value n_dot_wo = Frame::cos_theta(wo);

        active = active && ((n_dot_wi > 0.0f) && (n_dot_wo > 0.0f));

        if (none(active))
            return 0.0f;

        // Calculate the reflection half-vector
        Vector3 H = normalize(wo + si.wi);

        // Construct the microfacet distribution matching the
        // roughness values at the current surface position.
        MicrofacetDistribution<Value> distr(m_type, m_alpha_u, m_alpha_v,
                                            m_sample_visible);

        Value result(0.0f);
        if (m_sample_visible)
            masked(result, active) = distr.eval(H, active)
                                     * distr.smith_g1(si.wi, H, active)
                                     / (4.0f * n_dot_wi);
        else
            masked(result, active) = distr.pdf(si.wi, H, active)
                                     / (4 * abs_dot(wo, H));

        return result;
    }


    std::string to_string() const override {
        std::ostringstream oss;
        oss << "RoughConductor[" << std::endl
            << "  distribution = " << MicrofacetDistribution<Float>::distribution_name(m_type) << "," << std::endl
            << "  sample_visible = " << m_sample_visible << "," << std::endl
            << "  alpha_u = " << m_alpha_u << "," << std::endl
            << "  alpha_v = " << m_alpha_v << "," << std::endl
            << "  eta = " << m_eta << "," << std::endl
            << "  k = " << m_k << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_BSDF()
    MTS_DECLARE_CLASS()
private:
    EType m_type;
    Float m_alpha_u, m_alpha_v;
    bool m_sample_visible;
    /// Relative refractive index (real component) and its inverse.
    ref<ContinuousSpectrum> m_eta;
    ref<ContinuousSpectrum> m_inv_ext_eta;
    /// Relative refractive index (imaginary component).
    ref<ContinuousSpectrum> m_k;
};

MTS_IMPLEMENT_CLASS(RoughConductor, BSDF)
MTS_EXPORT_PLUGIN(RoughConductor, "Rough conductor BRDF");

NAMESPACE_END(mitsuba)
