#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/reflection.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

class RoughPlastic : public BSDF {
public:
    RoughPlastic(const Properties &props) {
        m_flags =  EGlossyReflection | EDiffuseReflection | EFrontSide;
        m_components.clear();
        m_components.push_back(EGlossyReflection | EFrontSide);
        m_components.push_back(EDiffuseReflection | EFrontSide);

        m_specular_reflectance = props.spectrum("specular_reflectance", 1.0f);
        m_diffuse_reflectance  = props.spectrum("diffuse_reflectance",  0.5f);

        // TODO: Verify the input parameters and fix them if necessary

        /* Compute weights that further steer samples towards
           the specular or diffuse components */
        Float d_avg = mean(m_diffuse_reflectance),
              s_avg = mean(m_specular_reflectance);
        m_specular_sampling_weight = s_avg / (d_avg + s_avg);

        /// Specifies the internal index of refraction at the interface
        Float int_ior = lookup_ior(props, "int_ior", "polypropylene");

        /// Specifies the external index of refraction at the interface
        Float ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0.0f || ext_ior < 0.0f || int_ior == ext_ior)
            Throw("The interior and exterior indices of "
                  "refraction must be positive and differ!");

        // TODO: nicer way to create uniform spectra.
        Properties props2("uniform");
        props2.set_float("value", int_ior / ext_ior);
        m_eta = (ContinuousSpectrum *)
            PluginManager::instance()
                ->create_object<ContinuousSpectrum>(props2).get();

        props2.set_float("value", 1.0f / (m_eta * m_eta), false);
        m_inv_eta_2 = (ContinuousSpectrum *)
            PluginManager::instance()
                ->create_object<ContinuousSpectrum>(props2).get();

        m_nonlinear = props.bool_("nonlinear", false);

        MicrofacetDistribution<Float> distr(props);
        m_type = distr.type();
        m_sample_visible = distr.sample_visible();

        if (distr.is_anisotropic())
            Throw("The 'roughplastic' plugin currently does not support "
                  "anisotropic microfacet distributions!");

        m_alpha = distr.alpha();

        m_specular_factor = props.float_("specular_factor", 0.5f);

        /* TODO: use proper rough transmittance computations.
         * Here we added an interpolation parameter “specular_factor”
         * and we blend between a diffuse and a microfacet model. */
    }

    template <typename SurfaceInteraction,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Value      = typename SurfaceInteraction::Value,
              typename Point2     = typename SurfaceInteraction::Point2,
              typename Spectrum   = Spectrum<Value>>
    std::pair<BSDFSample, Spectrum> sample_impl(
            const BSDFContext &ctx, const SurfaceInteraction &si,
            Value sample1, const Point2 &sample2, mask_t<Value> active) const {
        using Index   = typename BSDFSample::Index;
        using Vector3 = typename BSDFSample::Vector3;
        using Normal3 = typename BSDFSample::Normal3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        bool has_specular = ctx.is_enabled(EGlossyReflection, 0);
        bool has_diffuse  = ctx.is_enabled(EDiffuseReflection, 1);

        BSDFSample bs;
        bs.pdf = 0.0f;
        if (!has_specular && !has_diffuse)
            return { bs, 0.0f };

        Value n_dot_wi = Frame::cos_theta(si.wi);

        active = active && (n_dot_wi > 0.0f);

        Mask chose_specular = has_specular;

        /* Construct the microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution<Value> distr(
            m_type,
            m_alpha,
            m_sample_visible
        );

        Value prob_specular;
        if (has_specular && has_diffuse) {
            // Find the probability of sampling the specular component
            // prob_specular = 1.0f - m_external_rough_transmittance->eval(n_dot_wi, distr.alpha(), active);
            prob_specular = (m_specular_factor * m_specular_sampling_weight) /
                (m_specular_factor * m_specular_sampling_weight +
                (1.0f - m_specular_factor) * (1.0f - m_specular_sampling_weight));

            chose_specular = sample1 < prob_specular;
        }

        // Perfect specular reflection based on the microfacet normal
        Normal3 m = distr.sample(si.wi, sample2).first;
        bs.wo = select(chose_specular,
                       reflect(si.wi, m),
                       warp::square_to_cosine_hemisphere(sample2));
        bs.sampled_component = select(chose_specular, Index(0), Index(1));

        bs.sampled_type = select(chose_specular,
                                 Index(EGlossyReflection),
                                 Index(EDiffuseReflection));
        bs.eta = 1.0f;

        // Side check
        active = active && !(chose_specular && (Frame::cos_theta(bs.wo) <= 0.0f));

        if (none(active))
            return { bs, 0.0f };

        // Guard against numerical imprecisions
        Value pdf = pdf_impl(ctx, si, bs.wo, active);

        active = active && neq(pdf, 0.0f);

        if (none(active))
            return { bs, 0.0f };

        Spectrum result = select(active,
                                 eval_impl(ctx, si, bs.wo, active) / pdf,
                                 Spectrum(0.0f));

        masked(bs.pdf, active) = pdf;

        return { bs, result };
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                       const Vector3 &wo, mask_t<Value> active) const {
        using Frame = Frame<Vector3>;

        bool has_specular = ctx.is_enabled(EGlossyReflection, 0);
        bool has_diffuse  = ctx.is_enabled(EDiffuseReflection, 1);
        if (!(has_specular || has_diffuse))
            return 0.0f;

        Value n_dot_wi = Frame::cos_theta(si.wi);
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
            // Calculate the reflection half-vector
            Vector3 H = normalize(wo + si.wi);

            // Evaluate the microfacet normal distribution
            Value D = distr.eval(H);

            // Fresnel term
            Value F = std::get<0>(
                fresnel(dot(si.wi, H),
                        luminance(m_eta->eval(si, active),
                                  si.wavelengths, active)));

            // Smith's shadow-masking function
            Value G = distr.G(si.wi, wo, H);

            // Calculate the specular reflection component
            Value value = F * D * G / (4.0f * n_dot_wi);

            result += m_specular_reflectance->eval(si, active) * value;
        }

        if (has_diffuse) {
            Spectrum diff = m_diffuse_reflectance->eval(si, active);
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

            result += diff * (InvPi * n_dot_wo * T12 * T21 * m_inv_eta_2->eval(si, active));
        }

        masked(result, !active) = Spectrum(0.f);
        return result;
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3>
    Value pdf_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                   const Vector3 &wo, mask_t<Value> active) const {
        using Frame   = Frame<Vector3>;

        bool has_specular = ctx.is_enabled(EGlossyReflection, 0);
        bool has_diffuse  = ctx.is_enabled(EDiffuseReflection, 1);
        if (!(has_specular || has_diffuse))
            return 0.0f;

        Value n_dot_wi = Frame::cos_theta(si.wi);
        Value n_dot_wo = Frame::cos_theta(wo);

        active = active && ((n_dot_wi > 0.0f) && (n_dot_wo > 0.0f));

        /* Construct the microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution<Value> distr(
            m_type,
            m_alpha,
            m_sample_visible
        );

        // Calculate the reflection half-vector
        const Vector3 H = normalize(wo + si.wi);

        Value prob_diffuse, prob_specular;
        if (has_specular && has_diffuse) {
            /* Find the probability of sampling the specular component */
            // prob_specular = 1.0f - m_external_rough_transmittance->eval(n_dot_wi, distr.alpha(), active);
            prob_specular = (m_specular_factor * m_specular_sampling_weight) /
                (m_specular_factor * m_specular_sampling_weight +
                (1.0f - m_specular_factor) * (1.0f - m_specular_sampling_weight));

            prob_diffuse = 1.0f - prob_specular;
        } else {
            prob_diffuse = prob_specular = 1.0f;
        }

        Value result;
        if (has_specular) {
            // Jacobian of the half-direction mapping
            const Value dwh_dwo = rcp(4.0f * dot(wo, H));

            // Evaluate the microfacet model sampling density function
            const Value prob = distr.pdf(si.wi, H);

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
            << "  distribution = ";
        if (m_type == EBeckmann)
            oss << "beckmann";
        else
            oss << "ggx";
        oss << "," << std::endl
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
