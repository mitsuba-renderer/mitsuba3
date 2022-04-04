#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
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

This plugin provides a allows to apply a 3D texture (i.e. a plugin of type
`volume`) to a 2D surface. This can be useful to apply procedural 3D textures to
surfaces or texture surfaces without a meaningful UV parameterization (e.g., an
implicit surface). At a given point on a surface, the value will be determined
by looking up the corresponding value in the referenced `volume`. This is done
in world space and potentially requires using the volume's `to_world`
transformation to align the volume with the object using the texture.

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
class VolumeAdapter final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture, Volume)

    VolumeAdapter(const Properties &props) : Texture(props) {
        m_volume = props.volume<Volume>("volume", 0.75f);
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &it, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return m_volume->eval(it, active);
    }

    Float eval_1(const SurfaceInteraction3f &it, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return m_volume->eval_1(it, active);
    }

    Color3f eval_3(const SurfaceInteraction3f &it, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        auto ret = m_volume->eval_3(it, active);
        return Color3f(ret.x(), ret.y(), ret.z());
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("volume", m_volume.get(), +ParamFlags::Differentiable);
    }

    bool is_spatially_varying() const override { return true; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Volume[" << std::endl
            << "  volume = " << string::indent(m_volume) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
protected:
    ref<Volume> m_volume;
};

MI_IMPLEMENT_CLASS_VARIANT(VolumeAdapter, Texture)
MI_EXPORT_PLUGIN(VolumeAdapter, "Volumetric texture")
NAMESPACE_END(mitsuba)