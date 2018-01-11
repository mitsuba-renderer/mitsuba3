#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/bsdfutils.h>
#include <mitsuba/render/ior.h>

NAMESPACE_BEGIN(mitsuba)

class SmoothConductor : public BSDF {
public:
    SmoothConductor(const Properties &props) {
        m_flags = EDeltaReflection | EFrontSide;
        m_specular_reflectance = props.spectrum("specular_reflectance", 1.0f);

        //Float ext_eta = lookup_ior(props, "ext_eta", "air");

        m_eta = props.spectrum("eta", 0.f); // TODO / ext_eta;
        m_k   = props.spectrum("k",   1.f); // TODO / ext_eta;

        m_needs_differentials = false;
    }

    template <typename BSDFContext,
              typename BSDFSample = BSDFSample<typename BSDFContext::Point3>,
              typename Value      = typename BSDFContext::Value,
              typename Point2     = typename BSDFContext::Point2,
              typename Spectrum   = Spectrum<Value>>
    std::pair<BSDFSample, Spectrum> sample_impl(const BSDFContext &ctx,
                                                Value /*sample1*/,
                                                const Point2 &/*sample2*/,
                                                const mask_t<Value> &active_) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        BSDFSample bs;

        if ((ctx.component != (uint32_t) -1 && ctx.component != 0)
            || !(ctx.type_mask & EDeltaReflection))
            return { bs, 0.0f };

        Mask active(active_);

        Value n_dot_wi = Frame::cos_theta(ctx.si.wi);
        active = active && (n_dot_wi > 0.0f);

        if (none(active))
            return { bs, 0.0f };

        bs.sampled_component = 0;
        bs.sampled_type = EDeltaReflection;
        bs.wo = reflect(ctx.si.wi);
        bs.eta = 1.0f;

        masked(bs.pdf, active) = 1.0f;

        Spectrum value(0.0f);
        masked(value, active)
            = m_specular_reflectance->eval(ctx.si.wavelengths, active)
              * fresnel_conductor_exact(n_dot_wi,
                                        luminance(m_eta->eval(ctx.si.wavelengths, active),
                                                  ctx.si.wavelengths, active),
                                        luminance(m_k->eval(ctx.si.wavelengths, active),
                                                  ctx.si.wavelengths, active));
        return { bs, value };
    }

    template <typename BSDFContext,
              typename Value    = typename BSDFContext::Value,
              typename Vector3  = typename BSDFContext::Vector3,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const BSDFContext &ctx, const Vector3 &wo,
                       const mask_t<Value> &active_) const {
        using Frame = Frame<Vector3>;

        bool sample_reflection = (ctx.component == (uint32_t) -1 || ctx.component == 0)
                                 && (ctx.type_mask & EDeltaReflection);

        if (!sample_reflection)
            return Spectrum(0.0f);

        mask_t<Value> active(active_);

        Value n_dot_wi = Frame::cos_theta(ctx.si.wi);
        Value n_dot_wo = Frame::cos_theta(wo);

        /* Verify that the provided direction pair matches an ideal
           specular reflection; tolerate some roundoff errors */
        active = active && (n_dot_wi > 0.0f) && (n_dot_wo > 0.0f)
                 && !(abs(dot(reflect(ctx.si.wi), wo) - 1.0f) > math::DeltaEpsilon);

        if (none(active))
            return Spectrum(0.0f);

        return select(active,
                      m_specular_reflectance->eval(ctx.si.wavelengths, active)
                      * fresnel_conductor_exact(
                            n_dot_wi,
                            luminance(m_eta->eval(ctx.si.wavelengths, active),
                                      ctx.si.wavelengths, active),
                            luminance(m_k->eval(ctx.si.wavelengths, active),
                                      ctx.si.wavelengths, active)),
                      Spectrum(0.0f));
    }

    template <typename BSDFContext,
              typename Value    = typename BSDFContext::Value,
              typename Vector3  = typename BSDFContext::Vector3>
    Value pdf_impl(const BSDFContext &ctx, const Vector3 &wo,
                   const mask_t<Value> &active_) const {
        using Frame = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        bool sample_reflection = (ctx.component == (uint32_t) -1 || ctx.component == 0)
                                 && (ctx.type_mask & EDeltaReflection);

        if (!sample_reflection)
            return 0.0f;

        Mask active(active_);

        Value n_dot_wi = Frame::cos_theta(ctx.si.wi);
        Value n_dot_wo = Frame::cos_theta(wo);

        /* Verify that the provided direction pair matches an ideal
           specular reflection; tolerate some roundoff errors */
        active = active && (n_dot_wi > 0.0f) && (n_dot_wo > 0.0f)
                 && !(abs(dot(reflect(ctx.si.wi), wo) - 1.0f) > math::DeltaEpsilon);

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
