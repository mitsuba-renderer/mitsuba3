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
class ConstVolume final : public VolumeField<Float, Spectrum> {
public:
    MI_IMPORT_BASE(VolumeField, m_to_local)
    MI_IMPORT_TYPES(Field, SurfaceField, Texture)

    ConstVolume(const Properties &props) : Base(props) {
        m_value = props.get_unbounded_surface_field<Field>("value", 1.f);

        if (!props.has_property("value")) {
            m_out_type = FieldValueType::Float;
        } else {
            switch (props.type("value")) {
                case Properties::Type::Float:
                case Properties::Type::Integer:
                    m_out_type = FieldValueType::Float;
                    break;

                case Properties::Type::Object:
                    m_out_type = m_value->out_type();
                    break;

                default:
                    m_out_type = FieldValueType::Spectrum;
                    break;
            }
        }

    }

    void traverse(TraversalCallback *cb) override {
        cb->put("value", m_value, ParamFlags::Differentiable);
    }

    UnpolarizedSpectrum eval(const Interaction3f &it, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        SurfaceInteraction3f si = surface_interaction(it);
        auto result = m_value->eval(si, active);
        return result;
    }

    Float eval_1(const Interaction3f & /*it*/, Mask active) const override {
        return dr::select(active, m_value->mean(), 0.f);
    }

    Vector3f eval_3(const Interaction3f &it, Mask active = true) const override {
        SurfaceInteraction3f si = surface_interaction(it);
        Color3f value = m_value->eval_3(si, active);
        return Vector3f(value.x(), value.y(), value.z());
    }

    FieldValueType out_type() const override { return m_out_type; }

    uint32_t out_dim() const override {
        switch (m_out_type) {
            case FieldValueType::Float: return 1;
            case FieldValueType::Spectrum:
                return (uint32_t) dr::size_v<UnpolarizedSpectrum>;
            case FieldValueType::Color3:
            case FieldValueType::Array3:
                return 3;
            default:
                return m_value->out_dim();
        }
    }

    void eval_n(const Interaction3f &it, Float *out, uint32_t count,
                typename Base::Args args = {}, Mask active = true) const override {
        if (args.size != 0)
            Throw("ConstVolume::eval_n(): expected args_dim=0, got %u.",
                  args.size);
        uint32_t dim = out_dim();
        if (count != dim)
            Throw("ConstVolume::eval_n(): count (%u) must match out_dim (%u).",
                  count, dim);

        SurfaceInteraction3f si = surface_interaction(it);
        switch (m_out_type) {
            case FieldValueType::Float:
                out[0] = eval_1(it, active);
                break;

            case FieldValueType::Spectrum: {
                UnpolarizedSpectrum value = eval(it, active);
                for (uint32_t i = 0; i < count; ++i)
                    out[i] = value.entry(i);
                break;
            }

            case FieldValueType::Color3:
            case FieldValueType::Array3: {
                Color3f value = m_value->eval_3(si, active);
                out[0] = value.x();
                out[1] = value.y();
                out[2] = value.z();
                break;
            }

            default:
                Throw("ConstVolume::eval_n(): unsupported field output.");
        }
    }

    ScalarFloat max() const override { return m_value->max(); }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "ConstVolume[" << std::endl
            << "  to_local = " << string::indent(m_to_local, 13) << "," << std::endl
            << "  value = " << string::indent(m_value, 4) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(ConstVolume)
protected:
    SurfaceInteraction3f surface_interaction(const Interaction3f &it) const {
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.t           = 0.f;
        si.uv          = Point2f(0.f, 0.f);
        si.wavelengths = it.wavelengths;
        si.time        = it.time;
        return si;
    }

    ref<Field> m_value;
    FieldValueType m_out_type;

    MI_TRAVERSE_CB(Base, m_value)
};

MI_EXPORT_PLUGIN(ConstVolume)
NAMESPACE_END(mitsuba)
