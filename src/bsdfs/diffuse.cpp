#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/records.h>
#include <mitsuba/core/warp.h>

NAMESPACE_BEGIN(mitsuba)

class SmoothDiffuse final : public BSDF {
public:
    SmoothDiffuse(const Properties &props) {
        m_flags = EDiffuseReflection | EFrontSide;
        m_reflectance = props.spectrumf("radiance", Spectrumf(1.f));
    }

    template <typename BSDFSample,
              typename Value  = typename BSDFSample::Value,
              typename Point2 = typename BSDFSample::Point2,
              typename Spectrum = Spectrum<Value>>
    std::pair<Spectrum, Value> sample_impl(BSDFSample &bs,
                                           const Point2 &sample,
                                           const mask_t<Value> &active) const {
        using Index   = typename BSDFSample::Index;
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;

        if (!(bs.type_mask & EDiffuseReflection))
            return { 0.f, 0.f };

        Value n_dot_wi = Frame::cos_theta(bs.wi);
        auto below_horizon = (n_dot_wi <= 0.f | !active);

        if (all(below_horizon))
            return { 0.f, 0.f };

        masked(bs.wo,  active) = warp::square_to_cosine_hemisphere(sample);
        masked(bs.eta, active) = Value(1.f);
        masked(bs.sampled_component, active) = Index(0);
        masked(bs.sampled_type, active) = Index(EDiffuseReflection);

        Value pdf = select(below_horizon,
                           Value(0.f),
                           warp::square_to_cosine_hemisphere_pdf(bs.wo));
        Spectrum value = select(below_horizon, Spectrum(0.f), Spectrum(m_reflectance));
        return { value, pdf };
    }

    template <typename BSDFSample,
              typename Value = typename BSDFSample::Value,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const BSDFSample &bs,
                       EMeasure measure,
                       const mask_t<Value> &/* active */) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;
        if (!(bs.type_mask & EDiffuseReflection) || measure != ESolidAngle)
            return Spectrum(0.f);

        Value n_dot_wi = Frame::cos_theta(bs.wi);
        Value n_dot_wo = Frame::cos_theta(bs.wo);

        return select((n_dot_wi <= 0.f) | (n_dot_wo <= 0.f),
                      Spectrum(0.f),
                      Spectrum(m_reflectance))
               * (math::InvPi * n_dot_wo);
    }

    template <typename BSDFSample,
              typename Value = typename BSDFSample::Value>
    auto pdf_impl(const BSDFSample &bs,
                  EMeasure measure,
                  const mask_t<Value> &active) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;
        if (!(bs.type_mask & EDiffuseReflection) || measure != ESolidAngle)
            return Value(0.f);

        Value n_dot_wi = Frame::cos_theta(bs.wi);
        Value n_dot_wo = Frame::cos_theta(bs.wo);

        auto wi_below_horizon = (n_dot_wi <= 0.f | !active);
        auto wo_below_horizon = (n_dot_wo <= 0.f | !active);

        if (all(wi_below_horizon) && all(wo_below_horizon))
            return Value(0.f);

        return select(wi_below_horizon | wo_below_horizon,
                      Value(0.f),
                      warp::square_to_cosine_hemisphere_pdf(bs.wo));
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
