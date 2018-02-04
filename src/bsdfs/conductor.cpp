#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/reflection.h>

NAMESPACE_BEGIN(mitsuba)

class SmoothConductor : public BSDF {
public:
    SmoothConductor(const Properties &props) {
        m_flags = EDeltaReflection | EFrontSide;
        m_components.clear();
        m_components.push_back(EDeltaReflection | EFrontSide);

        m_specular_reflectance = props.spectrum("specular_reflectance", 1.0f);

        //Float ext_eta = lookup_ior(props, "ext_eta", "air");

        m_eta = props.spectrum("eta", 0.f); // TODO / ext_eta;
        m_k   = props.spectrum("k",   1.f); // TODO / ext_eta;
    }

    template <typename SurfaceInteraction,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Value      = typename SurfaceInteraction::Value,
              typename Point2     = typename SurfaceInteraction::Point2,
              typename Spectrum   = Spectrum<Value>>
    std::pair<BSDFSample, Spectrum> sample_impl(
            const BSDFContext &ctx, const SurfaceInteraction &si,
            Value /*sample1*/, const Point2 &/*sample2*/, mask_t<Value> active) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;

        Value n_dot_wi = Frame::cos_theta(si.wi);
        active = active && (n_dot_wi > 0.0f);

        BSDFSample bs;
        if (!ctx.is_enabled(EDeltaReflection) || none(active))
            return { bs, 0.0f };

        bs.sampled_component = 0;
        bs.sampled_type = EDeltaReflection;
        bs.wo  = reflect(si.wi);
        bs.eta = 1.0f;
        bs.pdf = 1.0f;

        Spectrum value(0.0f);
        masked(value, active) =
            m_specular_reflectance->eval(si.wavelengths, active) *
            fresnel_complex(
                n_dot_wi,
                Complex<Value>(luminance(m_eta->eval(si.wavelengths, active),
                                         si.wavelengths, active),
                               luminance(m_k->eval(si.wavelengths, active),
                                         si.wavelengths, active)));
        return { bs, value };
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                       const Vector3 &wo, mask_t<Value> active) const {
        using Frame = Frame<Vector3>;

        bool sample_reflection = ctx.is_enabled(EDeltaReflection);
        if (!sample_reflection)
            return Spectrum(0.0f);

        Value n_dot_wi = Frame::cos_theta(si.wi);
        Value n_dot_wo = Frame::cos_theta(wo);
        /* Verify that the provided direction pair matches an ideal
           specular reflection; tolerate some roundoff errors */
        active = active && (n_dot_wi > 0.0f) && (n_dot_wo > 0.0f)
                 && !(abs(dot(reflect(si.wi), wo) - 1.0f) > math::DeltaEpsilon);

        if (none(active))
            return Spectrum(0.0f);

        return select(
            active,
            m_specular_reflectance->eval(si.wavelengths, active) *
                fresnel_complex(
                    n_dot_wi, Complex<Value>(
                                  luminance(m_eta->eval(si.wavelengths, active),
                                            si.wavelengths, active),
                                  luminance(m_k->eval(si.wavelengths, active),
                                            si.wavelengths, active))),
            Spectrum(0.0f));
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3>
    Value pdf_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                   const Vector3 &wo, mask_t<Value> active) const {
        using Frame = Frame<Vector3>;

        bool sample_reflection = ctx.is_enabled(EDeltaReflection);
        if (!sample_reflection)
            return 0.0f;

        Value n_dot_wi = Frame::cos_theta(si.wi);
        Value n_dot_wo = Frame::cos_theta(wo);
        /* Verify that the provided direction pair matches an ideal
           specular reflection; tolerate some roundoff errors */
        active = active && (n_dot_wi > 0.0f) && (n_dot_wo > 0.0f)
                 && !(abs(dot(reflect(si.wi), wo) - 1.0f) > math::DeltaEpsilon);

        return select(active, Value(1.0f), Value(0.0f));
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothConductor[" << std::endl
            << "  eta = " << m_eta << "," << std::endl
            << "  k = "   << m_k   << "," << std::endl
            << "  specular_reflectance = " << m_specular_reflectance << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_BSDF()
    MTS_DECLARE_CLASS()
private:
    ref<ContinuousSpectrum> m_specular_reflectance;
    ref<ContinuousSpectrum> m_eta, m_k;
};

MTS_IMPLEMENT_CLASS(SmoothConductor, BSDF)
MTS_EXPORT_PLUGIN(SmoothConductor, "Smooth conductor")

NAMESPACE_END(mitsuba)
