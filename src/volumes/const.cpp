#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/volume.h>

NAMESPACE_BEGIN(mitsuba)

/**!
.. _volume-constvolume:

Constant-valued volume data source (:monosp:`constvolume`)
----------------------------------------------------------

.. pluginparameters::

 * - value
   - |float| or |spectrum|
   - Specifies the value of the constant volume.
   - |exposed|, |differentiable|

This plugin provides a volume data source that is constant throughout its domain.
Depending on how it is used, its value can either be a scalar or a color spectrum.

.. tabs::
    .. code-tab::  xml
        :name: lst-constvolume

        <medium type="heterogeneous">
            <volume type="constvolume" name="albedo">
                <rgb name="value" value="0.99, 0.8, 0.8"/>
            </volume>

            <!-- shorthand: this will create a 'constvolume' internally -->
            <rgb name="albedo" value="0.99, 0.99, 0.99"/>
        </medium>

    .. code-tab:: python

        'type': 'heterogeneous',
        'albedo': {
            'type': 'constvolume',
            'value': {
                'type': 'rgb',
                'value': [0.99, 0.8, 0.8]
            }
        }

        # shorthand: this will create a 'constvolume' internally
        'albedo': {
            'type': 'rgb',
            'value': [0.99, 0.8, 0.8]
        }

*/

template <typename Float, typename Spectrum>
class ConstVolume final : public Volume<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Volume, m_to_local, m_bbox)
    MI_IMPORT_TYPES(Texture)

    ConstVolume(const Properties &props) : Base(props) {
        m_value = props.texture<Texture>("value", 1.f);
        if (m_value->is_spatially_varying())
            Throw("The value given to ConstVolume should not be spatially-varying.");
        update_max();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("value", m_value.get(), +ParamFlags::Differentiable);
    }

    UnpolarizedSpectrum eval(const Interaction3f &it, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
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

    ScalarFloat max() const override {
        return m_max;
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        update_max();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "ConstVolume[" << std::endl
            << "  to_local = " << string::indent(m_to_local) << "," << std::endl
            << "  bbox = " << string::indent(m_bbox) << ","  << std::endl
            << "  value = " << string::indent(m_value) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
protected:
    void update_max() {
        // It should be okay to query with a fake surface interaction,
        // since the "texture" is just a constant value.
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.wavelengths =
            Wavelength(0.5f * (MI_CIE_MAX - MI_CIE_MIN) + MI_CIE_MIN);
        Spectrum result = m_value->eval(si);
        // TODO: use better syntax for this
        Float tmp = dr::mean(dr::mean(dr::detach(result)));
        if constexpr (dr::is_jit_v<Float>)
            m_max = tmp[0];
        else
            m_max = tmp;
    }


protected:
    ref<Texture> m_value;
    ScalarFloat m_max;
};

MI_IMPLEMENT_CLASS_VARIANT(ConstVolume, Volume)
MI_EXPORT_PLUGIN(ConstVolume, "Constant 3D texture")
NAMESPACE_END(mitsuba)
