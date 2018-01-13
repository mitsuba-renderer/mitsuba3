#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>

NAMESPACE_BEGIN(mitsuba)

class SmoothDiffuse final : public BSDF {
public:
    SmoothDiffuse(const Properties &props) {
        m_flags = (EDiffuseReflection | EFrontSide);
        m_reflectance = props.spectrum("reflectance", 0.5f);
    }

    template <typename BSDFSample, typename Value = typename BSDFSample::Value,
              typename Point2   = typename BSDFSample::Point2,
              typename Spectrum = Spectrum<Value>>
    std::pair<Spectrum, Value> sample_impl(BSDFSample &bs,
                                           const Point2 &sample,
                                           const mask_t<Value> &active) const {
        using Index   = typename BSDFSample::Index;
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        Value n_dot_wi = Frame::cos_theta(bs.wi);
        Mask above_horizon = (n_dot_wi > 0.0f);

        if (none(above_horizon && active) || !(bs.type_mask & EDiffuseReflection))
            return { 0.0f, 0.0f };

        masked(bs.wo,  active) = warp::square_to_cosine_hemisphere(sample);
        masked(bs.eta, active) = Value(1.0f);
        masked(bs.sampled_component, active) = Index(0);
        masked(bs.sampled_type, active) = Index(EDiffuseReflection);

        Value pdf = select(above_horizon,
                           warp::square_to_cosine_hemisphere_pdf(bs.wo),
                           Value(0.0f));

        Spectrum value = select(above_horizon,
                                m_reflectance->eval(bs.si.wavelengths, active),
                                Spectrum(0.0f));

        return { value, pdf };
    }

    template <typename BSDFSample, typename Value = typename BSDFSample::Value,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const BSDFSample &bs, EMeasure measure,
                       const mask_t<Value> &active) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;

        if (unlikely(!(bs.type_mask & EDiffuseReflection) || measure != ESolidAngle))
            return 0.0f;

        Value n_dot_wi = Frame::cos_theta(bs.wi);
        Value n_dot_wo = Frame::cos_theta(bs.wo);

        return select((n_dot_wi > 0.0f) && (n_dot_wo > 0.0f),
                      m_reflectance->eval(bs.si.wavelengths, active) * math::InvPi * n_dot_wo,
                      Spectrum(0.0f));
    }

    template <typename BSDFSample, typename Value = typename BSDFSample::Value>
    Value pdf_impl(const BSDFSample &bs, EMeasure measure,
                  const mask_t<Value> & /* active */) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        if (unlikely(!(bs.type_mask & EDiffuseReflection) || measure != ESolidAngle))
            return 0.0f;

        Mask above_horizon = (Frame::cos_theta(bs.wi) > 0.0f) &&
                             (Frame::cos_theta(bs.wo) > 0.0f);

        return select(above_horizon,
                      warp::square_to_cosine_hemisphere_pdf(bs.wo),
                      Value(0.0f));
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
