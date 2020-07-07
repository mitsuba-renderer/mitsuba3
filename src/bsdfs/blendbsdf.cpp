#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!
.. _bsdf-blendbsdf:

Blended material (:monosp:`blendbsdf`)
-------------------------------------------

.. pluginparameters::

 * - weight
   - |float| or |texture|
   - A floating point value or texture with values between zero and one. The extreme values zero and
     one activate the first and second nested BSDF respectively, and inbetween values interpolate
     accordingly. (Default: 0.5)
 * - (Nested plugin)
   - |bsdf|
   - Two nested BSDF instances that should be mixed according to the specified blending weight

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_blendbsdf.jpg
   :caption: A material created by blending between rough plastic and smooth metal based on a binary bitmap texture
.. subfigend::
    :label: fig-bsdf-blendbsdf

This plugin implements a *blend* material, which represents
linear combinations of two BSDF instances. Any surface scattering model in Mitsuba 2 (be it smooth,
rough, reflecting, or transmitting) can be mixed with others in this manner to synthesize new models.

The following XML snippet describes the material shown above:

.. code-block:: xml
    :name: blendbsdf

    <bsdf type="blendbsdf">
        <texture name="weight" type="bitmap">
            <string name="filename" value="pattern.png"/>
        </texture>
        <bsdf type="conductor">
        </bsdf>
        <bsdf type="roughplastic">
            <spectrum name="diffuse_reflectance" value="0.1"/>
        </bsdf>
    </bsdf>
 */

template <typename Float, typename Spectrum>
class BlendBSDF final : public BSDF<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(BSDF, m_flags, m_components)
    MTS_IMPORT_TYPES(Texture)

    BlendBSDF(const Properties &props) : Base(props) {
        int bsdf_index = 0;
        for (auto &[name, obj] : props.objects(false)) {
            auto *bsdf = dynamic_cast<Base *>(obj.get());
            if (bsdf) {
                if (bsdf_index == 2)
                    Throw("BlendBSDF: Cannot specify more than two child BSDFs");
                m_nested_bsdf[bsdf_index++] = bsdf;
                props.mark_queried(name);
            }
        }

        m_weight = props.texture<Texture>("weight");
        if (bsdf_index != 2)
            Throw("BlendBSDF: Two child BSDFs must be specified!");

        m_components.clear();
        for (size_t i = 0; i < 2; ++i)
            for (size_t j = 0; j < m_nested_bsdf[i]->component_count(); ++j)
                m_components.push_back(m_nested_bsdf[i]->flags(j));

        m_flags = m_nested_bsdf[0]->flags() | m_nested_bsdf[1]->flags();
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float sample1,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        Float weight = eval_weight(si, active);
        if (unlikely(ctx.component != (uint32_t) -1)) {
            bool sample_first = ctx.component < m_nested_bsdf[0]->component_count();
            BSDFContext ctx2(ctx);
            if (!sample_first)
                ctx2.component -= (uint32_t) m_nested_bsdf[0]->component_count();
            else
                weight = 1.f - weight;
            auto [bs, result] = m_nested_bsdf[sample_first ? 0 : 1]->sample(ctx2, si, sample1, sample2, active);
            result *= weight;
            return { bs, result };
        }

        BSDFSample3f bs = zero<BSDFSample3f>();
        Spectrum result(0.f);

        Mask m0 = active && sample1 >  weight,
             m1 = active && sample1 <= weight;

        if (any_or<true>(m0)) {
            auto [bs0, result0] = m_nested_bsdf[0]->sample(
                ctx, si, (sample1 - weight) / (1 - weight), sample2, m0);
            masked(bs, m0) = bs0;
            masked(result, m0) = result0;
        }

        if (any_or<true>(m1)) {
            auto [bs1, result1] = m_nested_bsdf[1]->sample(
                ctx, si, sample1 / weight, sample2, m1);
            masked(bs, m1) = bs1;
            masked(result, m1) = result1;
        }

        return { bs, result };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float weight = eval_weight(si, active);
        if (unlikely(ctx.component != (uint32_t) -1)) {
            bool sample_first = ctx.component < m_nested_bsdf[0]->component_count();
            BSDFContext ctx2(ctx);
            if (!sample_first)
                ctx2.component -= (uint32_t) m_nested_bsdf[0]->component_count();
            else
                weight = 1.f - weight;
            return weight * m_nested_bsdf[sample_first ? 0 : 1]->eval(ctx2, si, wo, active);
        }

        return m_nested_bsdf[0]->eval(ctx, si, wo, active) * (1 - weight) +
               m_nested_bsdf[1]->eval(ctx, si, wo, active) * weight;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

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
        return clamp(m_weight->eval_1(si, active), 0.f, 1.f);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("weight", m_weight.get());
        callback->put_object("bsdf_0", m_nested_bsdf[0].get());
        callback->put_object("bsdf_1", m_nested_bsdf[1].get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "BlendBSDF[" << std::endl
            << "  weight = " << string::indent(m_weight) << "," << std::endl
            << "  nested_bsdf[0] = " << string::indent(m_nested_bsdf[0]) << "," << std::endl
            << "  nested_bsdf[1] = " << string::indent(m_nested_bsdf[1]) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    ref<Texture> m_weight;
    ref<Base> m_nested_bsdf[2];
};

MTS_IMPLEMENT_CLASS_VARIANT(BlendBSDF, BSDF)
MTS_EXPORT_PLUGIN(BlendBSDF, "BlendBSDF material")
NAMESPACE_END(mitsuba)
