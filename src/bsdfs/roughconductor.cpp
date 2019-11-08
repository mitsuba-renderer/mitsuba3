#include <mitsuba/core/string.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class RoughConductor final : public BSDF<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(RoughConductor, BSDF);
    MTS_USING_BASE(BSDF, Base, m_flags, m_components)
    MTS_IMPORT_TYPES(ContinuousSpectrum)
    using SpectrumU = depolarized_t<Spectrum>;

    RoughConductor(const Properties &props) : Base(props) {
        m_eta = props.spectrum<ContinuousSpectrum>("eta", 0.f);
        m_k   = props.spectrum<ContinuousSpectrum>("k", 1.f);

        MicrofacetDistribution<ScalarFloat> distr(props);
        m_type = distr.type();
        m_sample_visible = distr.sample_visible();

        m_flags = BSDFFlags::GlossyReflection | BSDFFlags::FrontSide;

        m_alpha_u = m_alpha_v = distr.alpha_u();
        if (distr.is_anisotropic()) {
            m_alpha_v = distr.alpha_v();
            m_flags = m_flags | BSDFFlags::Anisotropic;
        }

        m_components.push_back(m_flags);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                                             Float /*sample1*/, const Point2f &sample2,
                                             Mask active) const override {
        BSDFSample3f bs;
        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        active &= cos_theta_i > 0.f;

        if (unlikely(!ctx.is_enabled(BSDFFlags::GlossyReflection) || none_or<false>(active)))
            return { bs, 0.f };

        /* Construct a microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution<Float> distr(m_type, m_alpha_u, m_alpha_v,
                                            m_sample_visible);

        // Sample M, the microfacet normal
        Normal3f m;
        std::tie(m, bs.pdf) = distr.sample(si.wi, sample2);

        // Perfect specular reflection based on the microfacet normal
        bs.wo = reflect(si.wi, m);
        bs.eta = 1.f;
        bs.sampled_component = 0;
        bs.sampled_type = +BSDFFlags::GlossyReflection;

        // Ensure that this is a valid sample
        active &= neq(bs.pdf, 0.f) && Frame3f::cos_theta(bs.wo) > 0.f;

        Float weight;
        if (likely(m_sample_visible))
            weight = distr.smith_g1(bs.wo, m);
        else
            weight = distr.G(si.wi, bs.wo, m) * dot(si.wi, m) /
                     (cos_theta_i * Frame3f::cos_theta(m));

        // Evaluate the Fresnel factor
        // TODO: handle polarization rather than discarding it here.
        Complex<SpectrumU> eta_c(depolarize(m_eta->eval(si, active)),
                                 depolarize(m_k->eval(si, active)));

        Spectrum F = fresnel_conductor(SpectrumU(dot(si.wi, m)), eta_c);

        // Jacobian of the half-direction mapping
        bs.pdf /= 4.f * dot(bs.wo, m);

        return { bs, (F * weight) & active };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                       const Vector3f &wo, Mask active) const override {

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely(!ctx.is_enabled(BSDFFlags::GlossyReflection) || none_or<false>(active)))
            return 0.f;

        // Calculate the half-direction vector
        Vector3f H = normalize(wo + si.wi);

        /* Construct a microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution<Float> distr(m_type, m_alpha_u, m_alpha_v,
                                            m_sample_visible);

        // Evaluate the microfacet normal distribution
        Float D = distr.eval(H);

        active &= neq(D, 0.f);

        // Evaluate the Fresnel factor
        // TODO: handle polarization rather than discarding it here.
        Complex<SpectrumU> eta_c(depolarize(m_eta->eval(si, active)),
                                 depolarize(m_k->eval(si, active)));
        Spectrum F = fresnel_conductor(SpectrumU(dot(si.wi, H)), eta_c);

        // Evaluate Smith's shadow-masking function
        Float G = distr.G(si.wi, wo, H);

        // Evaluate the full microfacet model (except Fresnel)
        Float result = D * G / (4.f * Frame3f::cos_theta(si.wi));

        return (F * result) & active;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f & si,
                   const Vector3f &wo, Mask active) const override {

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely(!ctx.is_enabled(BSDFFlags::GlossyReflection) || none_or<false>(active)))
            return 0.f;

        // Calculate the half-direction vector
        Vector3f H = normalize(wo + si.wi);

        /* Construct a microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution<Float> distr(m_type, m_alpha_u, m_alpha_v,
                                            m_sample_visible);

        Float result;
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
            << "  distribution = " << m_type << "," << std::endl
            << "  sample_visible = " << m_sample_visible << "," << std::endl
            << "  alpha_u = " << m_alpha_u << "," << std::endl
            << "  alpha_v = " << m_alpha_v << "," << std::endl
            << "  eta = " << string::indent(m_eta) << "," << std::endl
            << "  k = " << string::indent(m_k) << std::endl
            << "]";
        return oss.str();
    }

private:
    /// Specifies the type of microfacet distribution
    MicrofacetType m_type;
    /// Anisotropic roughness values
    ScalarFloat m_alpha_u, m_alpha_v;
    /// Importance sample the distribution of visible normals?
    bool m_sample_visible;

    /// Relative refractive index (real component)
    ref<ContinuousSpectrum> m_eta;
    /// Relative refractive index (imaginary component).
    ref<ContinuousSpectrum> m_k;
};

MTS_IMPLEMENT_PLUGIN(RoughConductor, BSDF, "Rough conductor");

NAMESPACE_END(mitsuba)
