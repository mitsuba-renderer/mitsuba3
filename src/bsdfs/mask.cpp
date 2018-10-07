#include <mitsuba/core/string.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

class Mask : public BSDF {
public:
    Mask(const Properties &props) {
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

        m_opacity = props.spectrum("opacity", 0.5f);

        for (size_t i = 0; i < m_nested_bsdf->component_count(); ++i)
            m_components.push_back(m_nested_bsdf->flags(i));

        // The "transmission" BSDF component is at the last index.
        m_components.push_back(ENull | EFrontSide | EBackSide);
        m_flags = m_nested_bsdf->flags() | m_components.back();
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
        using Mask = mask_t<Value>;

        uint32_t null_index = (uint32_t) component_count() - 1;

        bool sample_transmission = ctx.is_enabled(ENull, null_index);
        bool sample_nested       = ctx.component == (uint32_t) -1
                                || ctx.component < null_index;

        BSDFSample bs;
        Spectrum result(0.f);
        if (unlikely(!sample_transmission && !sample_nested))
            return { bs, result };

        Value opacity = clamp(m_opacity->eval(si, active)[0], 0.f, 1.f);
        if (sample_transmission != sample_nested)
            opacity = sample_transmission ? 1.f : 0.f;

        bs.wo                = -si.wi;
        bs.eta               = 1.f;
        bs.sampled_component = null_index;
        bs.sampled_type      = ENull;
        bs.pdf               = 1.f - opacity;
        result               = 1.f;

        Mask nested_mask = active && sample1 < opacity;
        if (any(nested_mask)) {
            sample1 /= opacity;
            auto tmp = m_nested_bsdf->sample(ctx, si, sample1, sample2, nested_mask);
            masked(bs, nested_mask) = tmp.first;
            masked(result, nested_mask) = tmp.second;
        }

        return { bs, result };
    }

    template <typename SurfaceInteraction, typename Vector3,
              typename Value    = typename SurfaceInteraction::Value,
              typename Spectrum = Spectrum<Value>>
    MTS_INLINE
    Spectrum eval_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                       const Vector3 &wo, mask_t<Value> active) const {
        Value opacity = clamp(m_opacity->eval(si, active)[0], 0.f, 1.f);
        return m_nested_bsdf->eval(ctx, si, wo, active) * opacity;
    }

    template <typename SurfaceInteraction, typename Vector3,
              typename Value = value_t<Vector3>>
    MTS_INLINE
    Value pdf_impl(const BSDFContext &ctx, const SurfaceInteraction &si,
                   const Vector3 &wo, mask_t<Value> active) const {
        uint32_t null_index = (uint32_t) component_count() - 1;
        bool sample_transmission = ctx.is_enabled(ENull, null_index);
        bool sample_nested       = ctx.component == (uint32_t) -1
                                || ctx.component < null_index;

        if (!sample_nested)
            return 0.f;

        Value result = m_nested_bsdf->pdf(ctx, si, wo, active);
        if (sample_transmission)
            result *= clamp(m_opacity->eval(si, active)[0], 0.f, 1.f);

        return result;
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
MTS_EXPORT_PLUGIN(Mask, "Mask material")

NAMESPACE_END(mitsuba)
