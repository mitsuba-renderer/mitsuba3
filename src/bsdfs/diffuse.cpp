#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>

NAMESPACE_BEGIN(mitsuba)

class SmoothDiffuse final : public BSDF {
public:
    SmoothDiffuse(const Properties &props) {
        m_reflectance = props.spectrum("reflectance", 0.5f);

        m_flags = (EDiffuseReflection | EFrontSide);
        m_components.push_back(m_flags);
        m_needs_differentials = false;
    }

    template <typename SurfaceInteraction,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Value      = typename SurfaceInteraction::Value,
              typename Point2     = typename SurfaceInteraction::Point2,
              typename Spectrum   = Spectrum<Value>>
    std::pair<BSDFSample, Spectrum> sample_impl(
            const SurfaceInteraction &si, const BSDFContext &ctx,
            Value /*sample1*/, const Point2 &sample2,
            const mask_t<Value> &active) const {
        using Index   = typename BSDFSample::Index;
        using Vector3 = typename BSDFSample::Vector3;

        Value n_dot_wi = Frame<Vector3>::cos_theta(si.wi);
        mask_t<Value> above_horizon = n_dot_wi > 0.0f;

        BSDFSample bs;
        if (none(above_horizon && active) || !ctx.is_enabled(EDiffuseReflection))
            return { bs, 0.0f };
        bs.wi = si.wi;
        bs.wo = warp::square_to_cosine_hemisphere(sample2);
        bs.eta = Value(1.0f);
        bs.sampled_component = Index(0);
        bs.sampled_type = Index(EDiffuseReflection);
        bs.pdf = warp::square_to_cosine_hemisphere_pdf(bs.wo);

        Spectrum value = select(above_horizon,
                                m_reflectance->eval(si.wavelengths, active),
                                Spectrum(0.0f));
        return { bs, value };
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const SurfaceInteraction &si, const BSDFContext &ctx,
                       const Vector3 &wo, const mask_t<Value> &active) const {
        using Frame = Frame<Vector3>;

        if (!ctx.is_enabled(EDiffuseReflection))
            return 0.0f;

        Value n_dot_wi = Frame::cos_theta(si.wi);
        Value n_dot_wo = Frame::cos_theta(wo);

        return select((n_dot_wi > 0.0f) && (n_dot_wo > 0.0f),
            m_reflectance->eval(si.wavelengths, active) * math::InvPi * n_dot_wo,
            Spectrum(0.0f)
        );
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3>
    Value pdf_impl(const SurfaceInteraction &si, const BSDFContext &ctx, const Vector3 &wo,
                   const mask_t<Value> & /*active*/) const {
        using Frame = Frame<Vector3>;

        if (!ctx.is_enabled(EDiffuseReflection))
            return 0.0f;

        mask_t<Value> above_horizon = (Frame::cos_theta(si.wi) > 0.0f) &&
                                      (Frame::cos_theta(wo) > 0.0f);
        return select(above_horizon,
                      warp::square_to_cosine_hemisphere_pdf(wo),
                      Value(0.f));
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
MTS_EXPORT_PLUGIN(SmoothDiffuse, "Smooth diffuse BRDF")

NAMESPACE_END(mitsuba)
