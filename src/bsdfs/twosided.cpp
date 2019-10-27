#include <mitsuba/core/string.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/spectrum.h>
#include <enoki/stl.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class TwoSidedBRDF final : public BSDF<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN(TwoSidedBRDF, BSDF);
    MTS_USING_BASE(BSDF, Base, m_flags, m_components)
    using BSDF = Base;

    TwoSidedBRDF(const Properties &props) : Base(props) {
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
        m_flags = BSDFFlags(0);
        for (size_t i = 0; i < m_nested_brdf[0]->component_count(); ++i) {
            auto c = (m_nested_brdf[0]->flags(i) & ~BSDFFlags::BackSide);
            m_components.push_back(c | BSDFFlags::FrontSide);
            m_flags = m_flags | m_components.back();
        }
        for (size_t i = 0; i < m_nested_brdf[1]->component_count(); ++i) {
            auto c = (m_nested_brdf[1]->flags(i) & ~BSDFFlags::FrontSide);
            m_components.push_back(c | BSDFFlags::BackSide);
            m_flags = m_flags | m_components.back();
        }

        if (has_flag(m_flags, BSDFFlags::Transmission))
            Throw("Only materials without a transmission component can be nested!");
    }

    MTS_INLINE
    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx_,
                                             const SurfaceInteraction3f &si_, Float sample1,
                                             const Point2f &sample2, Mask active) const override {
        using Result = std::pair<BSDFSample3f, Spectrum>;

        SurfaceInteraction3f si(si_);
        BSDFContext ctx(ctx_);

        Mask front_side = Frame3f::cos_theta(si.wi) > 0.f && active;
        Mask back_side  = Frame3f::cos_theta(si.wi) < 0.f && active;

        Result result = zero<Result>();
        if (any_or<true>(front_side))
            masked(result, front_side) =
                m_nested_brdf[0]->sample(ctx, si, sample1, sample2, front_side);

        if (any_or<true>(back_side)) {
            if (ctx.component != (uint32_t) -1)
                ctx.component -= (uint32_t) m_nested_brdf[0]->component_count();

            si.wi.z() *= -1.f;
            masked(result, back_side) =
                m_nested_brdf[1]->sample(ctx, si, sample1, sample2, back_side);
            masked(result.first.wo.z(), back_side) *= -1.f;
        }

        return result;
    }

    MTS_INLINE
    Spectrum eval(const BSDFContext &ctx_, const SurfaceInteraction3f &si_, const Vector3f &wo_,
                  Mask active) const override {
        SurfaceInteraction3f si(si_);
        BSDFContext ctx(ctx_);
        Vector3f wo(wo_);
        Spectrum result = 0.f;

        Mask front_side = Frame3f::cos_theta(si.wi) > 0.f && active;
        Mask back_side  = Frame3f::cos_theta(si.wi) < 0.f && active;

        if (any_or<true>(front_side))
            result = m_nested_brdf[0]->eval(ctx, si, wo, front_side);

        if (any_or<true>(back_side)) {
            if (ctx.component != (uint32_t) -1)
                ctx.component -= (uint32_t) m_nested_brdf[0]->component_count();

            si.wi.z() *= -1.f;
            wo.z() *= -1.f;

            masked(result, back_side) = m_nested_brdf[1]->eval(ctx, si, wo, back_side);
        }

        return result;
    }

    MTS_INLINE
    Float pdf(const BSDFContext &ctx_, const SurfaceInteraction3f &si_, const Vector3f &wo_,
              Mask active) const override {
        SurfaceInteraction3f si(si_);
        BSDFContext ctx(ctx_);
        Vector3f wo(wo_);
        Float result = 0.f;

        Mask front_side = Frame3f::cos_theta(si.wi) > 0.f && active;
        Mask back_side  = Frame3f::cos_theta(si.wi) < 0.f && active;

        if (any_or<true>(front_side))
            result = m_nested_brdf[0]->pdf(ctx, si, wo, front_side);

        if (any_or<true>(back_side)) {
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

protected:
    ref<BSDF> m_nested_brdf[2];
};

MTS_IMPLEMENT_PLUGIN(TwoSidedBRDF, BSDF, "Two-sided material adapter");
NAMESPACE_END(mitsuba)
