#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

class SmoothPlastic : public BSDF {
public:
    SmoothPlastic(const Properties &props) {
        /* Specifies the internal index of refraction at the interface */
        Float int_ior = lookup_ior(props, "int_ior", "polypropylene");

        /* Specifies the external index of refraction at the interface */
        Float ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0.f || ext_ior < 0.f)
            Throw("The interior and exterior indices of "
                  "refraction must be positive!");

        m_eta = int_ior / ext_ior;
        m_inv_eta_2 = 1.f / (m_eta * m_eta);

        // Numerically approximate the diffuse Fresnel reflectance
        m_fdr_int = fresnel_diffuse_reflectance(1.f / m_eta);
        m_fdr_ext = fresnel_diffuse_reflectance(m_eta);

        m_specular_reflectance = props.spectrum("specular_reflectance", 1.f);
        m_diffuse_reflectance  = props.spectrum("diffuse_reflectance",  .5f);

        /* Compute weights that further steer samples towards
           the specular or diffuse components */
        Float d_mean = m_diffuse_reflectance->mean(),
              s_mean = m_specular_reflectance->mean();

        m_specular_sampling_weight = s_mean / (d_mean + s_mean);

        m_nonlinear = props.bool_("nonlinear", false);

        m_flags = EDeltaReflection | EDiffuseReflection | EFrontSide;
        m_components.push_back(EDeltaReflection | EFrontSide);
        m_components.push_back(EDiffuseReflection | EFrontSide);
    }

    template <
        typename SurfaceInteraction,
        typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
        typename Value      = typename SurfaceInteraction::Value,
        typename Point2     = typename SurfaceInteraction::Point2,
        typename Spectrum   = Spectrum<Value>>
    MTS_INLINE
    std::pair<BSDFSample, Spectrum>
    sample_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                Value sample1, const Point2 &sample2,
                mask_t<Value> active) const {
        using Index   = typename BSDFSample::Index;
        using Vector3 = typename BSDFSample::Vector3;
        using Index   = uint32_array_t<Value>;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        bool has_specular = ctx.is_enabled(EDeltaReflection, 0),
             has_diffuse = ctx.is_enabled(EDiffuseReflection, 1);

        Value cos_theta_i = Frame::cos_theta(si.wi);
        active &= cos_theta_i > 0.f;

        BSDFSample bs;
        Spectrum result(0.f);
        if (unlikely((!has_specular && !has_diffuse) || none(active)))
            return { bs, result };

        /* Determine which component should be sampled */
        Value f_i           = std::get<0>(fresnel(cos_theta_i, Value(m_eta))),
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

        if (any(sample_specular)) {
            masked(bs.wo, sample_specular) = reflect(si.wi);
            masked(bs.pdf, sample_specular) = prob_specular;
            masked(bs.sampled_component, sample_specular) = Index(0);
            masked(bs.sampled_type, sample_specular) = Index(EDeltaReflection);

            Spectrum spec = m_specular_reflectance->eval(si, sample_specular) * f_i / bs.pdf;
            masked(result, sample_specular) = spec;
        }

        if (any(sample_diffuse)) {
            masked(bs.wo, sample_diffuse) = warp::square_to_cosine_hemisphere(sample2);
            masked(bs.pdf, sample_diffuse) = prob_diffuse * warp::square_to_cosine_hemisphere_pdf(bs.wo);
            masked(bs.sampled_component, sample_diffuse) = Index(1);
            masked(bs.sampled_type, sample_diffuse) = Index(EDiffuseReflection);

            Value f_o = std::get<0>(fresnel(Frame::cos_theta(bs.wo), Value(m_eta)));
            Spectrum diff = m_diffuse_reflectance->eval(si, sample_diffuse);
            diff /= 1.f - (m_nonlinear ? (diff * m_fdr_int) : m_fdr_int);
            diff *= m_inv_eta_2 * (1.f - f_i) * (1.f - f_o) / prob_diffuse;
            masked(result, sample_diffuse) = diff;
        }

        return { bs, result };
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3,
              typename Spectrum = Spectrum<Value>>
    MTS_INLINE
    Spectrum eval_impl(const BSDFContext &ctx,
                       const SurfaceInteraction &si,
                       const Vector3 &wo,
                       mask_t<Value> active) const {
        using Frame = Frame<Vector3>;

        bool has_diffuse = ctx.is_enabled(EDiffuseReflection, 1);

        Value cos_theta_i = Frame::cos_theta(si.wi),
              cos_theta_o = Frame::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely(!has_diffuse || none(active)))
            return 0.f;

        Value f_i = std::get<0>(fresnel(cos_theta_i, Value(m_eta))),
              f_o = std::get<0>(fresnel(cos_theta_o, Value(m_eta)));

        Spectrum diff = m_diffuse_reflectance->eval(si, active);
        diff /= 1.f - (m_nonlinear ? (diff * m_fdr_int) : m_fdr_int);

        diff *= warp::square_to_cosine_hemisphere_pdf(wo) *
                m_inv_eta_2 * (1.f - f_i) * (1.f - f_o);

        return select(active, diff, zero<Spectrum>());
    }

    template <typename SurfaceInteraction,
              typename Value   = typename SurfaceInteraction::Value,
              typename Vector3 = typename SurfaceInteraction::Vector3>
    MTS_INLINE
    Value pdf_impl(const BSDFContext &ctx,
                              const SurfaceInteraction &si, const Vector3 &wo,
                              mask_t<Value> active) const {
        using Frame = Frame<Vector3>;

        Value cos_theta_i = Frame::cos_theta(si.wi),
              cos_theta_o = Frame::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely(!ctx.is_enabled(EDiffuseReflection, 1) || none(active)))
            return 0.f;

        Value prob_diffuse = 1.f;

        if (ctx.is_enabled(EDeltaReflection, 0)) {
            Value f_i           = std::get<0>(fresnel(cos_theta_i, Value(m_eta))),
                  prob_specular = f_i * m_specular_sampling_weight;
            prob_diffuse  = (1.f - f_i) * (1.f - m_specular_sampling_weight);
            prob_diffuse = prob_diffuse / (prob_specular + prob_diffuse);
        }

        Value pdf = warp::square_to_cosine_hemisphere_pdf(wo) * prob_diffuse;

        return select(active, pdf, 0.f);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothPlastic[" << std::endl
            << "  specular_reflectance = "     << m_specular_reflectance              << "," << std::endl
            << "  diffuse_reflectance = "      << m_diffuse_reflectance               << "," << std::endl
            << "  specular_sampling_weight = " << m_specular_sampling_weight          << "," << std::endl
            << "  specular_sampling_weight = "  << (1.f - m_specular_sampling_weight)  << "," << std::endl
            << "  nonlinear = "                << (int) m_nonlinear                   << "," << std::endl
            << "  eta = "                      << m_eta                               << "," << std::endl
            << "  fdr_int = "                  << m_fdr_int                           << "," << std::endl
            << "  fdr_ext = "                  << m_fdr_ext                           << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_BSDF()
    MTS_DECLARE_CLASS()
private:
    ref<ContinuousSpectrum> m_diffuse_reflectance;
    ref<ContinuousSpectrum> m_specular_reflectance;
    Float m_eta, m_inv_eta_2;
    Float m_fdr_int, m_fdr_ext;
    Float m_specular_sampling_weight;
    bool m_nonlinear;
};


MTS_IMPLEMENT_CLASS(SmoothPlastic, BSDF)
MTS_EXPORT_PLUGIN(SmoothPlastic, "Smooth plastic");

NAMESPACE_END(mitsuba)
