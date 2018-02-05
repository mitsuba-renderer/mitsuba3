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

        // Add all nested components, overwriting any front / back side flag.
        m_flags = (uint32_t) 0;
        for (size_t i = 0; i < m_nested_brdf[0]->component_count(); ++i) {
            m_components.push_back((m_nested_brdf[0]->flags(i) & ~EBackSide)
                                   | EFrontSide);
            m_flags |= m_components.back();
        }
        for (size_t i = 0; i < m_nested_brdf[1]->component_count(); ++i) {
            m_components.push_back((m_nested_brdf[1]->flags(i) & ~EFrontSide)
                                   | EBackSide);
            m_flags |= m_components.back();
        }

        if (m_flags & BSDF::ETransmission)
            Log(EError, "Only materials without "
                "a transmission component can be nested!");
    }

    template <typename SurfaceInteraction,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Value      = typename SurfaceInteraction::Value,
              typename Point2     = typename SurfaceInteraction::Point2,
              typename Spectrum   = Spectrum<Value>>
    std::pair<BSDFSample, Spectrum> sample_impl(
            const SurfaceInteraction &si, const BSDFContext &ctx, Value sample1,
            const Point2 &sample2, mask_t<Value> active) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Mask    = mask_t<Value>;

        size_t components_nested = m_nested_brdf[0]->component_count();
        Spectrum result(0.0f), result_nested;
        BSDFSample bs, bs_nested;

        Mask flipped = (Frame<Vector3>::cos_theta(si.wi) < 0.0f);

        Mask mask = (!flipped && active);
        if (any(mask)) {
            std::tie(bs_nested, result_nested) = m_nested_brdf[0]->sample(
                si, ctx, sample1, sample2, active);
            // TODO: could this be a single masked assignment instead?
            // masked(bs,     mask) = bs_nested;
            masked(bs.wi, mask) = bs_nested.wi;
            masked(bs.wo, mask) = bs_nested.wo;
            masked(bs.pdf, mask) = bs_nested.pdf;
            masked(bs.eta, mask) = bs_nested.eta;
            masked(bs.sampled_type, mask) = bs_nested.sampled_type;
            masked(bs.sampled_component, mask) = bs_nested.sampled_component;

            masked(result, mask) = result_nested;
        }

        mask = (flipped && active);
        if (any(mask)) {
            SurfaceInteraction si_(si);
            BSDFContext ctx_(ctx);
            si_.wi.z() *= -1.0f;
            if (ctx_.component != (uint32_t) -1)
                ctx_.component -= components_nested;

            std::tie(bs_nested, result_nested) = m_nested_brdf[1]->sample(
                si_, ctx_, sample1, sample2, active);
            bs_nested.wo.z() *= -1.0f;
            bs_nested.sampled_component += components_nested;

            // TODO: could this be a single masked assignment instead?
            // masked(bs,     mask) = bs_nested;
            masked(bs.wi, mask) = bs_nested.wi;
            masked(bs.wo, mask) = bs_nested.wo;
            masked(bs.pdf, mask) = bs_nested.pdf;
            masked(bs.eta, mask) = bs_nested.eta;
            masked(bs.sampled_type, mask) = bs_nested.sampled_type;
            masked(bs.sampled_component, mask) = bs_nested.sampled_component;

            masked(result, mask) = result_nested;
        }

        return { bs, result };
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const SurfaceInteraction &si, const BSDFContext &ctx,
                       const Vector3 &wo, const mask_t<Value> &active) const {
        using Frame = Frame<Vector3>;
        using Mask  = mask_t<Value>;

        Spectrum result(0.0f);

        Mask eval_front = (Frame::cos_theta(si.wi) > 0.0f);
        Mask mask = (eval_front && active);
        if (any(mask))
            masked(result, mask) = m_nested_brdf[0]->eval(si, ctx, wo, mask);

        mask = (!eval_front && active);
        if (any(mask)) {
            Vector3 wo_(wo.x(), wo.y(), -wo.z());
            SurfaceInteraction si_(si);
            BSDFContext ctx_(ctx);

            si_.wi.z() *= -1.0f;
            if (ctx_.component != (uint32_t) -1)
                ctx_.component -= m_nested_brdf[0]->component_count();
            masked(result, mask) =  m_nested_brdf[1]->eval(si_, ctx_, wo_, mask);
        }

        return result;
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3>
    Value pdf_impl(const SurfaceInteraction &si, const BSDFContext &ctx, const Vector3 &wo,
                   const mask_t<Value> & active) const {
        using Mask = mask_t<Value>;

        Value result(0.0f);
        Mask sample_front = (si.wi.z() > 0.0f);

        Mask mask = (sample_front && active);
        if (any(mask))
            masked(result, mask) = m_nested_brdf[0]->pdf(si, ctx, wo, mask);

        mask = (!sample_front && active);
        if (any(mask)) {
            Vector3 wo_(wo.x(), wo.y(), -wo.z());
            SurfaceInteraction si_(si);
            BSDFContext ctx_(ctx);

            si_.wi.z() *= -1.0f;
            if (ctx_.component != (uint32_t) -1)
                ctx_.component -= m_nested_brdf[0]->component_count();

            masked(result, mask) = m_nested_brdf[1]->pdf(si_, ctx_, wo_, mask);
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
