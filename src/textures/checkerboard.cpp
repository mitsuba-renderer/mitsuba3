#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _texture-checkerboard:

Checkerboard texture (:monosp:`checkerboard`)
---------------------------------------------

.. pluginparameters::

 * - color0, color1
   - |spectrum| or |texture|
   - Color values for the two differently-colored patches (Default: 0.4 and 0.2)
   - |exposed|, |differentiable|

 * - to_uv
   - |transform|
   - Specifies an optional 3x3 UV transformation matrix. A 4x4 matrix can also be provided.
     In that case, the last row and columns will be ignored.  (Default: none)
   - |exposed|

This plugin provides a simple procedural checkerboard texture with customizable colors.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/texture_checkerboard.jpg
   :caption: Checkerboard applied to the material test object as well as the ground plane.
.. subfigend::
    :label: fig-texture-checkerboard

.. tabs::
    .. code-tab:: xml
        :name: checkerboard-texture

        <texture type="checkerboard">
            <rgb name="color0" value="0.1, 0.1, 0.1"/>
            <rgb name="color1" value="0.5, 0.5, 0.5"/>
        </texture>

    .. code-tab:: python

        'type': 'checkerboard',
        'color0': [0.1, 0.1, 0.1],
        'color1': [0.5, 0.5, 0.5]

 */

template <typename Float, typename Spectrum>
class Checkerboard final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    Checkerboard(const Properties &props) : Texture(props) {
        m_color0 = props.texture<Texture>("color0", .4f);
        m_color1 = props.texture<Texture>("color1", .2f);
        m_transform = props.get<ScalarTransform3f>("to_uv", ScalarTransform3f());
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("to_uv", m_transform,    +ParamFlags::NonDifferentiable);
        callback->put_object("color0",   m_color0.get(), +ParamFlags::Differentiable);
        callback->put_object("color1",   m_color1.get(), +ParamFlags::Differentiable);
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &it, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        Point2f uv = m_transform.transform_affine(it.uv);
        dr::mask_t<Point2f> mask = uv - dr::floor(uv) > .5f;
        UnpolarizedSpectrum result = dr::zeros<UnpolarizedSpectrum>();

        Mask m0 = dr::eq(mask.x(), mask.y()),
             m1 = !m0;

        m0 &= active; m1 &= active;

        if (dr::any_or<true>(m0))
            result[m0] = m_color0->eval(it, m0);

        if (dr::any_or<true>(m1))
            result[m1] = m_color1->eval(it, m1);

        return result;
    }

    Float eval_1(const SurfaceInteraction3f &it, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        Point2f uv = m_transform.transform_affine(it.uv);
        dr::mask_t<Point2f> mask = (uv - dr::floor(uv)) > .5f;
        Float result = 0.f;

        Mask m0 = dr::neq(mask.x(), mask.y()),
             m1 = !m0;

        m0 &= active; m1 &= active;

        if (dr::any_or<true>(m0))
            dr::masked(result, m0) = m_color0->eval_1(it, m0);

        if (dr::any_or<true>(m1))
            dr::masked(result, m1) = m_color1->eval_1(it, m1);

        return result;
    }

    Float mean() const override {
        return .5f * (m_color0->mean() + m_color1->mean());
    }

    bool is_spatially_varying() const override { return true; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Checkerboard[" << std::endl
            << "  color0 = " << string::indent(m_color0) << std::endl
            << "  color1 = " << string::indent(m_color1) << std::endl
            << "  transform = " << string::indent(m_transform) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
protected:
    ref<Texture> m_color0;
    ref<Texture> m_color1;
    ScalarTransform3f m_transform;
};

MI_IMPLEMENT_CLASS_VARIANT(Checkerboard, Texture)
MI_EXPORT_PLUGIN(Checkerboard, "Checkerboard texture")
NAMESPACE_END(mitsuba)
