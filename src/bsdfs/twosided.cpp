#include <mitsuba/core/string.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>

NAMESPACE_BEGIN(mitsuba)

class TwoSidedBRDF : public BSDF {
public:
    TwoSidedBRDF(const Properties &props) {
        auto bsdfs = props.objects();
        if (bsdfs.size() > 0)
            m_nested_brdf[0] = dynamic_cast<BSDF *>(bsdfs[0].second.get());
        if (bsdfs.size() > 1)
            m_nested_brdf[1] = dynamic_cast<BSDF *>(bsdfs[1].second.get());

        if (!m_nested_brdf[0])
            Throw("A nested one-sided material is required!");
        if (!m_nested_brdf[1])
            m_nested_brdf[1] = m_nested_brdf[0];

        m_flags = m_nested_brdf[0]->flags() | m_nested_brdf[1]->flags() | EFrontSide | EBackSide;

        if (m_flags & BSDF::ETransmission)
            Log(EError, "Only materials without "
                "a transmission component can be nested!");
    }

    template <typename BSDFSample, typename Value  = typename BSDFSample::Value,
              typename Point2 = typename BSDFSample::Point2,
              typename Spectrum = Spectrum<Value>>
    std::pair<Spectrum, Value> sample_impl(BSDFSample &bs,
                                           const Point2 &sample,
                                           const mask_t<Value> &active) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        Spectrum result(0.0f);
        Value pdf(0.0f);

        Mask flipped = (Frame::cos_theta(bs.wi) < 0.0f);

        Mask mask = (!flipped && active);
        if (any(mask)) {
            Spectrum result_(0.0f);
            Value pdf_(0.0f);
            std::tie(result_, pdf_) = m_nested_brdf[0]->sample(bs, sample, mask);
            masked(result, mask) = result_;
            masked(pdf,    mask) = pdf_;
        }

        mask = (flipped && active);
        if (any(mask)) {
            bs.wi.z() *= -1.0f;
            if (bs.component != -1)
                bs.component -= 1;

            Spectrum result_(0.0f);
            Value pdf_(0.0f);
            std::tie(result_, pdf_) = m_nested_brdf[1]->sample(bs, sample, mask);
            masked(result, mask) = result_;
            masked(pdf,    mask) = pdf_;

            // revert changes
            bs.wi.z() *= -1.0f;
            if (bs.component != -1)
                bs.component += 1;

            Mask valid = mask && (any(result > 0.0f) && neq(pdf, 0.0f));
            masked(bs.wo.z(), valid) *= -1.0f;
            masked(bs.sampled_component, valid) += 1;
        }

        return { result, pdf };
    }

    template <typename BSDFSample,
              typename Value = typename BSDFSample::Value,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const BSDFSample &bs, EMeasure measure,
                       const mask_t<Value> &active) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Frame = Frame<Vector3>;
        using Mask  = mask_t<Value>;

        Spectrum result(0.0f);

        Mask eval_front = (Frame::cos_theta(bs.wi) > 0.0f);

        Mask mask = (eval_front && active);
        if (any(mask))
            masked(result, mask) = m_nested_brdf[0]->eval(bs, measure, mask);

        mask = (!eval_front && active);
        if (any(mask)) {
            BSDFSample b(bs);
            if (b.component != -1)
                b.component -= 1;
            b.wi.z() *= -1.0f;
            b.wo.z() *= -1.0f;
            masked(result, mask) =  m_nested_brdf[1]->eval(b, measure, mask);
        }

        return result;
    }

    template <typename BSDFSample,
              typename Value = typename BSDFSample::Value>
    Value pdf_impl(const BSDFSample &bs, EMeasure measure,
                   const mask_t<Value> &active) const {
        using Mask = mask_t<Value>;

        Value result(0.0f);

        Mask sample_front = (bs.wi.z() > 0.0f);

        Mask mask = (sample_front && active);
        if (any(mask))
            masked(result, mask) = m_nested_brdf[0]->pdf(bs, measure, mask);

        mask = (!sample_front && active);
        if (any(mask)) {
            BSDFSample b(bs);
            if (b.component != -1)
                b.component -= 1;
            b.wi.z() *= -1.0f;
            b.wo.z() *= -1.0f;
            masked(result, mask) = m_nested_brdf[1]->pdf(b, measure, mask);
        }

        return result;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "TwoSided[" << std::endl
            << "  nested_brdf[0] = " << string::indent(m_nested_brdf[0]->to_string()) << "," << std::endl
            << "  nested_brdf[1] = " << string::indent(m_nested_brdf[1]->to_string()) << std::endl
            << "]";
        return oss.str();
    }


    MTS_IMPLEMENT_BSDF()
    MTS_DECLARE_CLASS()
protected:
    ref<BSDF> m_nested_brdf[2];
};

MTS_IMPLEMENT_CLASS(TwoSidedBRDF, BSDF)
MTS_EXPORT_PLUGIN(TwoSidedBRDF, "Two-sided BRDF adapter");

NAMESPACE_END(mitsuba)
