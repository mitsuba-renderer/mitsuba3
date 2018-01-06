
#include <mitsuba/core/string.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/bsdfutils.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>

NAMESPACE_BEGIN(mitsuba)

class RoughPlastic : public BSDF {
public:
    RoughPlastic(const Properties &props) {
        m_flags =  EGlossyReflection | EDiffuseReflection | EFrontSide;

        m_specular_reflectance = props.spectrum("specular_reflectance", 1.0f);
        m_diffuse_reflectance  = props.spectrum("diffuse_reflectance",  0.5f);

        // TODO: Verify the input parameters and fix them if necessary

        /* Compute weights that further steer samples towards
           the specular or diffuse components */
        Float d_avg = mean(m_diffuse_reflectance),
              s_avg = mean(m_specular_reflectance);
        m_specular_sampling_weight = s_avg / (d_avg + s_avg);

        /* Specifies the internal index of refraction at the interface */
        Float int_ior = lookup_ior(props, "int_ior", "polypropylene");

        /* Specifies the external index of refraction at the interface */
        Float ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0.0f || ext_ior < 0.0f || int_ior == ext_ior)
            Throw("The interior and exterior indices of "
                  "refraction must be positive and differ!");

        // TODO
        //m_eta = int_ior / ext_ior;
        //m_inv_eta_2 = 1.0f / (m_eta * m_eta);

        m_nonlinear = props.bool_("nonlinear", false);

        MicrofacetDistribution<Float> distr(props);
        m_type = distr.type();
        m_sample_visible = distr.sample_visible();

        if (distr.is_anisotropic())
            Throw("The 'roughplastic' plugin currently does not support "
                  "anisotropic microfacet distributions!");

        m_alpha = distr.alpha();

        m_specular_factor = props.float_("specular_factor", 0.5f);

        /* TODO use proper rough transmittance computations
         * Here we added an interpolation parameter “specular_factor”
         * and we blend between a diffuse and a microfacet model.
         */
    }

    template <typename BSDFContext,
              typename BSDFSample = BSDFSample<typename BSDFContext::Point3>,
              typename Value      = typename BSDFContext::Value,
              typename Point2     = typename BSDFContext::Point2,
              typename Spectrum   = Spectrum<Value>>
    std::pair<BSDFSample, Spectrum> sample_impl(const BSDFContext &ctx,
                                                Value /*sample1*/,
                                                const Point2 &sample2,
                                                const mask_t<Value> &active_) const {
        using Index   = typename BSDFSample::Index;
        using Vector3 = typename BSDFSample::Vector3;
        using Normal3 = typename BSDFSample::Normal3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        bool has_specular = (ctx.component == (size_t) -1 || ctx.component == (size_t) 0)
                            && (ctx.type_mask & EGlossyReflection),
             has_diffuse  = (ctx.component == (size_t) -1 || ctx.component == (size_t) 1)
                            && (ctx.type_mask & EDiffuseReflection);

        Mask active(active_);

        BSDFSample bs;
        bs.pdf = 0.0f;

        if (!has_specular && !has_diffuse)
            return { bs, 0.0f };

        Value n_dot_wi = Frame::cos_theta(ctx.si.wi);

        active = active && (n_dot_wi > 0.0f);

        Mask chose_specular = has_specular;
        Point2 sample(sample2);

        /* Construct the microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution<Value> distr(
            m_type,
            m_alpha,
            m_sample_visible
        );

        Value prob_specular;
        if (has_specular && has_diffuse) {
            /* Find the probability of sampling the specular component */
            //prob_specular = 1.0f - m_external_rough_transmittance->eval(n_dot_wi, distr.alpha(), active);
            prob_specular = m_specular_factor;

            /* Reallocate samples */
            prob_specular = (prob_specular * m_specular_sampling_weight) /
                (prob_specular * m_specular_sampling_weight +
                (1.0f - prob_specular) * (1.0f - m_specular_sampling_weight));


            chose_specular = (sample.y() < prob_specular);

            sample.y() = select(chose_specular,
                                sample.y() / prob_specular,
                                (sample.y() - prob_specular) / (1.0f - prob_specular));
        }

        /* Perfect specular reflection based on the microfacet normal */
        Normal3 m = distr.sample(ctx.si.wi, sample, active).first;
        bs.wo = select(chose_specular,
                       reflect(ctx.si.wi, m),
                       warp::square_to_cosine_hemisphere(sample));
        bs.sampled_component = select(chose_specular, Index(0), Index(1));

        bs.sampled_type = select(chose_specular,
                                 Index(EGlossyReflection),
                                 Index(EDiffuseReflection));
        bs.eta = 1.0f;

        /* Side check */
        active = active && !(chose_specular && (Frame::cos_theta(bs.wo) <= 0.0f));

        if (none(active))
            return { bs, 0.0f };

        /* Guard against numerical imprecisions */
        Value pdf = pdf_impl(ctx, bs.wo, active);

        active = active && neq(pdf, 0.0f);

        if (none(active))
            return { bs, 0.0f };

        Spectrum result = select(active,
                                 eval_impl(ctx, bs.wo, active) / pdf,
                                 Spectrum(0.0f));

        masked(bs.pdf, active) = pdf;

        return { bs, result };
    }

    template <typename BSDFContext,
              typename Value    = typename BSDFContext::Value,
              typename Vector3  = typename BSDFContext::Vector3,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const BSDFContext &ctx, const Vector3 &wo,
                       const mask_t<Value> &active_) const {
        using Frame = Frame<Vector3>;
        using Mask  = mask_t<Value>;

        bool has_specular = (ctx.component == (size_t) -1 || ctx.component == (size_t) 0)
                            && (ctx.type_mask & EGlossyReflection),
             has_diffuse  = (ctx.component == (size_t) -1 || ctx.component == (size_t) 1)
                            && (ctx.type_mask & EDiffuseReflection);

        Mask active(active_);

        if (!(has_specular || has_diffuse))
            return 0.0f;

        Value n_dot_wi = Frame::cos_theta(ctx.si.wi);
        Value n_dot_wo = Frame::cos_theta(wo);

        active = active && ((n_dot_wi > 0.0f) && (n_dot_wo > 0.0f));

        /* Construct the microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution<Value> distr(
            m_type,
            m_alpha,
            m_sample_visible
        );

        Spectrum result(0.0f);
        if (has_specular) {
            /* Calculate the reflection half-vector */
            Vector3 H = normalize(wo + ctx.si.wi);

            /* Evaluate the microfacet normal distribution */
            Value D = distr.eval(H, active);

            /* Fresnel term */
            Value F = fresnel_dielectric_ext(dot(ctx.si.wi, H),
                          luminance(m_eta->eval(ctx.si.wavelengths, active),
                                                ctx.si.wavelengths, active), active).first;

            /* Smith's shadow-masking function */
            Value G = distr.G(ctx.si.wi, wo, H, active);

            /* Calculate the specular reflection component */
            Value value = F * D * G / (4.0f * n_dot_wi);

            result += m_specular_reflectance->eval(ctx.si.wavelengths, active) * value;
        }

        if (has_diffuse) {
            Spectrum diff = m_diffuse_reflectance->eval(ctx.si.wavelengths, active);
            // Float T12 = m_external_rough_transmittance->eval(n_dot_wi, distr.alpha());
            // Float T21 = m_external_rough_transmittance->eval(n_dot_wo, distr.alpha());
            // Float Fdr = 1.f - m_internal_rough_transmittance->evalDiffuse(distr.alpha());
            Value T12 = m_specular_factor;
            Value T21 = m_specular_factor;
            Value Fdr = 1.f - m_specular_factor;

            if (m_nonlinear)
                diff /= Spectrum(1.0f) - diff * Fdr;
            else
                diff /= 1.0f - Fdr;

            result += diff * (InvPi * n_dot_wo * T12 * T21 * m_inv_eta_2->eval(ctx.si.wavelengths, active));
        }

        masked(result, !active) = Spectrum(0.f);
        return result;
    }

    template <typename BSDFContext,
              typename Value    = typename BSDFContext::Value,
              typename Vector3  = typename BSDFContext::Vector3>
    Value pdf_impl(const BSDFContext &ctx, const Vector3 &wo,
                   const mask_t<Value> &active_) const {
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        bool has_specular = (ctx.component == (size_t) -1 || ctx.component == (size_t) 0)
                            && (ctx.type_mask & EGlossyReflection),
             has_diffuse  = (ctx.component == (size_t) -1 || ctx.component == (size_t) 1)
                            && (ctx.type_mask & EDiffuseReflection);

        Mask active(active_);

        if (!(has_specular || has_diffuse))
            return 0.0f;

        Value n_dot_wi = Frame::cos_theta(ctx.si.wi);
        Value n_dot_wo = Frame::cos_theta(wo);

        active = active && ((n_dot_wi > 0.0f) && (n_dot_wo > 0.0f));

        /* Construct the microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution<Value> distr(
            m_type,
            m_alpha,
            m_sample_visible
        );

        /* Calculate the reflection half-vector */
        const Vector3 H = normalize(wo + ctx.si.wi);

        Value prob_diffuse, prob_specular;
        if (has_specular && has_diffuse) {
            /* Find the probability of sampling the specular component */
            // prob_specular = 1.0f - m_external_rough_transmittance->eval(n_dot_wi, distr.alpha(), active);
            prob_specular = m_specular_factor;

            /* Reallocate samples */
            prob_specular = (prob_specular * m_specular_sampling_weight) /
                (prob_specular * m_specular_sampling_weight +
                (1.0f - prob_specular) * (1.0f - m_specular_sampling_weight));

            prob_diffuse = 1.0f - prob_specular;
        } else {
            prob_diffuse = prob_specular = 1.0f;
        }

        Value result(0.0f);
        if (has_specular) {
            /* Jacobian of the half-direction mapping */
            const Value dwh_dwo = rcp(4.0f * dot(wo, H));

            /* Evaluate the microfacet model sampling density function */
            const Value prob = distr.pdf(ctx.si.wi, H, active);

            result = prob * dwh_dwo * prob_specular;
        }

        if (has_diffuse)
            result += prob_diffuse * warp::square_to_cosine_hemisphere_pdf(wo);

        masked(result, !active) = 0.0f;
        return result;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "RoughPlastic[" << std::endl
            << "  distribution = " << MicrofacetDistribution<Float>::distribution_name(m_type) << "," << std::endl
            << "  sample_visible = "           << m_sample_visible                    << "," << std::endl
            << "  alpha = "                    << m_alpha                             << "," << std::endl
            << "  specular_factor = "          << m_specular_factor                   << "," << std::endl
            << "  specular_reflectance = "     << m_specular_reflectance              << "," << std::endl
            << "  diffuse_reflectance = "      << m_diffuse_reflectance               << "," << std::endl
            << "  specular_sampling_weight = " << m_specular_sampling_weight          << "," << std::endl
            << "  diffuse_sampling_weight = "  << (1.0f - m_specular_sampling_weight) << "," << std::endl
            << "  eta = "                      << m_eta                               << "," << std::endl
            << "  nonlinear = "                << m_nonlinear                         << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_BSDF()
    MTS_DECLARE_CLASS()
private:
    EType m_type;
    ref<ContinuousSpectrum> m_diffuse_reflectance, m_specular_reflectance;
    ref<ContinuousSpectrum> m_eta, m_inv_eta_2;
    Float m_specular_factor;
    Float m_alpha;
    Float m_specular_sampling_weight;
    bool m_nonlinear;
    bool m_sample_visible;
};

MTS_IMPLEMENT_CLASS(RoughPlastic, BSDF)
MTS_EXPORT_PLUGIN(RoughPlastic, "Rough plastic BRDF");

NAMESPACE_END(mitsuba)
