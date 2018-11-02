#include <mitsuba/core/fwd.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

class RoughDielectric : public BSDF {
public:
    RoughDielectric(const Properties &props) {
        m_specular_reflectance   = props.spectrum("specular_reflectance",   1.f);
        m_specular_transmittance = props.spectrum("specular_transmittance", 1.f);

        // Specifies the internal index of refraction at the interface
        Float int_ior = lookup_ior(props, "int_ior", "bk7");

        // Specifies the external index of refraction at the interface
        Float ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0.f || ext_ior < 0.f || int_ior == ext_ior)
            Throw("The interior and exterior indices of "
                  "refraction must be positive and differ!");

        m_eta = int_ior / ext_ior;
        m_inv_eta = ext_ior / int_ior;

        MicrofacetDistribution<Float> distr(props);
        m_type = distr.type();
        m_sample_visible = distr.sample_visible();
        m_alpha_u = distr.alpha_u();
        m_alpha_v = distr.alpha_v();

        uint32_t extra = (m_alpha_u == m_alpha_v) ? EAnisotropic : (uint32_t) 0;
        m_components.push_back(EGlossyReflection | EFrontSide | EBackSide | extra);
        m_components.push_back(EGlossyTransmission | EFrontSide | EBackSide |
                               ENonSymmetric | extra);

        m_flags = m_components[0] | m_components[1];
    }

    template <typename SurfaceInteraction,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Value      = typename SurfaceInteraction::Value,
              typename Point2     = typename SurfaceInteraction::Point2,
              typename Spectrum   = Spectrum<Value>>
    MTS_INLINE
    std::pair<BSDFSample, Spectrum> sample_impl(
            const BSDFContext &ctx, const SurfaceInteraction &si, Value sample1,
            const Point2 &sample2, mask_t<Value> active) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Normal3 = typename BSDFSample::Normal3;
        using Index   = typename BSDFSample::Index;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        // Determine the type of interaction
        bool has_reflection    = ctx.is_enabled(EGlossyReflection, 0),
             has_transmission  = ctx.is_enabled(EGlossyTransmission, 1);

        BSDFSample bs;

        Value cos_theta_i = Frame::cos_theta(si.wi);

        /* Ignore perfectly grazing configurations */
        active &= neq(cos_theta_i, 0.f);

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
        if (unlikely(!m_sample_visible))
            sample_distr.scale_alpha(1.2f - .2f * sqrt(abs(cos_theta_i)));

        /* Sample the microfacet normal */
        Normal3 m;
        std::tie(m, bs.pdf) =
            sample_distr.sample(mulsign(si.wi, cos_theta_i), sample2);
        active &= neq(bs.pdf, 0.f);

        auto [F, cos_theta_t, eta_it, eta_ti] =
            fresnel(dot(si.wi, m), Value(m_eta));

        /* Select the lobe to be sampled */
        Spectrum weight;
        Mask selected_r, selected_t;
        if (likely(has_reflection && has_transmission)) {
            selected_r = sample1 <= F && active;
            weight = 1.f;
            bs.pdf *= select(selected_r, F, 1.f - F);
        } else {
            if (has_reflection || has_transmission) {
                selected_r = Mask(has_reflection) && active;
                weight = has_reflection ? F : (1.f - F);
            } else {
                return { bs, 0.f };
            }
        }

        selected_t = !selected_r && active;

        bs.eta               = select(selected_r, Value(1.f), eta_it);
        bs.sampled_component = select(selected_r, Index(0), Index(1));
        bs.sampled_type      = select(selected_r, Index(EGlossyReflection),
                                 Index(EGlossyTransmission));

        Value dwh_dwo = 0.f;

        /* Reflection sampling */
        if (any(selected_r)) {
            /* Perfect specular reflection based on the microfacet normal */
            bs.wo[selected_r] = reflect(si.wi, m);

            /* Ignore samples that ended up on the wrong side */
            active &= selected_t || (cos_theta_i * Frame::cos_theta(bs.wo) > 0.f);

            weight[selected_r] *= m_specular_reflectance->eval(si, selected_r && active);

            /* Jacobian of the half-direction mapping */
            dwh_dwo = rcp(4.f * dot(bs.wo, m));
        }

        /* Transmission sampling */
        if (any(selected_t)) {
            /* Perfect specular transmission based on the microfacet normal */
            bs.wo[selected_t]  = refract(si.wi, m, cos_theta_t, eta_ti);

            /* Ignore samples that ended up on the wrong side */
            active &= selected_r || (cos_theta_i * Frame::cos_theta(bs.wo) < 0.f);

            /* For transmission, radiance must be scaled to account for the solid
               angle compression that occurs when crossing the interface. */
            Value factor = (ctx.mode == ERadiance) ? eta_ti : Value(1.f);

            weight[selected_t] *= m_specular_transmittance->eval(si, selected_t && active)
                * sqr(factor);

            /* Jacobian of the half-direction mapping */
            masked(dwh_dwo, selected_t) =
                (sqr(bs.eta) * dot(bs.wo, m)) /
                 sqr(dot(si.wi, m) + bs.eta * dot(bs.wo, m));
        }

        if (likely(m_sample_visible))
            weight *= distr.smith_g1(bs.wo, m);
        else
            weight *= distr.G(si.wi, bs.wo, m) * dot(si.wi, m) /
                      (cos_theta_i * Frame::cos_theta(m));

        bs.pdf *= abs(dwh_dwo);

        return { bs, select(active, weight, 0.f) };
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3,
              typename Spectrum = Spectrum<Value>>
    MTS_INLINE
    Spectrum eval_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                                  const Vector3 &wo, mask_t<Value> active) const {
        using Frame = Frame<Vector3>;
        using Mask  = mask_t<Value>;

        Value cos_theta_i = Frame::cos_theta(si.wi),
              cos_theta_o = Frame::cos_theta(wo);

        /* Ignore perfectly grazing configurations */
        active &= neq(cos_theta_i, 0.f);

        // Determine the type of interaction
        bool has_reflection   = ctx.is_enabled(EGlossyReflection, 0),
             has_transmission = ctx.is_enabled(EGlossyTransmission, 1);

        Mask reflect = cos_theta_i * cos_theta_o > 0.f;

        /* Determine the relative index of refraction */
        Value eta     = select(cos_theta_i > 0.f, Value(m_eta), Value(m_inv_eta)),
              inv_eta = select(cos_theta_i > 0.f, Value(m_inv_eta), Value(m_eta));

        /* Compute the half-vector */
        Vector3 m = normalize(si.wi + wo * select(reflect, Value(1.f), eta));

        /* Ensure that the half-vector points into the
           same hemisphere as the macrosurface normal */
        m = mulsign(m, Frame::cos_theta(m));

        /* Construct the microfacet distribution matching the
           roughness values at the current surface position. */
        MicrofacetDistribution<Value> distr(
            m_type,
            m_alpha_u,
            m_alpha_v,
            m_sample_visible
        );

        /* Evaluate the microfacet normal distribution */
        Value D = distr.eval(m);

        /* Fresnel factor */
        Value F = std::get<0>(fresnel(dot(si.wi, m), Value(m_eta)));

        /* Smith's shadow-masking function */
        Value G = distr.G(si.wi, wo, m);

        Spectrum result(0.f);

        Mask eval_r = Mask(has_reflection) && reflect && active,
             eval_t = Mask(has_transmission) && !reflect && active;

        if (any(eval_r)) {
            Value value = F * D * G / (4.f * abs(cos_theta_i));

            result[eval_r] = m_specular_reflectance->eval(si, eval_r) * value;
        }

        if (any(eval_t)) {
            // Compute the total amount of transmission
            Value value =
                ((1.f - F) * D * G * eta * eta * dot(si.wi, m) * dot(wo, m)) /
                (cos_theta_i * sqr(dot(si.wi, m) + eta * dot(wo, m)));

            /* Missing term in the original paper: account for the solid angle
               compression when tracing radiance -- this is necessary for
               bidirectional methods. */
            Value factor = (ctx.mode == ERadiance) ? inv_eta : Value(1.f);

            result[eval_t] =
                m_specular_transmittance->eval(si, eval_t) *
                abs(value * sqr(factor));
        }

        return result;
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3>
    MTS_INLINE
    Value pdf_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                   const Vector3 &wo, mask_t<Value> active) const {
        using Frame = Frame<Vector3>;
        using Mask  = mask_t<Value>;

        Value cos_theta_i = Frame::cos_theta(si.wi),
              cos_theta_o = Frame::cos_theta(wo);

        /* Ignore perfectly grazing configurations */
        active &= neq(cos_theta_i, 0.f);

        // Determine the type of interaction
        bool has_reflection   = ctx.is_enabled(EGlossyReflection, 0),
             has_transmission = ctx.is_enabled(EGlossyTransmission, 1);

        Mask reflect = cos_theta_i * cos_theta_o > 0.f;
        active &= (Mask(has_reflection)   &&  reflect) ||
                  (Mask(has_transmission) && !reflect);

        /* Determine the relative index of refraction */
        Value eta = select(cos_theta_i > 0.f, Value(m_eta), Value(m_inv_eta));

        /* Compute the half-vector */
        Vector3 m = normalize(si.wi + wo * select(reflect, Value(1.f), eta));

        /* Ensure that the half-vector points into the
           same hemisphere as the macrosurface normal */
        m = mulsign(m, Frame::cos_theta(m));

        /* Jacobian of the half-direction mapping */
        Value dwh_dwo = select(reflect, rcp(4.f * dot(wo, m)),
                               (eta * eta * dot(wo, m)) /
                                   sqr(dot(si.wi, m) + eta * dot(wo, m)));

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
        if (unlikely(!m_sample_visible))
            sample_distr.scale_alpha(1.2f - .2f * sqrt(abs(Frame::cos_theta(si.wi))));

        // Evaluate the microfacet model sampling density function
        Value prob = sample_distr.pdf(mulsign(si.wi, Frame::cos_theta(si.wi)), m);

        if (likely(has_transmission && has_reflection)) {
            Value F = std::get<0>(fresnel(dot(si.wi, m), Value(m_eta)));
            prob *= select(reflect, F, 1.f - F);
        }

        return select(active, prob * abs(dwh_dwo), 0.f);
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
    ref<ContinuousSpectrum> m_specular_reflectance;
    ref<ContinuousSpectrum> m_specular_transmittance;
    EType m_type;
    Float m_alpha_u, m_alpha_v;
    Float m_eta, m_inv_eta;
    bool m_sample_visible;
};

MTS_IMPLEMENT_CLASS(RoughDielectric, BSDF)
MTS_EXPORT_PLUGIN(RoughDielectric, "Rough dielectric");

NAMESPACE_END(mitsuba)
