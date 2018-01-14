#include <mitsuba/core/string.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>

NAMESPACE_BEGIN(mitsuba)

class Mask : public BSDF {
public:
    Mask(const Properties &props){
        auto bsdfs = props.objects();
        m_nested_bsdf = dynamic_cast<BSDF *>(bsdfs[0].second.get());

        if (!m_nested_bsdf)
            Throw("A child BSDF is required");

        m_flags = m_nested_bsdf->flags() | ENull | EFrontSide | EBackSide;

        m_opacity = props.spectrum("opacity", 0.5f);
    }

    template <typename BSDFContext,
              typename BSDFSample = BSDFSample<typename BSDFContext::Point3>,
              typename Value      = typename BSDFContext::Value,
              typename Point2     = typename BSDFContext::Point2,
              typename Spectrum   = Spectrum<Value>>
    std::pair<BSDFSample, Spectrum> sample_impl(const BSDFContext &ctx,
                                                Value sample1,
                                                const Point2 &sample2,
                                                const mask_t<Value> &active) const {
        using Mask = mask_t<Value>;

        Point2 sample(sample2);
        Value pdf;
        Spectrum result(0.0f);

        Spectrum opacity = m_opacity->eval(ctx.si.wavelengths, active);
        Value prob = luminance(opacity, ctx.si.wavelengths, active);

        bool sample_transmission = ((ctx.component == (uint32_t) -1) || (ctx.component == 0))
                                   && (ctx.type_mask & ENull),
             sample_nested       = (ctx.component == (uint32_t) -1) || (ctx.component < 0);

        BSDFSample bs;
        if (sample_transmission && sample_nested) {
            Mask nested = (sample.x() < prob);
            sample.x() /= prob;
            Spectrum nested_result;
            std::tie(bs, nested_result)
                = m_nested_bsdf->sample(ctx, sample1, sample, (active && nested));

            Mask not_nested = !nested;
            masked(bs.wo,  not_nested) = -ctx.si.wi;
            masked(bs.eta, not_nested) = 1.0f;
            masked(bs.sampled_component, not_nested) = 0;
            masked(bs.sampled_type, not_nested) = ENull;
            bs.pdf = select(nested, bs.pdf * prob, 1.0f - prob);
            result = select(nested,
                            nested_result * opacity / prob,
                            (Spectrum(1.0f) - opacity) / pdf);
        } else if (sample_transmission) {
            bs.wo = -ctx.si.wi;
            bs.eta = 1.0f;
            bs.sampled_component = 0;
            bs.sampled_type = ENull;
            bs.pdf = 1.0f;
            result = Spectrum(1.0f) - opacity;
        } else if (sample_nested) {
            std::tie(bs, result) = m_nested_bsdf->sample(ctx, sample1, sample, active);
            result *= opacity;
        }

        return { bs, result };
    }

    template <typename BSDFContext,
              typename Value    = typename BSDFContext::Value,
              typename Vector3  = typename BSDFContext::Vector3,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const BSDFContext &ctx, const Vector3 &wo,
                       const mask_t<Value> &active) const {
        return m_nested_bsdf->eval(ctx, wo, active)
               * m_opacity->eval(ctx.si.wavelengths, active);
    }

    template <typename BSDFContext,
              typename Value    = typename BSDFContext::Value,
              typename Vector3  = typename BSDFContext::Vector3>
    Value pdf_impl(const BSDFContext &ctx, const Vector3 &wo,
                   const mask_t<Value> &active) const {
        bool sample_transmission = ((ctx.component == (uint32_t) -1) || (ctx.component == 0))
                                   && (ctx.type_mask & ENull),
             sample_nested       = (ctx.component == (uint32_t) -1) || (ctx.component < 0);

        Value prob = luminance(m_opacity->eval(ctx.si.wavelengths, active),
                               ctx.si.wavelengths, active);
        if (!sample_nested)
            return 0.0f;
        Value result = m_nested_bsdf->pdf(ctx, wo, active);
        if (sample_transmission)
            result *= prob;
        return result;

        // TODO what should we do in EDiscrete case?
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Mask[" << std::endl
            << "  opacity = "     << m_opacity << "," << std::endl
            << "  nested_bsdf = " << string::indent(m_nested_bsdf->to_string()) << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_BSDF()
    MTS_DECLARE_CLASS()
protected:
    ref<ContinuousSpectrum> m_opacity;
    ref<BSDF> m_nested_bsdf;
};
MTS_IMPLEMENT_CLASS(Mask, BSDF)
MTS_EXPORT_PLUGIN(Mask, "Mask BRDF")

NAMESPACE_END(mitsuba)
