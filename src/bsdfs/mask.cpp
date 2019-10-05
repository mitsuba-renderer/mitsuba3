#include <mitsuba/core/string.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MaskBSDF final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES();
    using BSDF = BSDF<Float, Spectrum>;
    using BSDF::component_count;
    using BSDF::m_components;
    using BSDF::m_flags;

    MaskBSDF(const Properties &props) : BSDF(props) {
        for (auto &kv : props.objects()) {
            auto *bsdf = dynamic_cast<BSDF *>(kv.second.get());
            if (bsdf) {
                if (m_nested_bsdf)
                    Throw("Cannot specify more than one child BSDF");
                m_nested_bsdf = bsdf;
            }
        }
        if (!m_nested_bsdf)
           Throw("Child BSDF not specified");

        m_opacity = props.spectrum<Float, Spectrum>("opacity", 0.5f);

        for (size_t i = 0; i < m_nested_bsdf->component_count(); ++i)
            m_components.push_back(m_nested_bsdf->flags(i));

        // The "transmission" BSDF component is at the last index.
        m_components.push_back(BSDFFlags::Null | BSDFFlags::FrontSide | BSDFFlags::BackSide);
        m_flags = m_nested_bsdf->flags() | m_components.back();
    }

    MTS_INLINE
    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                                             Float sample1, const Point2f &sample2,
                                             Mask active) const override {
        uint32_t null_index = (uint32_t) component_count() - 1;

        bool sample_transmission = ctx.is_enabled(BSDFFlags::Null, null_index);
        bool sample_nested       = ctx.component == (uint32_t) -1 || ctx.component < null_index;

        BSDFSample3f bs;
        Spectrum result(0.f);
        if (unlikely(!sample_transmission && !sample_nested))
            return { bs, result };

        Float opacity = clamp(m_opacity->eval(si, active)[0], 0.f, 1.f);
        if (sample_transmission != sample_nested)
            opacity = sample_transmission ? 1.f : 0.f;

        bs.wo                = -si.wi;
        bs.eta               = 1.f;
        bs.sampled_component = null_index;
        bs.sampled_type      = BSDFFlags::Null;
        bs.pdf               = 1.f - opacity;
        result               = 1.f;

        Mask nested_mask = active && sample1 < opacity;
        if (any_or<true>(nested_mask)) {
            sample1 /= opacity;
            auto tmp                = m_nested_bsdf->sample(ctx, si, sample1, sample2, nested_mask);
            masked(bs, nested_mask) = tmp.first;
            masked(result, nested_mask) = tmp.second;
        }

        return { bs, result };
    }

    MTS_INLINE
    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo,
                  Mask active) const override {
        Float opacity = clamp(m_opacity->eval(si, active)[0], 0.f, 1.f);
        return m_nested_bsdf->eval(ctx, si, wo, active) * opacity;
    }

    MTS_INLINE
    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo,
              Mask active) const override {
        uint32_t null_index      = (uint32_t) component_count() - 1;
        bool sample_transmission = ctx.is_enabled(BSDFFlags::Null, null_index);
        bool sample_nested       = ctx.component == (uint32_t) -1 || ctx.component < null_index;

        if (!sample_nested)
            return 0.f;

        Float result = m_nested_bsdf->pdf(ctx, si, wo, active);
        if (sample_transmission)
            result *= clamp(m_opacity->eval(si, active)[0], 0.f, 1.f);

        return result;
    }

    std::vector<ref<Object>> children() override {
        return { m_opacity.get(), m_nested_bsdf.get() };
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Mask[" << std::endl
            << "  opacity = " << m_opacity << "," << std::endl
            << "  nested_bsdf = " << string::indent(m_nested_bsdf->to_string()) << std::endl
            << "]";
        return oss.str();
    }

    // MTS_IMPLEMENT_BSDF_ALL()
    MTS_DECLARE_CLASS()
protected:
    ref<ContinuousSpectrum<Float, Spectrum>> m_opacity;
    ref<BSDF> m_nested_bsdf;
};

// MTS_IMPLEMENT_CLASS(MaskBSDF, BSDF)
// MTS_EXPORT_PLUGIN(MaskBSDF, "Mask material")

NAMESPACE_END(mitsuba)
