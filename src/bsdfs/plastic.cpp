#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/reflection.h>

NAMESPACE_BEGIN(mitsuba)

class SmoothPlastic : public BSDF {
public:
    SmoothPlastic(const Properties &props) {
        m_flags = EDeltaReflection | EDiffuseReflection | EFrontSide;
        m_components.clear();
        m_components.push_back(EDeltaReflection | EFrontSide);
        m_components.push_back(EDiffuseReflection | EFrontSide);

        /* Specifies the internal index of refraction at the interface */
        Float int_ior = lookup_ior(props, "int_ior", "polypropylene");

        /* Specifies the external index of refraction at the interface */
        Float ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0.0f || ext_ior < 0.0f)
            Throw("The interior and exterior indices of "
                  "refraction must be positive!");
        Float eta = int_ior / ext_ior;

        Properties props2("uniform");
        props2.set_float("value", eta);
        m_eta = (ContinuousSpectrum *) PluginManager::instance()
            ->create_object<ContinuousSpectrum>(props2).get();

        props2.set_float("value", rcp(eta * eta), false);
        m_inv_eta_2 = (ContinuousSpectrum *) PluginManager::instance()
            ->create_object<ContinuousSpectrum>(props2).get();

        // TODO: verify the input parameters and fix them if necessary
        m_specular_reflectance = props.spectrum("specular_reflectance", 1.0f);
        m_diffuse_reflectance  = props.spectrum("diffuse_reflectance",  0.5f);

        // Numerically approximate the diffuse Fresnel reflectance
        props2.set_float("value", fresnel_diffuse_reflectance(1.0f / eta), false);
        m_fdr_int = (ContinuousSpectrum *) PluginManager::instance()
            ->create_object<ContinuousSpectrum>(props2).get();

        props2.set_float("value", fresnel_diffuse_reflectance(eta), false);
        m_fdr_ext = (ContinuousSpectrum *) PluginManager::instance()
            ->create_object<ContinuousSpectrum>(props2).get();

        /* Compute weights that further steer samples towards
           the specular or diffuse components */
        Float d_avg = mean(m_diffuse_reflectance),
              s_avg = mean(m_specular_reflectance);

        m_specular_sampling_weight = s_avg / (d_avg + s_avg);

        m_nonlinear = props.bool_("nonlinear", false);
    }

    template <typename SurfaceInteraction,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Value      = typename SurfaceInteraction::Value,
              typename Point2     = typename SurfaceInteraction::Point2,
              typename Spectrum   = Spectrum<Value>>
    std::pair<BSDFSample, Spectrum> sample_impl(
            const BSDFContext &ctx, const SurfaceInteraction &si,
            Value sample1, const Point2 &sample2,
            mask_t<Value> active) const {
        using Index   = typename BSDFSample::Index;
        using Vector3 = typename BSDFSample::Vector3;
        using Index   = uint32_array_t<Value>;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        bool has_specular = ctx.is_enabled(EDeltaReflection, 0);
        bool has_diffuse = ctx.is_enabled(EDiffuseReflection, 1);

        Value n_dot_wi = Frame::cos_theta(si.wi);
        active = active && (n_dot_wi > 0.0f);

        BSDFSample bs;
        if ((!has_specular && !has_diffuse) || none(active))
            return { bs, 0.0f };

        bs.eta = 1.0f;
        Spectrum fdr_int   = m_fdr_int->eval(si.wavelengths, active);
        Spectrum inv_eta_2 = m_inv_eta_2->eval(si.wavelengths, active);

        Value eta_lum = luminance(m_eta->eval(si.wavelengths, active),
                                  si.wavelengths, active);
        // TODO: should pass Spectrum to fresnel
        Value fi = std::get<0>(fresnel(n_dot_wi, eta_lum));

        Spectrum result;
        if (has_diffuse && has_specular) {
            Value prob_specular = (fi * m_specular_sampling_weight)
                                  / (fi * m_specular_sampling_weight +
                                     (1.0f - fi) * (1.0f - m_specular_sampling_weight));

            // Importance sample wrt. the Fresnel reflectance
            Mask sample_specular = (sample1 < prob_specular);
            bs.sampled_component = select(sample_specular, Index(0), Index(1));
            bs.sampled_type      = select(sample_specular,
                                          Index(EDeltaReflection),
                                          Index(EDiffuseReflection));
            bs.wo = select(sample_specular,
                           reflect(si.wi),
                           warp::square_to_cosine_hemisphere(sample2));

            bs.pdf = select(sample_specular,
                               prob_specular,
                               (1.0f - prob_specular)
                               * warp::square_to_cosine_hemisphere_pdf(bs.wo));

            // Compute value for diffuse sample
            // TODO: should pass Spectrum to fresnel
            Value fo = std::get<0>(fresnel(Frame::cos_theta(bs.wo), eta_lum));
            Spectrum diff = m_diffuse_reflectance->eval(si.wavelengths, active);
            if (m_nonlinear)
                diff /= Spectrum(1.0f) - diff * fdr_int;
            else
                diff /= Spectrum(1.0f) - fdr_int;

            result = diff * (inv_eta_2 * (1.0f - fi) * (1.0f - fo)
                                      / (1.0f - prob_specular));
            masked(result, sample_specular)
                = m_specular_reflectance->eval(si.wavelengths, active) * fi / prob_specular;
        } else if (has_specular) {
            bs.sampled_component = 0;
            bs.sampled_type = EDeltaReflection;
            bs.wo = reflect(si.wi);
            bs.pdf = 1.0f;
            result = m_specular_reflectance->eval(si.wavelengths, active) * fi;
        } else {
            bs.sampled_component = 1;
            bs.sampled_type = EDiffuseReflection;
            bs.wo = warp::square_to_cosine_hemisphere(sample2);
            // TODO: should pass Spectrum to fresnel
            Value fo = std::get<0>(fresnel(Frame::cos_theta(bs.wo), eta_lum));

            Spectrum diff = m_diffuse_reflectance->eval(si.wavelengths, active);

            if (m_nonlinear)
                diff /= Spectrum(1.0f) - diff * fdr_int;
            else
                diff /= 1.0f - fdr_int;

            bs.pdf = warp::square_to_cosine_hemisphere_pdf(bs.wo);

            result = diff * (inv_eta_2 * (1.0f - fi) * (1.0f - fo));
        }

        masked(bs.pdf, !active) = 0.0f;
        return { bs, result };
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                       const Vector3 &wo, mask_t<Value> active) const {
        using Frame = Frame<Vector3>;

        bool has_specular = ctx.is_enabled(EDeltaReflection, 0);
        bool has_diffuse = ctx.is_enabled(EDiffuseReflection, 1);

        Value n_dot_wi = Frame::cos_theta(si.wi);
        Value n_dot_wo = Frame::cos_theta(wo);

        active = active && (n_dot_wi > 0.0f) && (n_dot_wo > 0.0f);
        if (none(active))
            return Spectrum(0.0f);

        Value eta_lum = luminance(m_eta->eval(si.wavelengths, active),
                                  si.wavelengths, active);
        // TODO: should pass Spectrum to fresnel
        Value fi = std::get<0>(fresnel(n_dot_wi, eta_lum));

        Spectrum result(0.0f);
        if (has_specular) {
            /* Check if the provided direction pair matches an ideal
               specular reflection; tolerate some roundoff errors */
            masked(result, active && (abs(dot(reflect(si.wi), wo) - 1.0f) < math::DeltaEpsilon))
                = m_specular_reflectance->eval(si.wavelengths, active) * fi;
        } else if (has_diffuse) {
            // TODO: should pass Spectrum to fresnel
            Value fo = std::get<0>(fresnel(n_dot_wo, eta_lum));

            Spectrum diff      = m_diffuse_reflectance->eval(si.wavelengths, active);
            Spectrum fdr_int   = m_fdr_int->eval(si.wavelengths, active);
            Spectrum inv_eta_2 = m_inv_eta_2->eval(si.wavelengths, active);

            if (m_nonlinear)
                diff /= Spectrum(1.0f) - diff * fdr_int;
            else
                diff /= 1.0f - fdr_int;

            masked(result, active) = diff
                * (warp::square_to_cosine_hemisphere_pdf(wo)
                * inv_eta_2 * (1.0f - fi) * (1.0f - fo));
        }

        return result;
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3>
    Value pdf_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                   const Vector3 &wo, mask_t<Value> active) const {
        using Frame = Frame<Vector3>;

        bool has_specular = ctx.is_enabled(EDeltaReflection, 0);
        bool has_diffuse = ctx.is_enabled(EDiffuseReflection, 1);

        Value n_dot_wi = Frame::cos_theta(si.wi);
        Value n_dot_wo = Frame::cos_theta(wo);

        active = active && (n_dot_wi > 0.0f) && (n_dot_wo > 0.0f);
        if (none(active))
            return 0.0f;

        Value prob_specular = has_specular ? 1.0f : 0.0f;
        if (has_specular && has_diffuse) {
            // TODO: should pass Spectrum to fresnel
            Value eta_lum = luminance(m_eta->eval(si.wavelengths, active),
                                      si.wavelengths, active);
            Value fi = std::get<0>(fresnel(n_dot_wi, eta_lum));
            prob_specular = (fi * m_specular_sampling_weight) /
                (fi * m_specular_sampling_weight +
                (1.0f - fi) * (1.0f - m_specular_sampling_weight));
        }

        Value pdf(0.0f);
        if (has_specular) {
            /* Check if the provided direction pair matches an ideal
               specular reflection; tolerate some roundoff errors */
            auto matches = abs(dot(reflect(si.wi), wo) - 1.0f) < math::DeltaEpsilon;
            masked(pdf, active && matches) = prob_specular;
        } else if (has_diffuse) {
            masked(pdf, active)
                = warp::square_to_cosine_hemisphere_pdf(wo) * (1.0f - prob_specular);
        }

        return pdf;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothPlastic[" << std::endl
            << "  specular_reflectance = "     << m_specular_reflectance              << "," << std::endl
            << "  diffuse_reflectance = "      << m_diffuse_reflectance               << "," << std::endl
            << "  specular_sampling_weight = " << m_specular_sampling_weight          << "," << std::endl
            << "  diffuse_sampling_Weight = "  << (1.0f - m_specular_sampling_weight) << "," << std::endl
            << "  nonlinear = "                << m_nonlinear                         << "," << std::endl
            << "  eta = "                      << m_eta                               << "," << std::endl
            << "  fdr_int = "                  << m_fdr_int                           << "," << std::endl
            << "  fdr_ext = "                  << m_fdr_ext                           << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_BSDF()
    MTS_DECLARE_CLASS()
private:
    ref<ContinuousSpectrum> m_fdr_int, m_fdr_ext, m_eta, m_inv_eta_2;
    ref<ContinuousSpectrum> m_diffuse_reflectance, m_specular_reflectance;
    Float m_specular_sampling_weight;
    bool m_nonlinear;
};


MTS_IMPLEMENT_CLASS(SmoothPlastic, BSDF)
MTS_EXPORT_PLUGIN(SmoothPlastic, "Smooth plastic BRDF");

NAMESPACE_END(mitsuba)
