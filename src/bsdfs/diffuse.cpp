#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>
#include <mitsuba/core/warp.h>

NAMESPACE_BEGIN(mitsuba)

class SmoothDiffuse final : public BSDF {
public:
    SmoothDiffuse(const Properties &props) {
        m_flags = EDiffuseReflection | EFrontSide;
        m_reflectance = props.spectrumf("reflectance", Spectrumf(1.0f));
    }

    template <typename BSDFSample,
              typename Value  = typename BSDFSample::Value,
              typename Point2 = typename BSDFSample::Point2,
              typename Spectrum = Spectrum<Value>>
    std::pair<Spectrum, Value> sample_impl(BSDFSample &bs,
                                           const Point2 &sample,
                                           const mask_t<Value> &active_) const {
        using Index   = typename BSDFSample::Index;
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        Mask active(active_);

        if (!(bs.type_mask & EDiffuseReflection))
            return { 0.0f, 0.0f };

        Mask below_horizon = (Frame::cos_theta(bs.wi) <= 0.0f);
        active &= ~below_horizon;

        if (none(active))
            return { 0.0f, 0.0f };

        masked(bs.wo,  active) = warp::square_to_cosine_hemisphere(sample);
        masked(bs.eta, active) = Value(1.0f);
        masked(bs.sampled_component, active) = Index(0);
        masked(bs.sampled_type, active) = Index(EDiffuseReflection);

        Value pdf(0.0f);
        masked(pdf, active) = warp::square_to_cosine_hemisphere_pdf(bs.wo);
        Spectrum value(0.0f);
        masked(value, active) = Spectrum(m_reflectance);

        return { value, pdf };
    }

    template <typename BSDFSample,
              typename Value = typename BSDFSample::Value,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const BSDFSample &bs,
                       EMeasure measure,
                       const mask_t<Value> &/*active*/) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        if (!(bs.type_mask & EDiffuseReflection) || measure != ESolidAngle)
            return 0.0f;

        Value n_dot_wi = Frame::cos_theta(bs.wi);
        Value n_dot_wo = Frame::cos_theta(bs.wo);

        Mask active = !((n_dot_wi <= 0.0f) | (n_dot_wo <= 0.0f));

        Spectrum result(0.0f);
        masked(result, active) = (math::InvPi * n_dot_wo) * Spectrum(m_reflectance);

        return result;
    }

    template <typename BSDFSample,
              typename Value = typename BSDFSample::Value>
    Value pdf_impl(const BSDFSample &bs,
                   EMeasure measure,
                   const mask_t<Value> &active_) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        if (!(bs.type_mask & EDiffuseReflection) || measure != ESolidAngle)
            return 0.0f;

        Mask active(active_);

        Value n_dot_wi = Frame::cos_theta(bs.wi);
        Value n_dot_wo = Frame::cos_theta(bs.wo);

        active &= !((n_dot_wi <= 0.0f) | (n_dot_wo <= 0.0f));

        if (none(active))
            return 0.0f;

        Value result(0.0f);
        masked(result, active) = warp::square_to_cosine_hemisphere_pdf(bs.wo);

        return result;
    }

    MTS_IMPLEMENT_BSDF()

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothDiffuse[" << std::endl
            << "  reflectance = " << string::indent(m_reflectance) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    Spectrumf m_reflectance;
};

MTS_IMPLEMENT_CLASS(SmoothDiffuse, BSDF)
MTS_EXPORT_PLUGIN(SmoothDiffuse, "Smooth diffuse BRDF")

NAMESPACE_END(mitsuba)
