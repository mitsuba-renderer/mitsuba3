#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/volume.h>

NAMESPACE_BEGIN(mitsuba)

/**!
.. _volume-constvolume:

Constant-valued volume data source (:monosp:`constvolume`)
----------------------------------------------------------

.. pluginparameters::

 * - Parameter
   - Type
   - Description

 * - value
   - |float| or |spectrum|
   - Specifies the value of the constant volume.

This plugin provides a volume data source that is constant throughout its domain.
Depending on how it is used, its value can either be a scalar or a color spectrum.

.. code-block:: xml
    :name: lst-constvolume

    <medium type="heterogeneous">
        <volume type="constvolume" name="albedo">
            <rgb name="value" value="0.99, 0.8, 0.8"/>
        </volume>

        <!-- shorthand: this will create a 'constvolume' internally -->
        <rgb name="albedo" value="0.99, 0.99, 0.99"/>
    </medium>
*/

template <typename Float, typename Spectrum>
class ConstVolume final : public Volume<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Volume, m_to_local)
    MTS_IMPORT_TYPES(Texture)

    ConstVolume(const Properties &props) : Base(props) {
        m_value = props.texture<Texture>("value", 1.f);
    }

    UnpolarizedSpectrum eval(const Interaction3f &it, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();
        si.t           = 0.f;
        si.uv          = Point2f(0.f, 0.f);
        si.wavelengths = it.wavelengths;
        si.time        = it.time;
        auto result = m_value->eval(si, active);
        return result;
    }

    Float eval_1(const Interaction3f & /* it */, Mask /* active */) const override {
        return m_value->mean();
    }

    ScalarFloat max() const override { NotImplementedError("max"); }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("value", m_value.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "ConstVolume[" << std::endl
            << "  to_local = " << m_to_local << "," << std::endl
            << "  value = " << m_value << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    ref<Texture> m_value;
};

MTS_IMPLEMENT_CLASS_VARIANT(ConstVolume, Volume)
MTS_EXPORT_PLUGIN(ConstVolume, "Constant 3D texture")
NAMESPACE_END(mitsuba)
