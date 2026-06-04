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
class Checkerboard final : public SurfaceField<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Field, SurfaceField, Texture)

    Checkerboard(const Properties &props) : SurfaceField(props) {
        m_color0 = props.get_surface_field<Field>("color0", .4f);
        m_color1 = props.get_surface_field<Field>("color1", .2f);
        m_transform = props.get<ScalarAffineTransform3f>("to_uv", ScalarAffineTransform3f());
        (void) resolve_output_type();
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("to_uv",  m_transform, ParamFlags::NonDifferentiable);
        cb->put("color0", m_color0,    ParamFlags::Differentiable);
        cb->put("color1", m_color1,    ParamFlags::Differentiable);
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &it, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        Point2f uv = m_transform * it.uv;
        dr::mask_t<Point2f> mask = uv - dr::floor(uv) > .5f;
        UnpolarizedSpectrum result = dr::zeros<UnpolarizedSpectrum>();

        Mask m0 = mask.x() == mask.y(),
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

        Point2f uv = m_transform * it.uv;
        dr::mask_t<Point2f> mask = (uv - dr::floor(uv)) > .5f;
        Float result = 0.f;

        // Preserve the legacy scalar checkerboard convention.
        Mask m0 = mask.x() != mask.y(),
             m1 = !m0;

        m0 &= active; m1 &= active;

        if (dr::any_or<true>(m0))
            dr::masked(result, m0) = m_color0->eval_1(it, m0);

        if (dr::any_or<true>(m1))
            dr::masked(result, m1) = m_color1->eval_1(it, m1);

        return result;
    }

    Color3f eval_3(const SurfaceInteraction3f &it, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        Point2f uv = m_transform * it.uv;
        dr::mask_t<Point2f> mask = (uv - dr::floor(uv)) > .5f;
        Color3f result = 0.f;

        Mask m0 = mask.x() == mask.y(),
             m1 = !m0;

        m0 &= active; m1 &= active;

        if (dr::any_or<true>(m0))
            dr::masked(result, m0) = m_color0->eval_3(it, m0);

        if (dr::any_or<true>(m1))
            dr::masked(result, m1) = m_color1->eval_3(it, m1);

        return result;
    }

    FieldValueType out_type() const override {
        return resolve_output_type();
    }

    uint32_t out_dim() const override {
        switch (out_type()) {
            case FieldValueType::Float: return 1;
            case FieldValueType::Spectrum:
                return (uint32_t) dr::size_v<UnpolarizedSpectrum>;
            case FieldValueType::Color3:
            case FieldValueType::Array3:
                return 3;
            default:
                return SurfaceField::out_dim();
        }
    }

    Float mean() const override {
        return .5f * (m_color0->mean() + m_color1->mean());
    }

    bool is_spatially_varying() const override { return true; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Checkerboard[" << std::endl
            << "  color0 = " << string::indent(m_color0) << "," << std::endl
            << "  color1 = " << string::indent(m_color1) << "," << std::endl
            << "  transform = " << string::indent(m_transform) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(Checkerboard)

protected:
    FieldValueType resolve_output_type() const {
        FieldValueType type0 = m_color0->out_type(),
                       type1 = m_color1->out_type();
        uint32_t dim0 = m_color0->out_dim(),
                 dim1 = m_color1->out_dim();

        uint32_t spectrum_dim = (uint32_t) dr::size_v<UnpolarizedSpectrum>;
        bool scalar0 = type0 == FieldValueType::Float && dim0 == 1,
             scalar1 = type1 == FieldValueType::Float && dim1 == 1,
             spec0 = type0 == FieldValueType::Spectrum && dim0 == spectrum_dim,
             spec1 = type1 == FieldValueType::Spectrum && dim1 == spectrum_dim;

        if (scalar0 && scalar1)
            return FieldValueType::Float;
        if ((scalar0 || spec0) && (scalar1 || spec1))
            return FieldValueType::Spectrum;

        bool color0 = type0 == FieldValueType::Color3 && dim0 == 3,
             color1 = type1 == FieldValueType::Color3 && dim1 == 3,
             array0 = type0 == FieldValueType::Array3 && dim0 == 3,
             array1 = type1 == FieldValueType::Array3 && dim1 == 3;
        if (color0 && color1)
            return FieldValueType::Color3;
        if (array0 && array1)
            return FieldValueType::Array3;
        if ((color0 || array0) && (color1 || array1))
            return FieldValueType::Array3;

        Throw("Checkerboard: color0 and color1 have incompatible field outputs "
              "%s[%u] and %s[%u]. Expected two scalar/spectral fields or two "
              "3-channel color/vector fields.",
              field_value_type_name(type0), dim0, field_value_type_name(type1),
              dim1);
    }

    ref<Field> m_color0;
    ref<Field> m_color1;
    ScalarAffineTransform3f m_transform;

    MI_TRAVERSE_CB(SurfaceField, m_color0, m_color1)
};

MI_EXPORT_PLUGIN(Checkerboard)
NAMESPACE_END(mitsuba)
