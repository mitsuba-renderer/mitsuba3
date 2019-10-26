#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class SmoothPlastic final : public BSDF<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN();
    MTS_USING_BASE(BSDF, Base, m_flags, m_components)
    using ContinuousSpectrum = typename Aliases::ContinuousSpectrum;
    using SpectrumU          = depolarized_t<Spectrum>;

    SmoothPlastic(const Properties &props) : Base(props) {
        // Specifies the internal index of refraction at the interface
        sFloat int_ior = lookup_ior(props, "int_ior", "polypropylene");

        // Specifies the external index of refraction at the interface
        sFloat ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0.f || ext_ior < 0.f)
            Throw("The interior and exterior indices of "
                  "refraction must be positive!");

        m_eta = int_ior / ext_ior;
        m_inv_eta_2 = 1.f / (m_eta * m_eta);

        // Numerically approximate the diffuse Fresnel reflectance
        m_fdr_int = fresnel_diffuse_reflectance(1.f / m_eta);
        m_fdr_ext = fresnel_diffuse_reflectance(m_eta);

        m_specular_reflectance = props.spectrum<Float, Spectrum>("specular_reflectance", 1.f);
        m_diffuse_reflectance  = props.spectrum<Float, Spectrum>("diffuse_reflectance", .5f);

        // Compute weights that further steer samples towards the specular or diffuse components
        sFloat d_mean = m_diffuse_reflectance->mean(),
               s_mean = m_specular_reflectance->mean();

        m_specular_sampling_weight = s_mean / (d_mean + s_mean);

        m_nonlinear = props.bool_("nonlinear", false);

        m_components.push_back(BSDFFlags::DeltaReflection | BSDFFlags::FrontSide);
        m_components.push_back(BSDFFlags::DiffuseReflection | BSDFFlags::FrontSide);
        m_flags = m_components[0] | m_components[1];
    }

    MTS_INLINE
    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                                             Float sample1, const Point2f &sample2,
                                             Mask active) const override {
        bool has_specular = ctx.is_enabled(BSDFFlags::DeltaReflection, 0),
             has_diffuse  = ctx.is_enabled(BSDFFlags::DiffuseReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        active &= cos_theta_i > 0.f;

        BSDFSample3f bs;
        Spectrum result(0.f);
        if (unlikely((!has_specular && !has_diffuse) || none_or<false>(active)))
            return { bs, result };

        // Determine which component should be sampled
        Float f_i           = std::get<0>(fresnel(cos_theta_i, Float(m_eta))),
              prob_specular = f_i * m_specular_sampling_weight,
              prob_diffuse  = (1.f - f_i) * (1.f - m_specular_sampling_weight);

        if (unlikely(has_specular != has_diffuse))
            prob_specular = has_specular ? 1.f : 0.f;
        else
            prob_specular = prob_specular / (prob_specular + prob_diffuse);

        prob_diffuse = 1.f - prob_specular;

        Mask sample_specular = active && (sample1 < prob_specular),
             sample_diffuse = active && !sample_specular;

        bs.eta = 1.f;
        bs.pdf = 0.f;

        if (any_or<true>(sample_specular)) {
            masked(bs.wo, sample_specular) = reflect(si.wi);
            masked(bs.pdf, sample_specular) = prob_specular;
            masked(bs.sampled_component, sample_specular) = 0;
            masked(bs.sampled_type, sample_specular)      = +BSDFFlags::DeltaReflection;

            Spectrum spec = m_specular_reflectance->eval(si, sample_specular) * f_i / bs.pdf;
            masked(result, sample_specular) = spec;
        }

        if (any_or<true>(sample_diffuse)) {
            masked(bs.wo, sample_diffuse) = warp::square_to_cosine_hemisphere(sample2);
            masked(bs.pdf, sample_diffuse) = prob_diffuse * warp::square_to_cosine_hemisphere_pdf(bs.wo);
            masked(bs.sampled_component, sample_diffuse) = 1;
            masked(bs.sampled_type, sample_diffuse) = +BSDFFlags::DiffuseReflection;

            Float f_o = std::get<0>(fresnel(Frame3f::cos_theta(bs.wo), Float(m_eta)));
            // TODO: handle polarization instead of discarding it here
            SpectrumU diff = depolarize(m_diffuse_reflectance->eval(si, sample_diffuse));
            diff /= 1.f - (m_nonlinear ? (diff * m_fdr_int) : m_fdr_int);
            diff *= m_inv_eta_2 * (1.f - f_i) * (1.f - f_o) / prob_diffuse;
            masked(result, sample_diffuse) = diff;
        }

        return { bs, result };
    }

    MTS_INLINE
    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo,
                  Mask active) const override {
        bool has_diffuse = ctx.is_enabled(BSDFFlags::DiffuseReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely(!has_diffuse || none_or<false>(active)))
            return 0.f;

        Float f_i = std::get<0>(fresnel(cos_theta_i, Float(m_eta))),
              f_o = std::get<0>(fresnel(cos_theta_o, Float(m_eta)));

        // TODO: handle polarization instead of discarding it here
        SpectrumU diff = depolarize(m_diffuse_reflectance->eval(si, active));
        diff /= 1.f - (m_nonlinear ? (diff * m_fdr_int) : m_fdr_int);

        diff *= warp::square_to_cosine_hemisphere_pdf(wo) *
                m_inv_eta_2 * (1.f - f_i) * (1.f - f_o);

        return select(active, diff, zero<Spectrum>());
    }

    MTS_INLINE
    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo,
              Mask active) const override {
        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely(!ctx.is_enabled(BSDFFlags::DiffuseReflection, 1) || none_or<false>(active)))
            return 0.f;

        Float prob_diffuse = 1.f;

        if (ctx.is_enabled(BSDFFlags::DeltaReflection, 0)) {
            Float f_i           = std::get<0>(fresnel(cos_theta_i, Float(m_eta))),
                  prob_specular = f_i * m_specular_sampling_weight;
            prob_diffuse  = (1.f - f_i) * (1.f - m_specular_sampling_weight);
            prob_diffuse = prob_diffuse / (prob_specular + prob_diffuse);
        }

        Float pdf = warp::square_to_cosine_hemisphere_pdf(wo) * prob_diffuse;

        return select(active, pdf, 0.f);
    }

    std::vector<ref<Object>> children() override {
        return { m_diffuse_reflectance.get(), m_specular_reflectance.get() };
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothPlastic[" << std::endl
            << "  specular_reflectance = "     << m_specular_reflectance              << "," << std::endl
            << "  diffuse_reflectance = "      << m_diffuse_reflectance               << "," << std::endl
            << "  specular_sampling_weight = " << m_specular_sampling_weight          << "," << std::endl
            << "  specular_sampling_weight = " << (1.f - m_specular_sampling_weight)  << "," << std::endl
            << "  nonlinear = "                << (int) m_nonlinear                   << "," << std::endl
            << "  eta = "                      << m_eta                               << "," << std::endl
            << "  fdr_int = "                  << m_fdr_int                           << "," << std::endl
            << "  fdr_ext = "                  << m_fdr_ext                           << std::endl
            << "]";
        return oss.str();
    }

private:
    ref<ContinuousSpectrum> m_diffuse_reflectance;
    ref<ContinuousSpectrum> m_specular_reflectance;
    sFloat m_eta, m_inv_eta_2;
    sFloat m_fdr_int, m_fdr_ext;
    sFloat m_specular_sampling_weight;
    bool m_nonlinear;
};

MTS_IMPLEMENT_PLUGIN(SmoothPlastic, BSDF, "Smooth plastic");
NAMESPACE_END(mitsuba)
