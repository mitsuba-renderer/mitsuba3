#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!
.. _bsdf-mask:

Opacity mask (:monosp:`mask`)
-------------------------------------------

.. pluginparameters::

 * - opacity
   - |spectrum| or |texture|
   - Specifies the opacity (where 1=completely opaque) (Default: 0.5)
   - |exposed|, |differentiable|, |discontinuous|

 * - (Nested plugin)
   - |bsdf|
   - A base BSDF model that represents the non-transparent portion of the scattering
   - |exposed|, |differentiable|

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_mask_before.jpg
    :caption: Rendering without an opacity mask
.. subfigure:: ../../resources/data/docs/images/render/bsdf_mask_after.jpg
    :caption: Rendering **with** an opacity mask
.. subfigend::
    :label: fig-bsdf-mask

This plugin applies an opacity mask to add nested BSDF instance. It interpolates
between perfectly transparent and completely opaque based on the :monosp:`opacity` parameter.

The transparency is internally implemented as a forward-facing Dirac delta distribution.
Note that the standard :ref:`path tracer <integrator-path>` does not have a good sampling strategy to deal with this,
but the (:ref:`volumetric path tracer <integrator-volpath>`) does. It may thus be preferable when rendering
scenes that contain the :ref:`mask <bsdf-mask>` plugin, even if there is nothing *volumetric* in the scene.

The following XML snippet describes a material configuration for a transparent leaf:

.. tabs::
    .. code-tab:: xml
        :name: mask-leaf

        <bsdf type="mask">
            <!-- Base material: a two-sided textured diffuse BSDF -->
            <bsdf type="twosided">
                <bsdf type="diffuse">
                    <texture name="reflectance" type="bitmap">
                        <string name="filename" value="leaf.png"/>
                    </texture>
                </bsdf>
            </bsdf>

            <!-- Fetch the opacity mask from a monochromatic texture -->
            <texture type="bitmap" name="opacity">
                <string name="filename" value="leaf_mask.png"/>
            </texture>
        </bsdf>

    .. code-tab:: python

        'type': 'mask',
        # Base material: a two-sided textured diffuse BSDF
        'material': {
            'type': 'twosided',
            'bsdf': {
                'type': 'diffuse',
                'reflectance': {
                    'type': 'bitmap',
                    'filename': 'leaf.png'
                }
            }
        },

        # Fetch the opacity mask from a monochromatic texture
        'opacity': {
            'type': 'texture',
            'filename': 'leaf_mask.png'
        }
*/

template <typename Float, typename Spectrum>
class MaskBSDF final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, component_count, m_components, m_flags)
    MI_IMPORT_TYPES(Texture)

    MaskBSDF(const Properties &props) : Base(props) {
        // Scalar-typed opacity texture
        m_opacity = props.texture<Texture>("opacity", 0.5f);

        for (auto &[name, obj] : props.objects(false)) {
            auto *bsdf = dynamic_cast<Base *>(obj.get());
            if (bsdf) {
                if (m_nested_bsdf)
                    Throw("Cannot specify more than one child BSDF");
                m_nested_bsdf = bsdf;
                props.mark_queried(name);
            }
        }
        if (!m_nested_bsdf)
           Throw("Child BSDF not specified");

        m_components.clear();
        for (size_t i = 0; i < m_nested_bsdf->component_count(); ++i)
            m_components.push_back(m_nested_bsdf->flags(i));

        // The "transmission" BSDF component is at the last index.
        m_components.push_back(BSDFFlags::Null | BSDFFlags::FrontSide | BSDFFlags::BackSide);
        m_flags = m_nested_bsdf->flags() | m_components.back();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("opacity",     m_opacity.get(),     ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_object("nested_bsdf", m_nested_bsdf.get(), +ParamFlags::Differentiable);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float sample1,
                                             const Point2f &sample2,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        uint32_t null_index = (uint32_t) component_count() - 1;

        bool sample_transmission = ctx.is_enabled(BSDFFlags::Null, null_index);
        bool sample_nested       = ctx.component == (uint32_t) -1 || ctx.component < null_index;

        BSDFSample3f bs = dr::zeros<BSDFSample3f>();
        Spectrum result(0.f);
        if (unlikely(!sample_transmission && !sample_nested))
            return { bs, result };

        Float opacity = eval_opacity(si, active);
        if (sample_transmission != sample_nested)
            opacity = sample_transmission ? 1.f : 0.f;

        bs.wo                = -si.wi;
        bs.eta               = 1.f;
        bs.sampled_component = null_index;
        bs.sampled_type      = +BSDFFlags::Null;
        bs.pdf               = 1.f - opacity;

        result = 1.f;
        if constexpr (dr::is_diff_v<Float>) {
            if (dr::grad_enabled(opacity)) {
                result = dr::replace_grad(
                    result,
                    Spectrum((1.f - opacity) / dr::detach(1.f - opacity)));
            }
        }

        Mask nested_mask = active && sample1 < opacity;
        if (dr::any_or<true>(nested_mask)) {
            sample1 /= opacity;
            auto tmp                = m_nested_bsdf->sample(ctx, si, sample1, sample2, nested_mask);
            dr::masked(bs, nested_mask) = tmp.first;
            dr::masked(result, nested_mask) = tmp.second * opacity / dr::detach(opacity);
            dr::masked(bs.pdf, nested_mask) *= opacity;
        }

        return { bs, result };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                  const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        Float opacity = eval_opacity(si, active);
        return m_nested_bsdf->eval(ctx, si, wo, active) * opacity;
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,
              const Vector3f &wo, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        uint32_t null_index      = (uint32_t) component_count() - 1;
        bool sample_transmission = ctx.is_enabled(BSDFFlags::Null, null_index);
        bool sample_nested       = ctx.component == (uint32_t) -1 || ctx.component < null_index;

        if (!sample_nested)
            return 0.f;

        Float result = m_nested_bsdf->pdf(ctx, si, wo, active);
        if (sample_transmission)
            result *= eval_opacity(si, active);

        return result;
    }

    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx,
                                        const SurfaceInteraction3f &si,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        uint32_t null_index      = (uint32_t) component_count() - 1;
        bool sample_transmission = ctx.is_enabled(BSDFFlags::Null, null_index);
        bool sample_nested       = ctx.component == (uint32_t) -1 || ctx.component < null_index;

        auto [value, pdf] = m_nested_bsdf->eval_pdf(ctx, si, wo, active);

        Float opacity = eval_opacity(si, active);
        value *= opacity;

        if (!sample_nested)
            pdf = 0.f;

        if (sample_transmission)
            pdf *= opacity;

        return { value, pdf };
    }

    Spectrum eval_null_transmission(const SurfaceInteraction3f &si,
                                    Mask active) const override {
        Float opacity = eval_opacity(si, active);
        return 1 - opacity * (1 - m_nested_bsdf->eval_null_transmission(si, active));
    }

    MI_INLINE Float eval_opacity(const SurfaceInteraction3f &si, Mask active) const {
        return dr::clip(m_opacity->eval_1(si, active), 0.f, 1.f);
    }

    Spectrum eval_diffuse_reflectance(const SurfaceInteraction3f &si,
                                      Mask active) const override {
        return m_nested_bsdf->eval_diffuse_reflectance(si, active);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Mask[" << std::endl
            << "  opacity = " << m_opacity << "," << std::endl
            << "  nested_bsdf = " << string::indent(m_nested_bsdf) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    ref<Texture> m_opacity;
    ref<Base> m_nested_bsdf;
};

MI_IMPLEMENT_CLASS_VARIANT(MaskBSDF, BSDF)
MI_EXPORT_PLUGIN(MaskBSDF, "Mask material")
NAMESPACE_END(mitsuba)
