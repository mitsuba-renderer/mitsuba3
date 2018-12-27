#include <mitsuba/core/string.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/spectrum.h>
#include <enoki/stl.h>

NAMESPACE_BEGIN(mitsuba)

class TwoSidedBRDF final : public BSDF {
public:
    TwoSidedBRDF(const Properties &props) {
        auto bsdfs = props.objects();
        if (bsdfs.size() > 0)
            m_nested_brdf[0] = dynamic_cast<BSDF *>(bsdfs[0].second.get());
        if (bsdfs.size() == 2)
            m_nested_brdf[1] = dynamic_cast<BSDF *>(bsdfs[1].second.get());
        else if (bsdfs.size() > 2)
            Throw("At most two nested BSDFs can be specified!");

        if (!m_nested_brdf[0])
            Throw("A nested one-sided material is required!");
        if (!m_nested_brdf[1])
            m_nested_brdf[1] = m_nested_brdf[0];

        // Add all nested components, overwriting any front / back side flag.
        m_flags = (uint32_t) 0;
        for (size_t i = 0; i < m_nested_brdf[0]->component_count(); ++i) {
            m_components.push_back((m_nested_brdf[0]->flags(i) & ~EBackSide) | EFrontSide);
            m_flags |= m_components.back();
        }
        for (size_t i = 0; i < m_nested_brdf[1]->component_count(); ++i) {
            m_components.push_back((m_nested_brdf[1]->flags(i) & ~EFrontSide) | EBackSide);
            m_flags |= m_components.back();
        }

        if (m_flags & BSDF::ETransmission)
            Throw("Only materials without a transmission component can be nested!");
    }

    template <typename SurfaceInteraction, typename Value, typename Point2,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Spectrum   = Spectrum<Value>>
    MTS_INLINE
    std::pair<BSDFSample, Spectrum> sample_impl(const BSDFContext &ctx_,
                                                const SurfaceInteraction &si_,
                                                Value sample1,
                                                const Point2 &sample2,
                                                mask_t<Value> active) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Frame = Frame<Vector3>;
        using Mask = mask_t<Value>;
        using Result = std::pair<BSDFSample, Spectrum>;

        SurfaceInteraction si(si_);
        BSDFContext ctx(ctx_);

        Mask front_side = Frame::cos_theta(si.wi) > 0.f && active;
        Mask back_side  = Frame::cos_theta(si.wi) < 0.f && active;
        Result result = zero<Result>();

        if (any(front_side))
            masked(result, front_side) = m_nested_brdf[0]->sample(ctx, si, sample1, sample2, front_side);

        if (any(back_side)) {
            if (ctx.component != (uint32_t) -1)
                ctx.component -= (uint32_t) m_nested_brdf[0]->component_count();

            si.wi.z() *= -1.f;
            masked(result, back_side) = m_nested_brdf[1]->sample(ctx, si, sample1, sample2, back_side);
            masked(result.first.wo.z(), back_side) *= -1.f;
        }

        return result;
    }

    template <typename SurfaceInteraction, typename Vector3,
              typename Value    = typename SurfaceInteraction::Value,
              typename Spectrum = Spectrum<Value>>
    MTS_INLINE
    Spectrum eval_impl(const BSDFContext &ctx_, const SurfaceInteraction &si_,
                       Vector3 wo, mask_t<Value> active) const {
        using Frame = mitsuba::Frame<Vector3>;
        using Mask = mask_t<Value>;

        SurfaceInteraction si(si_);
        BSDFContext ctx(ctx_);
        Spectrum result = 0.f;

        Mask front_side = Frame::cos_theta(si.wi) > 0.f && active;
        Mask back_side  = Frame::cos_theta(si.wi) < 0.f && active;

        if (any(front_side))
            result = m_nested_brdf[0]->eval(ctx, si, wo, front_side);

        if (any(back_side)) {
            if (ctx.component != (uint32_t) -1)
                ctx.component -= (uint32_t) m_nested_brdf[0]->component_count();

            si.wi.z() *= -1.f;
            wo.z() *= -1.f;

            masked(result, back_side) = m_nested_brdf[1]->eval(ctx, si, wo, back_side);
        }

        return result;
    }

    template <typename SurfaceInteraction, typename Vector3,
              typename Value = value_t<Vector3>>
    MTS_INLINE
    Value pdf_impl(const BSDFContext &ctx_, const SurfaceInteraction &si_,
                   Vector3 wo, mask_t<Value> active) const {
        using Frame = Frame<Vector3>;
        using Mask = mask_t<Value>;

        SurfaceInteraction si(si_);
        BSDFContext ctx(ctx_);
        Value result = 0.f;

        Mask front_side = Frame::cos_theta(si.wi) > 0.f && active;
        Mask back_side  = Frame::cos_theta(si.wi) < 0.f && active;

        if (any(front_side))
            result = m_nested_brdf[0]->pdf(ctx, si, wo, front_side);

        if (any(back_side)) {
            if (ctx.component != (uint32_t) -1)
                ctx.component -= (uint32_t) m_nested_brdf[0]->component_count();

            si.wi.z() *= -1.f;
            wo.z() *= -1.f;

            masked(result, back_side) = m_nested_brdf[1]->pdf(ctx, si, wo, back_side);
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
MTS_EXPORT_PLUGIN(TwoSidedBRDF, "Two-sided material adapter");

NAMESPACE_END(mitsuba)
