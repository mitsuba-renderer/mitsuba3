
#include <mitsuba/core/string.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>

NAMESPACE_BEGIN(mitsuba)
namespace {
/// Helper function: reflect \c wi with respect to a given surface normal
template <typename Vector3, typename Normal3>
inline Vector3 reflect(const Vector3 &wi, const Normal3 &m) {
    return 2.0f * dot(wi, m) * Vector3(m) - wi;
}

template <typename Spectrum, typename Value = typename Spectrum::Scalar>
Spectrum fresnel_conductor_exact(Value cos_theta_i, Spectrum eta, Spectrum k) {
    // Modified from "Optics" by K.D. Moeller, University Science Books, 1988

    Value cos_theta_i_2 = cos_theta_i * cos_theta_i,
        sin_theta_i_2 = 1.f - cos_theta_i_2,
        sin_theta_i_4 = sin_theta_i_2 * sin_theta_i_2;

    Spectrum temp_1 = eta * eta - k * k - Spectrum(sin_theta_i_2),
        a_2_pb_2 = safe_sqrt(temp_1*temp_1 + 4.f * k*k*eta*eta),
        a = safe_sqrt(0.5f * (a_2_pb_2 + temp_1));

    Spectrum term_1 = a_2_pb_2 + Spectrum(cos_theta_i_2),
        term_2 = 2.f * cos_theta_i * a;

    Spectrum rs_2 = (term_1 - term_2) / (term_1 + term_2);

    Spectrum term_3 = a_2_pb_2 * cos_theta_i_2 + Spectrum(sin_theta_i_4),
        term_4 = term_2 * sin_theta_i_2;

    Spectrum rp_2 = rs_2 * (term_3 - term_4) / (term_3 + term_4);

    return 0.5f * (rp_2 + rs_2);
}
}  // namespace

class RoughConductor : public BSDF {
public:
    RoughConductor(const Properties &props) {
        m_flags = EGlossyReflection | EFrontSide;

        m_inv_ext_eta = 1.f / lookup_ior(props, "ext_eta", "air");

        m_eta = props.spectrum("eta", 0.f);
        m_k   = props.spectrum("k", 1.f);

        MicrofacetDistribution<Float> distr(props);
        m_type = distr.type();
        m_sample_visible = distr.sample_visible();

        m_alpha_u = distr.alpha_u();
        if (distr.alpha_u() == distr.alpha_v())
            m_alpha_v = m_alpha_u;
        else
            m_alpha_v = distr.alpha_v();
    }

    template <typename BSDFSample, typename Value  = typename BSDFSample::Value,
              typename Point2 = typename BSDFSample::Point2,
              typename Spectrum = Spectrum<Value>>
    std::pair<Spectrum, Value> sample_impl(BSDFSample &bs,
                                           const Point2 &sample,
                                           const mask_t<Value> &active_) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Normal3 = typename BSDFSample::Normal3;
        using Frame = Frame<Vector3>;
        using Mask = mask_t<Value>;

        if ((bs.component != -1 && bs.component != 0)
            || !(bs.type_mask & EGlossyReflection))
            return { 0.0f, 0.0f };

        Mask active(active_);

        Value n_dot_wi = Frame::cos_theta(bs.wi);
        active = active && !(n_dot_wi <= 0.0f);

        if (none(active))
            return { 0.0f, 0.0f };

        // Construct the microfacet distribution matching the
        // roughness values at the current surface position.
        MicrofacetDistribution<Value> distr(m_type, m_alpha_u, m_alpha_v,
                                            m_sample_visible);

        // Sample M, the microfacet normal
        Value pdf;
        Normal3 m;
        std::tie(m, pdf) = distr.sample(bs.wi, sample, active);

        active = active && !eq(pdf, 0.0f);

        // Perfect specular reflection based on the microfacet normal
        masked(bs.wo,  active) = reflect(bs.wi, m);
        masked(bs.eta, active) = 1.0f;
        masked(bs.sampled_component, active) = 0;
        masked(bs.sampled_type, active) = EGlossyReflection;

        active = active && !(Frame::cos_theta(bs.wo) <= 0.0f);

        // Side check
        if (none(active))
            return { 0.0f, 0.0f };

        const Spectrum F = fresnel_conductor_exact(
            dot(bs.wi, m), Spectrum(m_eta * m_inv_ext_eta),
            Spectrum(m_k * m_inv_ext_eta)
        );

        Value weight;
        if (m_sample_visible)
            weight = distr.smith_g1(bs.wo, m, active);
        else
            weight = distr.eval(m, active) * distr.G(bs.wi, bs.wo, m, active)
                     * dot(bs.wi, m) / (pdf * n_dot_wi);

        // Jacobian of the half-direction mapping
        pdf /= 4.0f * dot(bs.wo, m);

        masked(pdf,    !active) = 0.0f;
        masked(weight, !active) = 0.0f;

        return { F * weight, pdf };
    }

    template <typename BSDFSample,
              typename Value = typename BSDFSample::Value,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const BSDFSample &bs, EMeasure measure,
                       const mask_t<Value> & active_) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        // Stop if this component was not requested
        if ((bs.component != -1 && bs.component != 0)
            || !(bs.type_mask & EGlossyReflection)
            || measure != ESolidAngle)
            return 0.0f;

        Mask active(active_);

        Value n_dot_wi = Frame::cos_theta(bs.wi);
        Value n_dot_wo = Frame::cos_theta(bs.wo);

        active = active && !((n_dot_wi <= 0.0f) & (n_dot_wo <= 0.0f));

        if (none(active))
            return 0.0f;

        // Calculate the reflection half-vector
        Vector3 H = normalize(bs.wo + bs.wi);

        // Construct the microfacet distribution matching the
        // roughness values at the current surface position.
        MicrofacetDistribution<Value> distr(m_type, enoki::mean(m_alpha_u),
                                            enoki::mean(m_alpha_v), m_sample_visible);

        // Evaluate the microfacet normal distribution
        const Value D = distr.eval(H, active);

        active = active && neq(D, 0.0f);

        if (none(active))
            return 0.0f;

        // Fresnel factor
        const Spectrum F = fresnel_conductor_exact(
            dot(bs.wi, H), Spectrum(m_eta * m_inv_ext_eta),
            Spectrum(m_k * m_inv_ext_eta)
        );

        // Smith's shadow-masking function
        const Value G = distr.G(bs.wi, bs.wo, H, active);

        // Calculate the total amount of reflection
        Value model = D * G / (4.0f * Frame::cos_theta(bs.wi));

        Spectrum result(0.f);
        masked(result, active) = F * model;

        return result;
    }

    template <typename BSDFSample,
              typename Value = typename BSDFSample::Value>
    Value pdf_impl(const BSDFSample &bs, EMeasure measure,
                   const mask_t<Value> &active_) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        Mask active(active_);

        // Stop if this component was not requested
        if ((bs.component != -1 && bs.component != 0)
            || !(bs.type_mask & EGlossyReflection)
            || measure != ESolidAngle)
            return 0.0f;

        Value n_dot_wi = Frame::cos_theta(bs.wi);
        Value n_dot_wo = Frame::cos_theta(bs.wo);

        active = active && !((n_dot_wi <= 0.0f) & (n_dot_wo <= 0.0f));

        if (none(active))
            return 0.0f;

        // Calculate the reflection half-vector
        Vector3 H = normalize(bs.wo + bs.wi);

        // Construct the microfacet distribution matching the
        // roughness values at the current surface position.
        MicrofacetDistribution<Value> distr(m_type, m_alpha_u, m_alpha_v,
                                            m_sample_visible);

        Value result(0.0f);
        if (m_sample_visible)
            masked(result, active) = distr.eval(H, active)
                                     * distr.smith_g1(bs.wi, H, active) / (4.0f * n_dot_wi);
        else
            masked(result, active) = distr.pdf(bs.wi, H, active) / (4 * abs_dot(bs.wo, H));

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
    ref<ContinuousSpectrum> m_eta, m_k;
    Float m_inv_ext_eta;
};

MTS_IMPLEMENT_CLASS(RoughConductor, BSDF)
MTS_EXPORT_PLUGIN(RoughConductor, "Rough conductor BRDF");

NAMESPACE_END(mitsuba)
