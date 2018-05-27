#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/reflection.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

class ThinDielectric : public BSDF {
public:
    ThinDielectric(const Properties &props) {
        // Specifies the internal index of refraction at the interface
        Float int_ior = lookup_ior(props, "int_ior", "bk7");
        // Specifies the external index of refraction at the interface
        Float ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0.f || ext_ior < 0.f)
            Throw("The interior and exterior indices of "
                  "refraction must be positive!");

        m_eta = int_ior / ext_ior;

        m_specular_reflectance   = props.spectrum("specular_reflectance",   1.f);
        m_specular_transmittance = props.spectrum("specular_transmittance", 1.f);

        m_flags = ENull | EDeltaReflection | EFrontSide | EBackSide;
        m_components.push_back(EDeltaReflection | EFrontSide | EBackSide);
        m_components.push_back(ENull | EFrontSide | EBackSide);
    }

    template <typename SurfaceInteraction,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Value      = typename SurfaceInteraction::Value,
              typename Point2     = typename SurfaceInteraction::Point2,
              typename Spectrum   = Spectrum<Value>>
    MTS_INLINE
    std::pair<BSDFSample, Spectrum> sample_impl(
            const BSDFContext &ctx, const SurfaceInteraction &si,
            Value sample1, const Point2 &/*sample2*/, mask_t<Value> active) const {
        using Index   = typename BSDFSample::Index;
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        bool sample_reflection   = ctx.is_enabled(EDeltaReflection, 0);
        bool sample_transmission = ctx.is_enabled(ENull, 1);

        Value r = std::get<0>(fresnel(abs(Frame::cos_theta(si.wi)), Value(m_eta)));

        // Account for internal reflections: r' = r + trt + tr^3t + ..
        r *= 2.f / (1.f + r);

        Mask selected_r = active && Mask(sample_reflection) &&
                          (!Mask(sample_transmission) || sample1 < r),
             selected_t = active && Mask(sample_transmission) && !selected_r;

        BSDFSample bs;
        Spectrum value(0.f);

        bs.wo = select(selected_r, reflect(si.wi), -si.wi);
        bs.pdf = select(selected_r, r, 1.f - r);
        bs.eta = 1.f;
        bs.sampled_component = select(selected_r, Index(0), Index(1));
        bs.sampled_type = select(selected_r, Index(EDeltaReflection), Index(ENull));

        if (any(selected_r))
            value[selected_r] = m_specular_reflectance->eval(si, selected_r);

        if (any(selected_t))
            value[selected_t] = m_specular_reflectance->eval(si, selected_t);

        return { bs, value };
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
        oss << "ThinDielectric[" << std::endl
            << "  eta = "                    << string::indent(m_eta)                    << "," << std::endl
            << "  specular_reflectance = "   << string::indent(m_specular_reflectance)   << "," << std::endl
            << "  specular_transmittance = " << string::indent(m_specular_transmittance) << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_BSDF()
    MTS_DECLARE_CLASS()
private:
    Float m_eta;
    ref<ContinuousSpectrum> m_specular_transmittance;
    ref<ContinuousSpectrum> m_specular_reflectance;
};

MTS_IMPLEMENT_CLASS(ThinDielectric, BSDF)
MTS_EXPORT_PLUGIN(ThinDielectric, "Thin dielectric")

NAMESPACE_END(mitsuba)
