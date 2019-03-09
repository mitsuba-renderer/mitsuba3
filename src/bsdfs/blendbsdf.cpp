#include <mitsuba/core/string.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

class BlendBSDF final : public BSDF {
public:
    BlendBSDF(const Properties &props) : BSDF(props) {
        int bsdf_index = 0;
        for (auto &kv : props.objects()) {
            auto *bsdf = dynamic_cast<BSDF *>(kv.second.get());
            if (bsdf) {
                if (bsdf_index == 2)
                    Throw("BlendBSDF: Cannot specify more than two child BSDFs");
                m_nested_bsdf[bsdf_index++] = bsdf;
            }
        }

        m_weight = props.spectrum("weight");
        if (bsdf_index != 2)
            Throw("BlendBSDF: Two child BSDFs must be specified!");

        for (size_t i = 0; i < 2; ++i)
            for (size_t j = 0; j < m_nested_bsdf[i]->component_count(); ++j)
                m_components.push_back(m_nested_bsdf[i]->flags(j));

        m_flags = m_nested_bsdf[0]->flags() | m_nested_bsdf[1]->flags();
    }

    template <typename SurfaceInteraction, typename Value, typename Point2,
              typename BSDFSample = BSDFSample<typename SurfaceInteraction::Point3>,
              typename Spectrum   = Spectrum<Value>>
    MTS_INLINE
    std::pair<BSDFSample, Spectrum> sample_impl(const BSDFContext &ctx,
                                                const SurfaceInteraction &si,
                                                Value sample1,
                                                const Point2 &sample2,
                                                mask_t<Value> active) const {

        if (unlikely(ctx.component != (uint32_t) -1)) {
            bool sample_first = ctx.component < m_nested_bsdf[0]->component_count();
            BSDFContext ctx2(ctx);
            if (!sample_first)
                ctx2.component -= (uint32_t) m_nested_bsdf[0]->component_count();
            return m_nested_bsdf[sample_first ? 0 : 1]->sample(ctx2, si, sample1, sample2, active);
        }

        using Mask = mask_t<Value>;

        BSDFSample bs;
        Spectrum result(0.f);

        Value weight = clamp(m_weight->eval(si, active)[0], 0.f, 1.f);

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

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3,
              typename Spectrum = Spectrum<Value>>
    MTS_INLINE
    Spectrum eval_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                       const Vector3 &wo, const mask_t<Value> &active) const {
        if (unlikely(ctx.component != (uint32_t) -1)) {
            bool sample_first = ctx.component < m_nested_bsdf[0]->component_count();
            BSDFContext ctx2(ctx);
            if (!sample_first)
                ctx2.component -= (uint32_t) m_nested_bsdf[0]->component_count();
            return m_nested_bsdf[sample_first ? 0 : 1]->eval(ctx2, si, wo, active);
        }

        Value weight = clamp(m_weight->eval(si, active)[0], 0.f, 1.f);
        return m_nested_bsdf[0]->eval(ctx, si, wo, active) * (1 - weight) +
               m_nested_bsdf[1]->eval(ctx, si, wo, active) * weight;
    }

    template <typename SurfaceInteraction,
              typename Value    = typename SurfaceInteraction::Value,
              typename Vector3  = typename SurfaceInteraction::Vector3>
    MTS_INLINE
    Value pdf_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                   const Vector3 &wo, const mask_t<Value> &active) const {
        if (unlikely(ctx.component != (uint32_t) -1)) {
            bool sample_first = ctx.component < m_nested_bsdf[0]->component_count();
            BSDFContext ctx2(ctx);
            if (!sample_first)
                ctx2.component -= (uint32_t) m_nested_bsdf[0]->component_count();
            return m_nested_bsdf[sample_first ? 0 : 1]->pdf(ctx2, si, wo, active);
        }

        Value weight = clamp(m_weight->eval(si, active)[0], 0.f, 1.f);
        return m_nested_bsdf[0]->pdf(ctx, si, wo, active) * (1 - weight) +
               m_nested_bsdf[1]->pdf(ctx, si, wo, active) * weight;
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

    MTS_IMPLEMENT_BSDF_ALL()
    MTS_DECLARE_CLASS()
protected:
    ref<ContinuousSpectrum> m_weight;
    ref<BSDF> m_nested_bsdf[2];
};

MTS_IMPLEMENT_CLASS(BlendBSDF, BSDF)
MTS_EXPORT_PLUGIN(BlendBSDF, "BlendBSDF material")

NAMESPACE_END(mitsuba)
