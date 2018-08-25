#include <mitsuba/core/string.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

class RoughConductor : public BSDF {
public:
    RoughConductor(const Properties &props) {
        m_eta = props.spectrum("eta", 0.f);
        m_k   = props.spectrum("k", 1.f);

        MicrofacetDistribution<Float> distr(props);
        m_type = distr.type();
        m_sample_visible = distr.sample_visible();

        m_flags = EGlossyReflection | EFrontSide;

        m_alpha_u = m_alpha_v = distr.alpha_u();
        if (distr.is_anisotropic()) {
            m_alpha_v = distr.alpha_v();
            m_flags |= EAnisotropic;
        }

        m_components.push_back(m_flags);
    }

    template <typename SurfaceInteraction, typename Value, typename Point2,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Spectrum   = Spectrum<Value>>
    MTS_INLINE
    std::pair<BSDFSample, Spectrum> sample_impl(const BSDFContext &ctx,
                                                const SurfaceInteraction &si,
                                                Value /* sample1 */,
                                                const Point2 & sample2,
                                                mask_t<Value> active) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Normal3 = typename BSDFSample::Normal3;
        using Frame = mitsuba::Frame<Vector3>;

        BSDFSample bs;
        Value cos_theta_i = Frame::cos_theta(si.wi);
        active &= cos_theta_i > 0.f;

        if (unlikely(!ctx.is_enabled(EGlossyReflection) || none(active)))
            return { bs, 0.f };

        /* Construct a microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution<Value> distr(m_type, m_alpha_u, m_alpha_v,
                                            m_sample_visible);

        /* Sample M, the microfacet normal */
        Normal3 m;
        std::tie(m, bs.pdf) = distr.sample(si.wi, sample2);

        /* Perfect specular reflection based on the microfacet normal */
        bs.wo = reflect(si.wi, m);
        bs.eta = 1.f;
        bs.sampled_component = 0;
        bs.sampled_type = EGlossyReflection;

        /* Ensure that this is a valid sample */
        active &= neq(bs.pdf, 0.f) && Frame::cos_theta(bs.wo) > 0.f;

        Value weight;
        if (likely(m_sample_visible))
            weight = distr.smith_g1(bs.wo, m);
        else
            weight = distr.G(si.wi, bs.wo, m) * dot(si.wi, m) /
                     (cos_theta_i * Frame::cos_theta(m));

        /* Evaluate the Fresnel factor */
        Complex<Spectrum> eta_c(m_eta->eval(si, active),
                                m_k->eval(si, active));

        Spectrum F = fresnel_conductor(Spectrum(dot(si.wi, m)), eta_c);

        /* Jacobian of the half-direction mapping */
        bs.pdf /= 4.f * dot(bs.wo, m);

        return { bs, (F * weight) & active };
    }

    template <typename SurfaceInteraction, typename Vector3,
              typename Value    = typename SurfaceInteraction::Value,
              typename Spectrum = Spectrum<Value>>
    MTS_INLINE
    Spectrum eval_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                       const Vector3 &wo, mask_t<Value> active) const {
        using Frame = mitsuba::Frame<Vector3>;

        Value cos_theta_i = Frame::cos_theta(si.wi),
              cos_theta_o = Frame::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely(!ctx.is_enabled(EGlossyReflection) || none(active)))
            return 0.f;

        /* Calculate the half-direction vector */
        Vector3 H = normalize(wo + si.wi);

        /* Construct a microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution<Value> distr(m_type, m_alpha_u, m_alpha_v,
                                            m_sample_visible);

        /* Evaluate the microfacet normal distribution */
        Value D = distr.eval(H);

        active &= neq(D, 0.f);

        /* Evaluate the Fresnel factor */
        Complex<Spectrum> eta_c(m_eta->eval(si, active),
                                m_k->eval(si, active));
        Spectrum F = fresnel_conductor(Spectrum(dot(si.wi, H)), eta_c);

        /* Evaluate Smith's shadow-masking function */
        Value G = distr.G(si.wi, wo, H);

        /* Evaluate the full microfacet model (except Fresnel) */
        Value result = D * G / (4.f * Frame::cos_theta(si.wi));

        return (F * result) & active;
    }

    template <typename SurfaceInteraction, typename Vector3,
              typename Value = value_t<Vector3>>
    MTS_INLINE
    Value pdf_impl(const BSDFContext &ctx, const SurfaceInteraction & si,
                   const Vector3 &wo, mask_t<Value> active) const {
        using Frame = mitsuba::Frame<Vector3>;

        Value cos_theta_i = Frame::cos_theta(si.wi),
              cos_theta_o = Frame::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely(!ctx.is_enabled(EGlossyReflection) || none(active)))
            return 0.f;

        /* Calculate the half-direction vector */
        Vector3 H = normalize(wo + si.wi);

        /* Construct a microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution<Value> distr(m_type, m_alpha_u, m_alpha_v,
                                            m_sample_visible);

        Value result;
        if (likely(m_sample_visible))
            result = distr.eval(H) * distr.smith_g1(si.wi, H) /
                     (4.f * cos_theta_i);
        else
            result = distr.pdf(si.wi, H) / (4.f * dot(wo, H));

        return select(active, result, 0.f);
    }


    std::string to_string() const override {
        std::ostringstream oss;
        oss << "RoughConductor[" << std::endl
            << "  distribution = ";
        if (m_type == EBeckmann)
            oss << "beckmann";
        else
            oss << "ggx";
        oss << "," << std::endl
            << "  sample_visible = " << m_sample_visible << "," << std::endl
            << "  alpha_u = " << m_alpha_u << "," << std::endl
            << "  alpha_v = " << m_alpha_v << "," << std::endl
            << "  eta = " << string::indent(m_eta) << "," << std::endl
            << "  k = " << string::indent(m_k) << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_BSDF()
    MTS_DECLARE_CLASS()

private:
    /// Specifies the type of microfacet distribution
    EType m_type;

    /// Anisotropic roughness values
    Float m_alpha_u, m_alpha_v;

    /// Importance sample the distribution of visible normals?
    bool m_sample_visible;

    /// Relative refractive index (real component)
    ref<ContinuousSpectrum> m_eta;

    /// Relative refractive index (imaginary component).
    ref<ContinuousSpectrum> m_k;
};

MTS_IMPLEMENT_CLASS(RoughConductor, BSDF)
MTS_EXPORT_PLUGIN(RoughConductor, "Rough conductor");

NAMESPACE_END(mitsuba)
