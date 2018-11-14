#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/spectrum.h>

#define MTS_ROUGH_TRANSMITTANCE_RES 64

NAMESPACE_BEGIN(mitsuba)

class RoughPlastic final : public BSDF {
public:
    RoughPlastic(const Properties &props) : BSDF(props) {
        /// Specifies the internal index of refraction at the interface
        Float int_ior = lookup_ior(props, "int_ior", "polypropylene");

        /// Specifies the external index of refraction at the interface
        Float ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0.f || ext_ior < 0.f || int_ior == ext_ior)
            Throw("The interior and exterior indices of "
                  "refraction must be positive and differ!");

        m_eta = int_ior / ext_ior;
        m_inv_eta_2 = 1.f / (m_eta * m_eta);

        m_specular_reflectance = props.spectrum("specular_reflectance", 1.f);
        m_diffuse_reflectance  = props.spectrum("diffuse_reflectance",  .5f);

        /* Compute weights that further steer samples towards
           the specular or diffuse components */
        Float d_mean = m_diffuse_reflectance->mean(),
              s_mean = m_specular_reflectance->mean();

        m_specular_sampling_weight = s_mean / (d_mean + s_mean);

        m_nonlinear = props.bool_("nonlinear", false);

        MicrofacetDistribution<Float> distr(props);
        m_type = distr.type();
        m_sample_visible = distr.sample_visible();

        if (distr.is_anisotropic())
            Throw("The 'roughplastic' plugin currently does not support "
                  "anisotropic microfacet distributions!");

        m_alpha = distr.alpha();

        /* Precompute rough reflectance */
        MicrofacetDistribution<FloatP> distr_p(m_type, m_alpha);
        Vector3fX wi = zero<Vector3fX>(MTS_ROUGH_TRANSMITTANCE_RES);
        for (size_t i = 0; i < slices(wi); ++i) {
            Float mu     = std::max((Float) 1e-6f, i * Float(1.f) / (slices(wi) - 1));
            slice(wi, i) = Vector3f(std::sqrt(1 - mu * mu), 0.f, mu);
        }
        m_external_transmittance = 1.f - distr_p.eval_reflectance(wi, m_eta);
        m_internal_reflectance =
            mean(distr_p.eval_reflectance(wi, 1.f / m_eta) * wi.z()) * 2.f;

        m_flags =  EGlossyReflection | EDiffuseReflection | EFrontSide;
        m_components.push_back(EGlossyReflection | EFrontSide);
        m_components.push_back(EDiffuseReflection | EFrontSide);
    }

    template <typename SurfaceInteraction,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Value      = typename SurfaceInteraction::Value,
              typename Point2     = typename SurfaceInteraction::Point2,
              typename Spectrum   = Spectrum<Value>>
    MTS_INLINE
    std::pair<BSDFSample, Spectrum> sample_impl(
            const BSDFContext &ctx, const SurfaceInteraction &si,
            Value sample1, const Point2 &sample2, mask_t<Value> active) const {
        using Index   = typename BSDFSample::Index;
        using Vector3 = typename BSDFSample::Vector3;
        using Normal3 = typename BSDFSample::Normal3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        bool has_specular = ctx.is_enabled(EDeltaReflection, 0),
             has_diffuse  = ctx.is_enabled(EDiffuseReflection, 1);

        Value cos_theta_i = Frame::cos_theta(si.wi);
        active &= cos_theta_i > 0.f;

        BSDFSample bs;
        Spectrum result(0.f);
        if (unlikely((!has_specular && !has_diffuse) || none(active)))
            return { bs, result };

        Value t_i = lerp_gather<Value>(m_external_transmittance.data(),
                                       cos_theta_i,
                                       MTS_ROUGH_TRANSMITTANCE_RES, active);

        /* Determine which component should be sampled */
        Value prob_specular = (1.f - t_i) * m_specular_sampling_weight,
              prob_diffuse  = t_i * (1.f - m_specular_sampling_weight);

        if (unlikely(has_specular != has_diffuse))
            prob_specular = has_specular ? 1.f : 0.f;
        else
            prob_specular = prob_specular / (prob_specular + prob_diffuse);
        prob_diffuse = 1.f - prob_specular;

        Mask sample_specular = active && (sample1 < prob_specular),
             sample_diffuse = active && !sample_specular;

        bs.eta = 1.f;

        if (any(sample_specular)) {
            MicrofacetDistribution<Value> distr(m_type, m_alpha, m_sample_visible);
            Normal3 m = std::get<0>(distr.sample(si.wi, sample2));

            masked(bs.wo, sample_specular) = reflect(si.wi, m);
            masked(bs.sampled_component, sample_specular) = Index(0);
            masked(bs.sampled_type, sample_specular) = Index(EGlossyReflection);
        }

        if (any(sample_diffuse)) {
            masked(bs.wo, sample_diffuse) = warp::square_to_cosine_hemisphere(sample2);
            masked(bs.sampled_component, sample_diffuse) = Index(1);
            masked(bs.sampled_type, sample_diffuse) = Index(EDiffuseReflection);
        }

        bs.pdf = pdf_impl(ctx, si, bs.wo, active);
        active &= bs.pdf > 0.f;
        result = eval_impl(ctx, si, bs.wo, active);

        return { bs, select(active, result / bs.pdf, 0.f) };
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3,
              typename Spectrum = Spectrum<Value>>
    MTS_INLINE
    Spectrum eval_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                       const Vector3 &wo, mask_t<Value> active) const {
        using Frame = Frame<Vector3>;

        bool has_specular = ctx.is_enabled(EGlossyReflection, 0),
             has_diffuse  = ctx.is_enabled(EDiffuseReflection, 1);

        Value cos_theta_i = Frame::cos_theta(si.wi),
              cos_theta_o = Frame::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        MicrofacetDistribution<Value> distr(m_type, m_alpha, m_sample_visible);

        Spectrum result(0.f);
        if (unlikely((!has_specular && !has_diffuse) || none(active)))
            return result;

        if (has_specular) {
            // Calculate the reflection half-vector
            Vector3 H = normalize(wo + si.wi);

            // Evaluate the microfacet normal distribution
            Value D = distr.eval(H);

            // Fresnel term
            Value F = std::get<0>(fresnel(dot(si.wi, H), Value(m_eta)));

            // Smith's shadow-masking function
            Value G = distr.G(si.wi, wo, H);

            // Calculate the specular reflection component
            Value value = F * D * G / (4.f * cos_theta_i);

            result += m_specular_reflectance->eval(si, active) * value;
        }

        if (has_diffuse) {
            Value t_i = lerp_gather<Value>(m_external_transmittance.data(),
                                           cos_theta_i,
                                           MTS_ROUGH_TRANSMITTANCE_RES, active),
                  t_o = lerp_gather<Value>(m_external_transmittance.data(),
                                           cos_theta_o,
                                           MTS_ROUGH_TRANSMITTANCE_RES, active);

            Spectrum diff = m_diffuse_reflectance->eval(si, active);
            diff /= 1.f - (m_nonlinear ? (diff * m_internal_reflectance)
                                       : Spectrum(m_internal_reflectance));

            result += diff * (InvPi * m_inv_eta_2 * cos_theta_o * t_i * t_o);
        }

        return select(active, result, 0.f);
    }

    template <typename Value>
    Value lerp_gather(const Float *data, Value x, size_t size,
                      mask_t<Value> active = true) const {
        using UInt = uint_array_t<Value>;
        x *= Float(size - 1);

        UInt index = min(UInt(x), scalar_t<UInt>(size - 2));

        Value w1 = x - Value(index),
              w0 = 1.f - w1,
              v0 = gather<Value>(data, index, active),
              v1 = gather<Value>(data + 1, index, active);

        return w0 * v0 + w1 * v1;
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3>
    MTS_INLINE
    Value pdf_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                   const Vector3 &wo, mask_t<Value> active) const {
        using Frame = Frame<Vector3>;

        bool has_specular = ctx.is_enabled(EDeltaReflection, 0),
             has_diffuse = ctx.is_enabled(EDiffuseReflection, 1);

        Value cos_theta_i = Frame::cos_theta(si.wi),
              cos_theta_o = Frame::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely((!has_specular && !has_diffuse) || none(active)))
            return 0.f;

        Value t_i = lerp_gather<Value>(m_external_transmittance.data(),
                                       cos_theta_i,
                                       MTS_ROUGH_TRANSMITTANCE_RES, active);

        /* Determine which component should be sampled */
        Value prob_specular = (1.f - t_i) * m_specular_sampling_weight,
              prob_diffuse  = t_i * (1.f - m_specular_sampling_weight);

        if (unlikely(has_specular != has_diffuse))
            prob_specular = has_specular ? 1.f : 0.f;
        else
            prob_specular = prob_specular / (prob_specular + prob_diffuse);
        prob_diffuse = 1.f - prob_specular;

        Vector3 H = normalize(wo + si.wi);

        MicrofacetDistribution<Value> distr(m_type, m_alpha, m_sample_visible);
        Value result = 0.f;
        if (m_sample_visible)
            result = distr.eval(H) * distr.smith_g1(si.wi, H) /
                     (4.f * cos_theta_i);
        else
            result = distr.pdf(si.wi, H) / (4.f * dot(wo, H));
        result *= prob_specular;

        result += prob_diffuse * warp::square_to_cosine_hemisphere_pdf(wo);

        return result;
    }

    std::vector<ref<Object>> children() override {
        return { m_diffuse_reflectance.get(), m_specular_reflectance.get() };
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "RoughPlastic[" << std::endl
            << "  distribution = ";
        if (m_type == EBeckmann)
            oss << "beckmann";
        else
            oss << "ggx";
        oss << "," << std::endl
            << "  sample_visible = "           << m_sample_visible                    << "," << std::endl
            << "  alpha = "                    << m_alpha                             << "," << std::endl
            << "  specular_reflectance = "     << m_specular_reflectance              << "," << std::endl
            << "  diffuse_reflectance = "      << m_diffuse_reflectance               << "," << std::endl
            << "  specular_sampling_weight = " << m_specular_sampling_weight          << "," << std::endl
            << "  eta = "                      << m_eta                               << "," << std::endl
            << "  nonlinear = "                << m_nonlinear                         << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_BSDF_ALL()
    MTS_DECLARE_CLASS()
private:
    ref<ContinuousSpectrum> m_diffuse_reflectance;
    ref<ContinuousSpectrum> m_specular_reflectance;
    EType m_type;
    Float m_eta, m_inv_eta_2;
    Float m_alpha;
    Float m_specular_sampling_weight;
    bool m_nonlinear;
    bool m_sample_visible;
    FloatX m_external_transmittance;
    Float m_internal_reflectance;
};

MTS_IMPLEMENT_CLASS(RoughPlastic, BSDF)
MTS_EXPORT_PLUGIN(RoughPlastic, "Rough plastic");

NAMESPACE_END(mitsuba)
