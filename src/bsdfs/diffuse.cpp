#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

class SmoothDiffuse final : public BSDF {
public:
    SmoothDiffuse(const Properties &props) {
        m_reflectance = props.spectrum("reflectance", 0.5f);
        m_flags = EDiffuseReflection | EFrontSide;
        m_components.push_back(m_flags);
    }

    template <typename SurfaceInteraction, typename Value, typename Point2,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Spectrum   = Spectrum<Value>>
    MTS_INLINE
    std::pair<BSDFSample, Spectrum> sample_impl(const BSDFContext &ctx,
                                                const SurfaceInteraction &si,
                                                Value /* sample1 */,
                                                const Point2 &sample2,
                                                mask_t<Value> active) const {
        using Frame = Frame<typename BSDFSample::Vector3>;

        Value cos_theta_i = Frame::cos_theta(si.wi);
        BSDFSample bs;

        active &= cos_theta_i > 0.f;
        if (unlikely(none(active) || !ctx.is_enabled(EDiffuseReflection)))
            return { bs, 0.f };

        bs.wo = warp::square_to_cosine_hemisphere(sample2);
        bs.pdf = warp::square_to_cosine_hemisphere_pdf(bs.wo);
        bs.eta = 1.f;
        bs.sampled_component = 0;
        bs.sampled_type = (uint32_t) EDiffuseReflection;

        Spectrum value = m_reflectance->eval(si, active);

        return { bs, select(active && bs.pdf > 0, value, 0.f) };
    }

    template <typename SurfaceInteraction, typename Vector3,
              typename Value    = typename SurfaceInteraction::Value,
              typename Spectrum = Spectrum<Value>>
    MTS_INLINE
    Spectrum eval_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                       const Vector3 &wo, mask_t<Value> active) const {
        using Frame = mitsuba::Frame<Vector3>;

        if (!ctx.is_enabled(EDiffuseReflection))
            return 0.f;

        Value cos_theta_i = Frame::cos_theta(si.wi),
              cos_theta_o = Frame::cos_theta(wo);

        Spectrum value = m_reflectance->eval(si, active) *
                         math::InvPi * cos_theta_o;

        return select(cos_theta_i > 0.f && cos_theta_o > 0.f, value, 0.f);
    }

    template <typename SurfaceInteraction, typename Vector3,
              typename Value = value_t<Vector3>>
    MTS_INLINE
    Value pdf_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                   const Vector3 &wo, mask_t<Value> /* active */) const {
        using Frame = Frame<Vector3>;
        using Frame = mitsuba::Frame<Vector3>;

        if (!ctx.is_enabled(EDiffuseReflection))
            return 0.f;

        Value cos_theta_i = Frame::cos_theta(si.wi),
              cos_theta_o = Frame::cos_theta(wo);

        Value pdf = warp::square_to_cosine_hemisphere_pdf(wo);

        return select(cos_theta_i > 0.f && cos_theta_o > 0.f, pdf, 0.f);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothDiffuse[" << std::endl
            << "  reflectance = " << string::indent(m_reflectance) << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_BSDF()
    MTS_DECLARE_CLASS()
private:
    ref<ContinuousSpectrum> m_reflectance;
};

MTS_IMPLEMENT_CLASS(SmoothDiffuse, BSDF)
MTS_EXPORT_PLUGIN(SmoothDiffuse, "Smooth diffuse material")

NAMESPACE_END(mitsuba)
