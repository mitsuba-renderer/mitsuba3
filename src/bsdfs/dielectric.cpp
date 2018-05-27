#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/reflection.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/ior.h>

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

        m_specular_reflectance   = props.spectrum("specular_reflectance", 1.f);
        m_specular_transmittance = props.spectrum("specular_transmittance", 1.f);

        m_components.push_back(EDeltaReflection | EFrontSide | EBackSide);
        m_components.push_back(EDeltaTransmission | EFrontSide | EBackSide
                               | ENonSymmetric);
    }

    template <typename SurfaceInteraction, typename Value, typename Point2,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Spectrum   = Spectrum<Value>>
    MTS_INLINE
    std::pair<BSDFSample, Spectrum> sample_impl(const BSDFContext &ctx,
                                                const SurfaceInteraction &si,
                                                Value sample1,
                                                const Point2 &/* sample2 */,
                                                mask_t<Value> active) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Index   = typename BSDFSample::Index;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        Mask sample_reflection   = ctx.is_enabled(EDeltaReflection, 0);
        Mask sample_transmission = ctx.is_enabled(EDeltaTransmission, 1);

        BSDFSample bs;
        Spectrum spec(0.f);

        /* Evaluate the Fresnel equations for unpolarized illumination */
        Value cos_theta_i = Frame::cos_theta(si.wi);

        Value F, cos_theta_t, eta_it, eta_ti;
        std::tie(F, cos_theta_t, eta_it, eta_ti) = fresnel(cos_theta_i, Value(m_eta));

        /* Lobe selection */
        Mask selected_reflection = Mask(sample_reflection) && (!Mask(sample_transmission) || sample1 < F),
             selected_transmission = Mask(sample_transmission) && !selected_reflection;
        selected_reflection &= active;
        selected_transmission &= active;

        bs.sampled_component = select(selected_reflection, Index(0), Index(1));
        bs.sampled_type      = select(selected_reflection,
                                      Index(EDeltaReflection),
                                      Index(EDeltaTransmission));

        bs.wo                = select(selected_reflection,
                                      reflect(si.wi),
                                      refract(si.wi, cos_theta_t, eta_ti));

        bs.eta               = select(selected_reflection, Value(1.f), eta_it);

        bs.pdf               = 1.f;


        if (any(selected_reflection)) {
            masked(bs.pdf, selected_reflection && sample_transmission) = F;

            masked(spec, selected_reflection) =
                m_specular_reflectance->eval(si,
                                             selected_reflection) *
                select(sample_transmission, 1.f, F);
        }

        if (any(selected_transmission)) {
            /* For transmission, radiance must be scaled to account for the solid
               angle compression that occurs when crossing the interface. */
            Value factor = (ctx.mode == ERadiance) ? eta_ti : Value(1.f);

            masked(bs.pdf, selected_transmission && sample_reflection) = 1.f - F;

            masked(spec, selected_transmission) =
                m_specular_transmittance->eval(si,
                                               selected_transmission) *
                factor * factor * select(sample_reflection, 1.f, 1.f - F);
        }

        return { bs, spec };
    }

    template <typename SurfaceInteraction, typename Vector3,
              typename Value    = typename SurfaceInteraction::Value,
              typename Spectrum = Spectrum<Value>>
    MTS_INLINE
    Spectrum eval_impl(const BSDFContext &/* ctx */, const SurfaceInteraction &/* si */,
                       const Vector3 &/* wo */, mask_t<Value> /* active */) const {
        return 0.f;
    }

    template <typename SurfaceInteraction, typename Vector3,
              typename Value = value_t<Vector3>>
    MTS_INLINE
    Value pdf_impl(const BSDFContext &/* ctx */, const SurfaceInteraction & /* si */,
                   const Vector3 &/* wo */, mask_t<Value> /* active */) const {
        return 0.f;
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
    Float m_eta;
    ref<ContinuousSpectrum> m_specular_reflectance;
    ref<ContinuousSpectrum> m_specular_transmittance;
};

MTS_IMPLEMENT_CLASS(SmoothDielectric, BSDF)
MTS_EXPORT_PLUGIN(SmoothDielectric, "Smooth dielectric")

NAMESPACE_END(mitsuba)
