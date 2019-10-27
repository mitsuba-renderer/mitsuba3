#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/timer.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/spectrum.h>

#define MTS_ROUGH_TRANSMITTANCE_RES 64

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class RoughPlastic final : public BSDF<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN(RoughPlastic, BSDF);
    MTS_USING_BASE(BSDF, Base, m_flags, m_components)
    using ContinuousSpectrum = typename Aliases::ContinuousSpectrum;

    RoughPlastic(const Properties &props) : Base(props) {
        /// Specifies the internal index of refraction at the interface
        sFloat int_ior = lookup_ior(props, "int_ior", "polypropylene");

        /// Specifies the external index of refraction at the interface
        sFloat ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0.f || ext_ior < 0.f || int_ior == ext_ior)
            Throw("The interior and exterior indices of "
                  "refraction must be positive and differ!");

        m_eta = int_ior / ext_ior;
        m_inv_eta_2 = 1.f / (m_eta * m_eta);

        m_specular_reflectance = props.spectrum<Float, Spectrum>("specular_reflectance", 1.f);
        m_diffuse_reflectance  = props.spectrum<Float, Spectrum>("diffuse_reflectance",  .5f);

        /* Compute weights that further steer samples towards
           the specular or diffuse components */
        sFloat d_mean = m_diffuse_reflectance->mean(),
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

        /* Precompute rough reflectance (vectorized) */ {
            using FloatP    = Packet<sFloat>;
            using Vector3fX = Vector<DynamicArray<FloatP>, 3>;

            MicrofacetDistribution<FloatP> distr_p(m_type, m_alpha);
            Vector3fX wi = zero<Vector3fX>(MTS_ROUGH_TRANSMITTANCE_RES);
            for (size_t i = 0; i < slices(wi); ++i) {
                sFloat mu    = std::max((sFloat) 1e-6f, sFloat(i) / sFloat(slices(wi) - 1));
                slice(wi, i) = Vector3f(std::sqrt(1 - mu * mu), 0.f, mu);
            }
            m_external_transmittance = 1.f - distr_p.eval_reflectance(wi, m_eta);
            m_internal_reflectance =
                mean(distr_p.eval_reflectance(wi, 1.f / m_eta) * wi.z()) * 2.f;
        }

        m_components.push_back(BSDFFlags::GlossyReflection | BSDFFlags::FrontSide);
        m_components.push_back(BSDFFlags::DiffuseReflection | BSDFFlags::FrontSide);
        m_flags =  m_components[0] | m_components[1];
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

        Float t_i = lerp_gather(m_external_transmittance.data(), cos_theta_i,
                                MTS_ROUGH_TRANSMITTANCE_RES, active);

        // Determine which component should be sampled
        Float prob_specular = (1.f - t_i) * m_specular_sampling_weight,
              prob_diffuse  = t_i * (1.f - m_specular_sampling_weight);

        if (unlikely(has_specular != has_diffuse))
            prob_specular = has_specular ? 1.f : 0.f;
        else
            prob_specular = prob_specular / (prob_specular + prob_diffuse);
        prob_diffuse = 1.f - prob_specular;

        Mask sample_specular = active && (sample1 < prob_specular),
             sample_diffuse = active && !sample_specular;

        bs.eta = 1.f;

        if (any_or<true>(sample_specular)) {
            MicrofacetDistribution<Float> distr(m_type, m_alpha, m_sample_visible);
            Normal3f m = std::get<0>(distr.sample(si.wi, sample2));

            masked(bs.wo, sample_specular) = reflect(si.wi, m);
            masked(bs.sampled_component, sample_specular) = 0;
            masked(bs.sampled_type, sample_specular) = +BSDFFlags::GlossyReflection;
        }

        if (any_or<true>(sample_diffuse)) {
            masked(bs.wo, sample_diffuse) = warp::square_to_cosine_hemisphere(sample2);
            masked(bs.sampled_component, sample_diffuse) = 1;
            masked(bs.sampled_type, sample_diffuse) = +BSDFFlags::DiffuseReflection;
        }

        bs.pdf = pdf(ctx, si, bs.wo, active);
        active &= bs.pdf > 0.f;
        result = eval(ctx, si, bs.wo, active);

        return { bs, select(active, result / bs.pdf, 0.f) };
    }

    MTS_INLINE
    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo,
                  Mask active) const override {
        bool has_specular = ctx.is_enabled(BSDFFlags::GlossyReflection, 0),
             has_diffuse  = ctx.is_enabled(BSDFFlags::DiffuseReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        MicrofacetDistribution<Float> distr(m_type, m_alpha, m_sample_visible);

        Spectrum result(0.f);
        if (unlikely((!has_specular && !has_diffuse) || none_or<false>(active)))
            return result;

        if (has_specular) {
            // Calculate the reflection half-vector
            Vector3f H = normalize(wo + si.wi);

            // Evaluate the microfacet normal distribution
            Float D = distr.eval(H);

            // Fresnel term
            Float F = std::get<0>(fresnel(dot(si.wi, H), Float(m_eta)));

            // Smith's shadow-masking function
            Float G = distr.G(si.wi, wo, H);

            // Calculate the specular reflection component
            Float value = F * D * G / (4.f * cos_theta_i);

            result += m_specular_reflectance->eval(si, active) * value;
        }

        if (has_diffuse) {
            using SpectrumU = depolarized_t<Spectrum>;

            Float t_i = lerp_gather(m_external_transmittance.data(), cos_theta_i,
                                    MTS_ROUGH_TRANSMITTANCE_RES, active),
                  t_o = lerp_gather(m_external_transmittance.data(), cos_theta_o,
                                    MTS_ROUGH_TRANSMITTANCE_RES, active);

            SpectrumU diff = depolarize(m_diffuse_reflectance->eval(si, active));
            diff /= 1.f - (m_nonlinear ? (diff * m_internal_reflectance)
                                       : SpectrumU(m_internal_reflectance));

            result += diff * (math::InvPi<Float> * m_inv_eta_2 * cos_theta_o * t_i * t_o);
        }

        return select(active, result, 0.f);
    }

    Float lerp_gather(const scalar_t<Float> *data, Float x, size_t size,
                      Mask active = true) const {
        using UInt = uint_array_t<Float>;
        x *= Float(size - 1);

        UInt index = min(UInt(x), scalar_t<UInt>(size - 2));

        Float w1 = x - Float(index),
              w0 = 1.f - w1,
              v0 = gather<Float>(data, index, active),
              v1 = gather<Float>(data + 1, index, active);

        return w0 * v0 + w1 * v1;
    }

    MTS_INLINE
    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo,
              Mask active) const override {
        bool has_specular = ctx.is_enabled(BSDFFlags::DeltaReflection, 0),
             has_diffuse = ctx.is_enabled(BSDFFlags::DiffuseReflection, 1);

        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        active &= cos_theta_i > 0.f && cos_theta_o > 0.f;

        if (unlikely((!has_specular && !has_diffuse) || none_or<false>(active)))
            return 0.f;

        Float t_i = lerp_gather(m_external_transmittance.data(), cos_theta_i,
                                MTS_ROUGH_TRANSMITTANCE_RES, active);

        // Determine which component should be sampled
        Float prob_specular = (1.f - t_i) * m_specular_sampling_weight,
              prob_diffuse  = t_i * (1.f - m_specular_sampling_weight);

        if (unlikely(has_specular != has_diffuse))
            prob_specular = has_specular ? 1.f : 0.f;
        else
            prob_specular = prob_specular / (prob_specular + prob_diffuse);
        prob_diffuse = 1.f - prob_specular;

        Vector3f H = normalize(wo + si.wi);

        MicrofacetDistribution<Float> distr(m_type, m_alpha, m_sample_visible);
        Float result = 0.f;
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
            << "  distribution = " << m_type << "," << std::endl
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

private:
    ref<ContinuousSpectrum> m_diffuse_reflectance;
    ref<ContinuousSpectrum> m_specular_reflectance;
    MicrofacetType m_type;
    sFloat m_eta, m_inv_eta_2;
    sFloat m_alpha;
    sFloat m_specular_sampling_weight;
    bool m_nonlinear;
    bool m_sample_visible;
    DynamicBuffer<Float> m_external_transmittance;
    sFloat m_internal_reflectance;
};

MTS_IMPLEMENT_PLUGIN(RoughPlastic, BSDF, "Rough plastic");

NAMESPACE_END(mitsuba)
