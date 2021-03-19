#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/texture.h>

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
   - |float|, |spectrum| or |string|
   - Specifies the value of the constant volume. If a string is passed, the
     implementation will interpret the string as a list of floating point values.

This plugin provides a volume data source that is constant throughout its domain.
Depending on how it is used, its value can either be a scalar, a color spectrum, or a higher dimensional vector.
The last option can be used to specify parameters of more complex volume models (e.g., the SGGX phase function).

.. code-block:: xml
    :name: lst-constvolume

    <medium type="heterogeneous">
        <volume type="constvolume" name="albedo">
            <rgb name="value" value="0.99,0.8,0.8"/>
        </volume>

        <!-- shorthand: this will create a 'constvolume' internally -->
        <rgb name="albedo" value="0.99,0.99,0.99"/>

        <!-- Use the string parameter to pass the list of SGGX phase function parameters -->
        <phase type="sggx">
            <volume type="constvolume" name="S">
                <string name="value" value="0.001, 1.0, 0.001, 0.0, 0.0, 0.0"/>
            </volume>
        </phase>
    </medium>
*/

template <typename Float, typename Spectrum>
class ConstVolume final : public Volume<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Volume, is_inside, m_world_to_local)
    MTS_IMPORT_TYPES(Texture)

    ConstVolume(const Properties &props) : Base(props) {
        if (props.type("value") == Properties::Type::String) {
            std::vector<std::string> values_str =
                string::tokenize(props.string("value"), " ,");

            if (values_str.size() != 6)
                Throw("String values parsing currently only supported for 6 "
                      "values (got %s)", values_str.size());

            std::vector<ScalarFloat> values;
            values.reserve(values_str.size());
            for (size_t i = 0; i < values_str.size(); ++i) {
                try {
                    values.push_back((ScalarFloat) std::stod(values_str[i]));
                } catch (...) {
                    Throw("Could not parse floating point value '%s'",
                          values_str[i]);
                }
            }
            m_vector = Vector<Float, 6>(values[0], values[1], values[2],
                                        values[3], values[4], values[5]);
        } else {
            m_color = props.texture<Texture>("value", 1.f);
        }
    }

    UnpolarizedSpectrum eval(const Interaction3f &it, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();
        si.t           = 0.f;
        si.uv          = Point2f(0.f, 0.f);
        si.wavelengths = it.wavelengths;
        si.time        = it.time;
        auto result = m_color->eval(si, active);
        return result;
    }

    Float eval_1(const Interaction3f & /* it */, Mask /* active */) const override {
        return m_color->mean();
    }

    Vector<Float, 6> eval_6(const Interaction3f & /* it */, Mask /* active */) const override {
        return m_vector;
    }

    Mask is_inside(const Interaction3f &it, Mask /*active*/) const override {
        auto p = m_world_to_local * it.p;
        return ek::all((p >= 0) && (p <= 1));
    }

    ScalarFloat max() const override { NotImplementedError("max"); }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("color", m_color.get());
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "ConstVolume[" << std::endl
            << "  world_to_local = " << m_world_to_local << "," << std::endl
            << "  color = " << m_color << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    ref<Texture> m_color;
    Vector<Float, 6> m_vector;
};

MTS_IMPLEMENT_CLASS_VARIANT(ConstVolume, Volume)
MTS_EXPORT_PLUGIN(ConstVolume, "Constant 3D texture")
NAMESPACE_END(mitsuba)
