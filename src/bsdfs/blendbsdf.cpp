#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class BlendBSDF final : public BSDF<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN(BlendBSDF, BSDF);
    MTS_USING_BASE(BSDF, m_flags, m_components)
    using BSDF               = Base;
    using ContinuousSpectrum = typename Aliases::ContinuousSpectrum;

    BlendBSDF(const Properties &props) : Base(props) {
        int bsdf_index = 0;
        for (auto &kv : props.objects()) {
            auto *bsdf = dynamic_cast<BSDF *>(kv.second.get());
            if (bsdf) {
                if (bsdf_index == 2)
                    Throw("BlendBSDF: Cannot specify more than two child BSDFs");
                m_nested_bsdf[bsdf_index++] = bsdf;
            }
        }

        m_weight = props.spectrum<Float, Spectrum>("weight");
        if (bsdf_index != 2)
            Throw("BlendBSDF: Two child BSDFs must be specified!");

        for (size_t i = 0; i < 2; ++i)
            for (size_t j = 0; j < m_nested_bsdf[i]->component_count(); ++j)
                m_components.push_back(m_nested_bsdf[i]->flags(j));

        m_flags = m_nested_bsdf[0]->flags() | m_nested_bsdf[1]->flags();
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                                             Float sample1, const Point2f &sample2,
                                             Mask active) const override {

        if (unlikely(ctx.component != (uint32_t) -1)) {
            bool sample_first = ctx.component < m_nested_bsdf[0]->component_count();
            BSDFContext ctx2(ctx);
            if (!sample_first)
                ctx2.component -= (uint32_t) m_nested_bsdf[0]->component_count();
            return m_nested_bsdf[sample_first ? 0 : 1]->sample(ctx2, si, sample1, sample2, active);
        }

        BSDFSample3f bs;
        Spectrum result(0.f);

        Float weight = eval_weight(si, active);

        Mask m0 = active && sample1 >  weight,
             m1 = active && sample1 <= weight;

        if (any_or<true>(m0)) {
            auto [bs0, result0] = m_nested_bsdf[0]->sample(
                ctx, si, sample1 / weight, sample2, m0);
            masked(bs, m0) = bs0;
            masked(result, m0) = result0;
        }

        if (any_or<true>(m1)) {
            auto [bs1, result1] = m_nested_bsdf[1]->sample(
                ctx, si, (sample1 + weight) / (1.f - weight), sample2, m1);
            masked(bs, m1) = bs1;
            masked(result, m1) = result1;
        }

        return { bs, result };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo,
                  Mask active) const override {
        if (unlikely(ctx.component != (uint32_t) -1)) {
            bool sample_first = ctx.component < m_nested_bsdf[0]->component_count();
            BSDFContext ctx2(ctx);
            if (!sample_first)
                ctx2.component -= (uint32_t) m_nested_bsdf[0]->component_count();
            return m_nested_bsdf[sample_first ? 0 : 1]->eval(ctx2, si, wo, active);
        }

        Float weight = eval_weight(si, active);
        return m_nested_bsdf[0]->eval(ctx, si, wo, active) * (1 - weight) +
               m_nested_bsdf[1]->eval(ctx, si, wo, active) * weight;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo,
              Mask active) const override {
        if (unlikely(ctx.component != (uint32_t) -1)) {
            bool sample_first = ctx.component < m_nested_bsdf[0]->component_count();
            BSDFContext ctx2(ctx);
            if (!sample_first)
                ctx2.component -= (uint32_t) m_nested_bsdf[0]->component_count();
            return m_nested_bsdf[sample_first ? 0 : 1]->pdf(ctx2, si, wo, active);
        }

        Float weight = eval_weight(si, active);
        return m_nested_bsdf[0]->pdf(ctx, si, wo, active) * (1 - weight) +
               m_nested_bsdf[1]->pdf(ctx, si, wo, active) * weight;
    }

    MTS_INLINE Float eval_weight(const SurfaceInteraction3f &si, const Mask &active) const {
        return clamp(m_weight->eval1(si, active), 0.f, 1.f);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "BlendBSDF[" << std::endl
            << "  weight = " << string::indent(m_weight->to_string()) << "," << std::endl
            << "  nested_bsdf[0] = " << string::indent(m_nested_bsdf[0]->to_string()) << "," << std::endl
            << "  nested_bsdf[0] = " << string::indent(m_nested_bsdf[1]->to_string()) << std::endl
            << "]";
        return oss.str();
    }

    std::vector<ref<Object>> children() override {
        return { m_nested_bsdf[0].get(), m_nested_bsdf[1].get(), m_weight.get() };
    }

protected:
    ref<ContinuousSpectrum> m_weight;
    ref<BSDF> m_nested_bsdf[2];
};

MTS_IMPLEMENT_PLUGIN(BlendBSDF, BSDF, "BlendBSDF material")
NAMESPACE_END(mitsuba)
