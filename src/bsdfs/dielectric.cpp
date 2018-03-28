#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/reflection.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

/// Smooth dielectric material.
class SmoothDielectric final : public BSDF {
public:
    SmoothDielectric(const Properties &props) {
        m_flags = EDeltaReflection | EFrontSide | EBackSide
                  | EDeltaTransmission | EFrontSide | EBackSide | ENonSymmetric;

        // Specifies the internal index of refraction at the interface
        Float int_ior = lookup_ior(props, "int_ior", "bk7");

        // Specifies the external index of refraction at the interface
        Float ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0 || ext_ior < 0)
            Throw("The interior and exterior indices of refraction must"
                  " be positive!");

        m_eta = int_ior / ext_ior;
        m_inv_eta = rcp(m_eta);

        m_specular_reflectance   = props.spectrum("specular_reflectance", 1.f);
        m_specular_transmittance = props.spectrum("specular_transmittance", 1.f);

        m_components.push_back(EDeltaReflection | EFrontSide | EBackSide);
        m_components.push_back(EDeltaTransmission | EFrontSide | EBackSide
                               | ENonSymmetric);

        m_needs_differentials = false;
    }

    template <typename SurfaceInteraction,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Value      = typename SurfaceInteraction::Value,
              typename Point2     = typename SurfaceInteraction::Point2,
              typename Spectrum   = Spectrum<Value>>
    std::pair<BSDFSample, Spectrum> sample_impl(
            const SurfaceInteraction &si, const BSDFContext &ctx, Value sample1,
            const Point2 &/*sample2*/, const mask_t<Value> &active) const {
        using Index   = typename BSDFSample::Index;
        using Vector3 = typename BSDFSample::Vector3;
        using Mask    = mask_t<Value>;

        Mask sample_reflection   = active && Mask(ctx.is_enabled(EDeltaReflection, 0));
        Mask sample_transmission = active && Mask(ctx.is_enabled(EDeltaTransmission, 1));
        Mask sample_any = sample_reflection || sample_transmission;

        BSDFSample bs;
        if (none(sample_any))
            return { bs, 0.0f };

        Value cos_theta_i = Frame<Vector3>::cos_theta(si.wi);
        Value F, cos_theta_t;
        std::tie(F, cos_theta_t) = fresnel_dielectric_ext(cos_theta_i, Value(m_eta));

        Value threshold = select(sample_transmission && sample_reflection, F, 1.0f);
        Mask selected_reflection = sample_reflection && (sample1 <= threshold);

        bs.sampled_component = select(selected_reflection, Index(0), Index(1));
        bs.sampled_type      = select(selected_reflection,
                                      Index(EDeltaReflection),
                                      Index(EDeltaTransmission));
        bs.wi                = si.wi;
        bs.wo                = select(selected_reflection,
                                      reflect(si.wi),
                                      refract(si.wi, Value(m_eta), cos_theta_t));
        bs.eta               = select(selected_reflection,
                                      Value(1.0f),
                                      select(cos_theta_t < 0,
                                             Value(m_eta), Value(m_inv_eta))
                               );
        bs.pdf               = 1.0f;
        masked(bs.pdf, sample_transmission &&  selected_reflection) = F;
        masked(bs.pdf, sample_reflection   && !selected_reflection) = 1.0f - F;

        /* For transmission, radiance must be scaled to account for the solid
           angle compression that occurs when crossing the interface. */
        Value factor = (ctx.mode == ERadiance)
            ? select(cos_theta_t < 0.0f, Value(m_inv_eta), Value(m_eta)) : 1.0f;

        Spectrum radiance = bs.pdf * select(selected_reflection,
            m_specular_reflectance->eval(si.wavelengths, selected_reflection),
            (factor * factor)
            * m_specular_transmittance->eval(si.wavelengths, !selected_reflection)
        );

        return { bs, radiance };
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3,
              typename Spectrum = Spectrum<Value>>
        Spectrum eval_impl(const SurfaceInteraction &si, const BSDFContext &ctx,
                           const Vector3 &wo, mask_t<Value> active) const {
        using Frame = Frame<Vector3>;
        using Mask  = mask_t<Value>;

        Mask sample_reflection   = ctx.is_enabled(EDeltaReflection, 0);
        Mask sample_transmission = ctx.is_enabled(EDeltaTransmission, 1);

        Value cos_theta_i = Frame::cos_theta(si.wi);
        Value F, cos_theta_t;
        std::tie(F, cos_theta_t) = fresnel_dielectric_ext(cos_theta_i, Value(m_eta));

        Mask is_reflection = cos_theta_i * Frame::cos_theta(wo) >= 0;
        Mask has_reflection = sample_reflection
            && (abs(dot(reflect(si.wi), wo) - 1.0f) > math::DeltaEpsilon);
        Mask has_transmission = sample_transmission
            && (abs(dot(refract(si.wi, Value(m_eta), cos_theta_t), wo)
                    - 1.0f) > math::DeltaEpsilon);
        active = active && ((is_reflection && has_reflection)
                            || (!is_reflection && has_transmission));

        Spectrum result = 1.0f;
        masked(result, !active) = Spectrum(0.0f);
        masked(result, active && is_reflection && sample_transmission) =
            m_specular_reflectance->eval(si.wavelengths) * F;
        /* Radiance must be scaled to account for the solid angle compression
           that occurs when crossing the interface. */
        Value factor = ctx.mode == ERadiance
            ? select(cos_theta_t < 0.0f, Value(m_inv_eta), Value(m_eta)) : 1.0f;
        masked(result, active && !is_reflection && sample_reflection) =
            m_specular_transmittance->eval(si.wavelengths)
            * factor * factor * (1.0f - F);

        return result;
    }

    template <typename SurfaceInteraction,
              typename Value   = typename SurfaceInteraction::Value,
              typename Vector3 = typename SurfaceInteraction::Vector3>
    Value pdf_impl(const SurfaceInteraction &si, const BSDFContext &ctx,
                   const Vector3 &wo, mask_t<Value> active) const {
        using Frame = Frame<Vector3>;
        using Mask  = mask_t<Value>;

        Mask sample_reflection   = ctx.is_enabled(EDeltaReflection, 0);
        Mask sample_transmission = ctx.is_enabled(EDeltaTransmission, 1);

        Value cos_theta_i = Frame::cos_theta(si.wi);
        Value F, cos_theta_t;
        std::tie(F, cos_theta_t) = fresnel_dielectric_ext(cos_theta_i, Value(m_eta));

        Mask is_reflection = cos_theta_i * Frame::cos_theta(wo) >= 0;
        Mask has_reflection = sample_reflection
            && (abs(dot(reflect(si.wi), wo) - 1.0f) > math::DeltaEpsilon);
        Mask has_transmission = sample_transmission
            && (abs(dot(refract(si.wi, Value(m_eta), cos_theta_t), wo)
                    - 1.0f) > math::DeltaEpsilon);
        active = active && ((is_reflection && has_reflection)
                            || (!is_reflection && has_transmission));

        Value result = 1.0f;
        masked(result, !active) = 0.0f;
        masked(result, active &&  is_reflection && sample_transmission) = F;
        masked(result, active && !is_reflection && sample_reflection) = 1.0f - F;

        return result;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothDielectric[" << std::endl
            << "  eta = " << m_eta << "," << std::endl
            << "  specular_reflectance = " << string::indent(m_specular_reflectance) << "," << std::endl
            << "  specular_transmittance = " << string::indent(m_specular_transmittance) << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_BSDF()
    MTS_DECLARE_CLASS()
private:
    Float m_eta, m_inv_eta;
    ref<ContinuousSpectrum> m_specular_reflectance, m_specular_transmittance;
};

MTS_IMPLEMENT_CLASS(SmoothDielectric, BSDF)
MTS_EXPORT_PLUGIN(SmoothDielectric, "Smooth dielectric BSDF")

NAMESPACE_END(mitsuba)
