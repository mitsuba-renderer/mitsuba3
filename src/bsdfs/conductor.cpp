#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

class SmoothConductor final : public BSDF {
public:
    SmoothConductor(const Properties &props) : BSDF(props) {
        m_flags = EDeltaReflection | EFrontSide;
        m_components.push_back(EDeltaReflection | EFrontSide);

        m_specular_reflectance = props.spectrum("specular_reflectance", 1.f);

        m_eta = props.spectrum("eta", 0.f);
        m_k   = props.spectrum("k",   1.f);
    }

    template <typename SurfaceInteraction,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Value      = typename SurfaceInteraction::Value,
              typename Point2     = typename SurfaceInteraction::Point2,
              typename Spectrum   = Spectrum<Value>>
    MTS_INLINE std::pair<BSDFSample, Spectrum>
    sample_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                Value /* sample1 */, const Point2 & /* sample2 */,
                mask_t<Value> active) const {
        using Frame = Frame<typename BSDFSample::Vector3>;

        Value cos_theta_i = Frame::cos_theta(si.wi);
        active &= cos_theta_i > 0.f;

        BSDFSample bs;
        Spectrum value(0.f);
        if (unlikely(none_or<false>(active) || !ctx.is_enabled(EDeltaReflection)))
            return { bs, value };

        bs.sampled_component = 0;
        bs.sampled_type = EDeltaReflection;
        bs.wo  = reflect(si.wi);
        bs.eta = 1.f;
        bs.pdf = 1.f;

        Complex<Spectrum> eta(m_eta->eval(si, active),
                              m_k  ->eval(si, active));

        value = m_specular_reflectance->eval(si, active) *
            fresnel_conductor(Spectrum(cos_theta_i), eta);

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

    std::vector<ref<Object>> children() override {
        return { m_specular_reflectance.get(), m_eta.get(), m_k.get() };
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothConductor[" << std::endl
            << "  eta = " << string::indent(m_eta) << "," << std::endl
            << "  k = "   << string::indent(m_k)   << "," << std::endl
            << "  specular_reflectance = " << string::indent(m_specular_reflectance) << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_BSDF_ALL()
    MTS_DECLARE_CLASS()
private:
    ref<ContinuousSpectrum> m_specular_reflectance;
    ref<ContinuousSpectrum> m_eta, m_k;
};

MTS_IMPLEMENT_CLASS(SmoothConductor, BSDF)
MTS_EXPORT_PLUGIN(SmoothConductor, "Smooth conductor")

NAMESPACE_END(mitsuba)
