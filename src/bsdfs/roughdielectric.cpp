#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/reflection.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

class RoughDielectric : public BSDF {
public:
    RoughDielectric(const Properties &props) {
        m_specular_reflectance   = props.spectrum("specular_reflectance",   1.0f);
        m_specular_transmittance = props.spectrum("specular_transmittance", 1.0f);

        // Specifies the internal index of refraction at the interface
        Float int_ior = lookup_ior(props, "int_ior", "bk7");

        // Specifies the external index of refraction at the interface
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

        MicrofacetDistribution<Float> distr(props);
        m_type = distr.type();
        m_sample_visible = distr.sample_visible();
        m_alpha_u = distr.alpha_u();
        m_alpha_v = distr.alpha_v();

        uint32_t extra = (m_alpha_u == m_alpha_v) ? EAnisotropic : (uint32_t) 0;
        m_components.push_back(EGlossyReflection | EFrontSide
            | EBackSide | extra);
        m_components.push_back(EGlossyTransmission | EFrontSide
            | EBackSide | ENonSymmetric | extra);

        m_flags = m_components[0] | m_components[1];
    }

    template <typename SurfaceInteraction,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Value      = typename SurfaceInteraction::Value,
              typename Point2     = typename SurfaceInteraction::Point2,
              typename Spectrum   = Spectrum<Value>>
    std::pair<BSDFSample, Spectrum> sample_impl(
            const BSDFContext &ctx, const SurfaceInteraction &si, Value sample1,
            const Point2 &sample, mask_t<Value> active) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Normal3 = typename BSDFSample::Normal3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        // Determine the type of interaction
        bool has_reflection    = ctx.is_enabled(EGlossyReflection, 0);
        bool has_transmission  = ctx.is_enabled(EGlossyTransmission, 1);

        BSDFSample bs;
        if (!has_reflection && !has_transmission)
            return { bs, 0.0f };

        Value cos_theta_wi = Frame::cos_theta(si.wi);

        /* Construct the microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution<Value> distr(
            m_type,
            m_alpha_u,
            m_alpha_v,
            m_sample_visible
        );

        /* Trick by Walter et al.: slightly scale the roughness values to
           reduce importance sampling weights. Not needed for the
           Heitz and D'Eon sampling technique. */
        MicrofacetDistribution<Value> sample_distr(distr);
        if (!m_sample_visible)
            sample_distr.scale_alpha(1.2f - 0.2f * sqrt(abs(cos_theta_wi)));

        // Sample M, the microfacet normal
        Value microfacet_pdf;
        Normal3 m;
        std::tie(m, microfacet_pdf)
            = sample_distr.sample(sign(cos_theta_wi) * si.wi, sample);

        active = active && neq(microfacet_pdf, 0.0f);
        if (none(active))
            return { bs, 0.0f };

        bs.pdf = microfacet_pdf;
        Value eta_value = enoki::mean(m_eta->eval(si.wavelengths, active));
        Value inv_eta_value = rcp(eta_value);

        Value F, cos_theta_t;
        std::tie(F, cos_theta_t, std::ignore, std::ignore) =
            fresnel(dot(si.wi, m), eta_value);

        Mask sampled_reflection = has_reflection;
        Spectrum weight(1.0f);
        if (has_reflection && has_transmission) {
            sampled_reflection = (sample1 <= F);
            bs.pdf *= select(sampled_reflection, F, (1.0f - F));
        } else {
            weight *= has_reflection ? F : (1.0f - F);
        }

        Value dwh_dwo;
        // ----- Reflection sampling
        Mask mask = sampled_reflection && active;
        if (any(mask)) {
            // Perfect specular reflection based on the microfacet normal
            masked(bs.wo,  mask)               = reflect(si.wi, m);
            masked(bs.eta, mask)               = 1.0f;
            masked(bs.sampled_component, mask) = 0;
            masked(bs.sampled_type, mask)      = EGlossyReflection;

            // Side check
            masked(active, mask) = active
                                && (cos_theta_wi * Frame::cos_theta(bs.wo) > 0.0f);
            mask = mask && active;

            masked(weight, mask) = weight * m_specular_reflectance->eval(si.wavelengths);

            // Jacobian of the half-direction mapping
            masked(dwh_dwo, mask) = rcp(4.0f * dot(bs.wo, m));
        }

        // ----- Transmission sampling
        mask = !sampled_reflection && neq(cos_theta_t, 0.0f) && active;
        if (any(mask)) {
            // Perfect specular transmission based on the microfacet normal
            masked(bs.wo, mask)  = refract(si.wi, m, eta_value, cos_theta_t);
            masked(bs.eta, mask) = select(cos_theta_t < 0.0f,
                                          eta_value,
                                          inv_eta_value);
            masked(bs.sampled_component, mask) = 1;
            masked(bs.sampled_type, mask) = EGlossyTransmission;

            // Side check
            masked(active, mask) = active && neq(cos_theta_t, 0.0f)
                                          && (cos_theta_wi * Frame::cos_theta(bs.wo) < 0.0f);
            mask = mask && active;

            /* Radiance must be scaled to account for the solid angle compression
               that occurs when crossing the interface. */
            Value factor = (ctx.mode == ERadiance)
                ? select(cos_theta_t < 0.0f,
                         inv_eta_value,
                         eta_value)
                : Value(1.0f);

            masked(weight, mask) =
                weight * m_specular_transmittance->eval(si.wavelengths)
                       * (factor * factor);

            // Jacobian of the half-direction mapping
            Value sqrt_denom = dot(si.wi, m) + bs.eta * dot(bs.wo, m);
            masked(dwh_dwo, mask) = (bs.eta * bs.eta * dot(bs.wo, m))
                                    / (sqrt_denom * sqrt_denom);
        }

        if (none(active))
            return { bs, 0.0f };

        if (m_sample_visible)
            weight *= distr.smith_g1(bs.wo, m);
        else
            weight *= abs(distr.eval(m)
                      * distr.G(si.wi, bs.wo, m) * dot(si.wi, m)
                      / (microfacet_pdf * cos_theta_wi));

        bs.pdf *= abs(dwh_dwo);

        // Zero-out inactive lanes
        masked(weight, !active) = Spectrum(0.0f);
        return { bs, weight };
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                       const Vector3 &wo, mask_t<Value> active) const {
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        Value cos_theta_wi = Frame::cos_theta(si.wi);
        active = active && neq(cos_theta_wi, 0.0f);

        // Determine the type of interaction
        bool has_reflection   = ctx.is_enabled(EGlossyReflection, 0);
        bool has_transmission = ctx.is_enabled(EGlossyTransmission, 1);
        Mask reflect = (cos_theta_wi * Frame::cos_theta(wo) > 0.0f);

        Vector3 H;

        active = active && (   (Mask(has_reflection)   && reflect)
                            || (Mask(has_transmission) && !reflect));
        if (none(active))
            return Spectrum(0.0f);

        Value eta_value = enoki::mean(m_eta->eval(si.wavelengths, active));
        Value inv_eta_value = rcp(eta_value);

        // Compute the half-vector
        Value eta = select(reflect,
                           Value(1.0f),                // reflection
                           select(cos_theta_wi > 0.0f, // transmission
                                  eta_value,
                                  inv_eta_value));

        H = normalize(si.wi + wo * eta);

        /* Ensure that the half-vector points into the
           same hemisphere as the macrosurface normal */
        H *= sign(Frame::cos_theta(H));

        /* Construct the microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution<Value> distr(
            m_type,
            m_alpha_u,
            m_alpha_v,
            m_sample_visible
        );

        // Evaluate the microfacet normal distribution
        Value D = distr.eval(H);

        active = active && neq(D, 0.0f);
        if (none(active))
            return Spectrum(0.0f);

        // Fresnel factor
        Value F = std::get<0>(fresnel(dot(si.wi, H), eta_value));

        // Smith's shadow-masking function
        Value G = distr.G(si.wi, wo, H);

        Spectrum result(0.0f);

        if (has_reflection && any(reflect && active)) {
            // Compute the total amount of reflection
            Value value = F * D * G / (4.0f * abs(cos_theta_wi));

            masked(result, reflect && active)
                = m_specular_reflectance->eval(si.wavelengths) * value;
        }

        if (has_transmission && any(!reflect && active)) {
            // Compute the total amount of transmission
            Value sqrt_denom = dot(si.wi, H) + eta * dot(wo, H);
            Value value = ((1.0f - F) * D * G * eta * eta
                * dot(si.wi, H) * dot(wo, H)) /
                (cos_theta_wi * sqrt_denom * sqrt_denom);

            /* Missing term in the original paper: account for the solid angle
               compression when tracing radiance -- this is necessary for
               bidirectional methods. */
            Value factor = (ctx.mode == ERadiance)
                ? select(cos_theta_wi > 0.0f,
                         inv_eta_value,
                         eta_value)
                : Value(1.0f);

            masked(result, (!reflect && active))
                = m_specular_transmittance->eval(si.wavelengths)
                  * abs(value * factor * factor);
        }

        return result;
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3>
    Value pdf_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                   const Vector3 &wo, mask_t<Value> active) const {
        using Frame = Frame<Vector3>;
        using Mask  = mask_t<Value>;

        // Determine the type of interaction
        bool has_reflection   = ctx.is_enabled(EGlossyReflection, 0);
        bool has_transmission = ctx.is_enabled(EGlossyTransmission, 1);
        Mask reflect = (Frame::cos_theta(si.wi) * Frame::cos_theta(wo) > 0.0f);

        Value eta_value     = enoki::mean(m_eta->eval(si.wavelengths, active));
        Value inv_eta_value = rcp(eta_value);

        Vector3 H;
        Value dwh_dwo(0.0f);

        Mask mask = (reflect && active);
        if (has_reflection && any(mask)) {
            // Compute the reflection half-vector
            masked(H, mask) = normalize(wo + si.wi);

            // Jacobian of the half-direction mapping
            masked(dwh_dwo, mask) = rcp(4.0f * dot(wo, H));
        }

        mask = !reflect && active;
        if (has_transmission && any(mask)) {
            // Compute the transmission half-vector
            Value eta = select(Frame::cos_theta(si.wi) > 0.0f,
                               eta_value,
                               inv_eta_value);
            masked(H, mask) = normalize(si.wi + wo * eta);

            // Jacobian of the half-direction mapping
            Value sqrt_denom = dot(si.wi, H) + eta * dot(wo, H);
            masked(dwh_dwo, mask) = (eta * eta * dot(wo, H))
                                    / (sqrt_denom * sqrt_denom);
        }

        active = active && neq(dwh_dwo, 0.0f);
        if (none(active))
            return 0.0f;

        /* Ensure that the half-vector points into the
           same hemisphere as the macrosurface normal */
        H *= sign(Frame::cos_theta(H));

        /* Construct the microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution<Value> sample_distr(
            m_type,
            m_alpha_u,
            m_alpha_v,
            m_sample_visible
        );

        /* Trick by Walter et al.: slightly scale the roughness values to
           reduce importance sampling weights. Not needed for the
           Heitz and D'Eon sampling technique. */
        if (!m_sample_visible)
            sample_distr.scale_alpha(1.2f - 0.2f * sqrt(abs(Frame::cos_theta(si.wi))));

        // Evaluate the microfacet model sampling density function
        Value prob = sample_distr.pdf(sign(Frame::cos_theta(si.wi)) * si.wi, H);

        if (has_transmission && has_reflection) {
            Value F = std::get<0>(fresnel(dot(si.wi, H), eta_value));
            prob *= select(reflect, F, (1.0f - F));
        }

        return abs(prob * dwh_dwo);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "RoughDielectric[" << std::endl
            << "  distribution = ";
        if (m_type == EBeckmann)
            oss << "beckmann";
        else
            oss << "ggx";
        oss << "," << std::endl
            << "  sample_visible = "         << m_sample_visible << "," << std::endl
            << "  alpha_u = "                << m_alpha_u        << "," << std::endl
            << "  alpha_v = "                << m_alpha_v        << "," << std::endl
            << "  eta = "                    << m_eta            << "," << std::endl
            << "  specular_reflectance = "   << string::indent(m_specular_reflectance)   << "," << std::endl
            << "  specular_transmittance = " << string::indent(m_specular_transmittance) << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_BSDF()
    MTS_DECLARE_CLASS()

private:
    EType m_type;
    Float m_alpha_u, m_alpha_v;
    bool m_sample_visible;
    ref<ContinuousSpectrum> m_eta;
    ref<ContinuousSpectrum> m_specular_reflectance, m_specular_transmittance;
};

MTS_IMPLEMENT_CLASS(RoughDielectric, BSDF)
MTS_EXPORT_PLUGIN(RoughDielectric, "Rough dielectric BSDF");

NAMESPACE_END(mitsuba)
