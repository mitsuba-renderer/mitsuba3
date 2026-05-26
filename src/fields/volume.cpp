#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/volume.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _texture-volume:

Volumetric texture (:monosp:`volume`)
---------------------------------------------

.. pluginparameters::

 * - volume
   - |float|, |spectrum| or |volume|
   - Volumetric texture (Default: 0.75).
   - |exposed|, |differentiable|

This plugin allows using a 3D texture (i.e. a :ref:`volume <sec-volume>` plugin)
to texture a 2D surface. This is intended to be used to texture surfaces without
a meaningful UV parameterization (e.g., an implicit surface) or to apply
procedural 3D textures. At a given point on a surface, the texture value will be
determined by looking up the corresponding value in the referenced `volume`.
This is done in world space and potentially requires using the volume's
`to_world` transformation to align the volume with the object using the texture.

.. tabs:: .. code-tab:: xml

        <texture type="volume">
            <volume name="volume" type="gridvolume">
                <string name="filename" value="my_volume.vol"/>
            </volume>
        </texture>

    .. code-tab:: python

        'type': 'volume',
        'volume': {
            'type': 'gridvolume',
            'filename': 'my_volume.vol'
        }

 */

template <typename Float, typename Spectrum>
class VolumeAdapter final : public SurfaceField<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Field, SurfaceField, Texture, Volume)

    using Args    = typename Texture::Args;
    using Array6f = typename Texture::Array6f;

    VolumeAdapter(const Properties &props) : SurfaceField(props) {
        m_volume = props.get_volume_field<Field>("volume");
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &it,
                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return m_volume->eval(static_cast<const Interaction3f &>(it), active);
    }

    Float eval_1(const SurfaceInteraction3f &it, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return m_volume->eval_1(static_cast<const Interaction3f &>(it), active);
    }

    Color3f eval_3(const SurfaceInteraction3f &it, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        auto ret = m_volume->eval_3(static_cast<const Interaction3f &>(it),
                                    active);
        return Color3f(ret.x(), ret.y(), ret.z());
    }

    FieldValueType out_type() const override {
        FieldValueType type = m_volume->out_type();
        return type == FieldValueType::Array3 ? FieldValueType::Color3 : type;
    }

    uint32_t out_dim() const override { return m_volume->out_dim(); }

    void eval_n(const SurfaceInteraction3f &si, Float *out, uint32_t count,
                Args args = {}, Mask active = true) const override {
        if (args.size != 0)
            Throw("VolumeAdapter::eval_n(): expected args_dim=0, got %u.",
                  args.size);
        uint32_t dim = out_dim();
        FieldValueType type = out_type();
        if (count != dim)
            Throw("VolumeAdapter::eval_n(): count (%u) must match out_dim (%u).",
                  count, dim);

        switch (type) {
            case FieldValueType::Float:
                out[0] = eval_1(si, active);
                break;

            case FieldValueType::Spectrum: {
                UnpolarizedSpectrum value = eval(si, active);
                for (uint32_t i = 0; i < count; ++i)
                    out[i] = value.entry(i);
                break;
            }

            case FieldValueType::Color3: {
                Color3f value = eval_3(si, active);
                out[0] = value.x();
                out[1] = value.y();
                out[2] = value.z();
                break;
            }

            case FieldValueType::Features:
                m_volume->eval_n(static_cast<const Interaction3f &>(si), out,
                                 count, Args{}, active);
                break;

            default:
                Throw("VolumeAdapter::eval_n(): unsupported field output.");
        }
    }

    Array6f eval_array6(const SurfaceInteraction3f &si,
                        Args args = {},
                        Mask active = true) const override {
        if (args.size != 0)
            Throw("VolumeAdapter::eval_array6(): expected args_dim=0, got %u.",
                  args.size);
        FieldValueType type = out_type();
        uint32_t dim = out_dim();
        if (type != FieldValueType::Features || dim != 6)
            Throw("VolumeAdapter::eval_array6(): expected Features[6], got "
                  "a different field output.");
        return m_volume->eval_array6(static_cast<const Interaction3f &>(si),
                                     Args{}, active);
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("volume", m_volume, ParamFlags::Differentiable);
    }

    bool is_spatially_varying() const override { return true; }

    ScalarFloat max() const override { return m_volume->max(); };

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Volume[" << std::endl
            << "  volume = " << string::indent(m_volume) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(VolumeAdapter)
protected:
    ref<Field> m_volume;

    MI_TRAVERSE_CB(SurfaceField, m_volume)
};

MI_EXPORT_PLUGIN(VolumeAdapter)
NAMESPACE_END(mitsuba)
