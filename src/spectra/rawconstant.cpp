#include <mitsuba/core/properties.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _spectrum-rawconstant:

Raw constant-valued texture (:monosp:`rawconstant`)
---------------------------------------------------

.. pluginparameters::

 * - value
   - :paramtype:`float` or :paramtype:`vector`
   - The constant value(s) to be returned. Can be a single float or a 3D vector.
   - |exposed|, |differentiable|

A constant-valued texture that returns the same value regardless of color mode,
UV coordinates or wavelength. No color conversion or range validation takes place.
The value can be 1D or 3D. For 1D inputs, the same value is replicated across components
when a 3D value is queried.

If color-handling is desired, see the :ref:`spectrum-srgb <spectrum-srgb>` plugin instead.

.. tabs::
    .. code-tab:: xml
        :name: rawconstant-1d

        <texture type="rawconstant">
            <float name="value" value="0.5"/>
        </texture>

    .. code-tab:: xml
        :name: rawconstant-3d

        <texture type="rawconstant">
            <vector name="value" value="0.5, 1.0, 0.3"/>
        </texture>

    .. code-tab:: python

        'type': 'rawconstant',
        'value': 0.5  # or [0.5, -2.0, 0.3]

 */

// Actualy ipmlementation, templated over the channel count.
template <typename Float, typename Spectrum, size_t Channels>
class RawConstantTextureImpl final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)
    static_assert(Channels == 1 || Channels == 3, "Channels must be 1 or 3.");
    using Value = std::conditional_t<Channels == 1, Float, Vector3f>;

    RawConstantTextureImpl(const Properties &props, const Value &value)
        : Texture(props), m_value(value) {}

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &/*si*/, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (dr::size_v<UnpolarizedSpectrum> == Channels || Channels == 1) {
            // The number of values we have either matches the number of entries
            // in an UnpolarizedSpectrum, or we can broadcast to it.
            return m_value;
        } else {
            // All other cases: throw to avoid unintuitive behavior.
            Throw("RawConstantTexture: eval() is not defined for %d channels "
                  "in variant where UnpolarizedSpectrum has %d entries.",
                  Channels, dr::size_v<UnpolarizedSpectrum>);
        }
    }

    Float eval_1(const SurfaceInteraction3f &/*si*/, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        if constexpr (Channels == 1) {
            return m_value;
        } else {
            Throw("RawConstantTexture: eval_1() is not defined for 3D-valued textures.");
        }
    }

    Color3f eval_3(const SurfaceInteraction3f &/*si*/, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        // If we have a single value, just broadcast it across all channels.
        return Color3f(m_value);
    }

    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f &si, const Wavelength &sample,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureSample, active);
        Wavelength wavelengths = dr::empty<Wavelength>();
        if constexpr (is_spectral_v<Spectrum>) {
            // Even though the value is constant, we may still want a valid
            // wavelength to be sampled.
            wavelengths = MI_CIE_MIN + (MI_CIE_MAX - MI_CIE_MIN) * sample;
        }
        return { wavelengths, eval(si, active) };
    }

    Float mean() const override {
        return dr::mean(m_value);
    }

    ScalarFloat max() const override {
        return dr::max_nested(m_value);
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("value", m_value, ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        dr::make_opaque(m_value);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "RawConstantTexture[" << std::endl
            << "  value = " << m_value << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(RawConstantTexture)
protected:
    Value m_value;
};

// Note: this gets expanded to a `RawConstantTextureImpl` specialized to the channel count.
template <typename Float, typename Spectrum>
class RawConstantTexture final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    RawConstantTexture(const Properties &props) : Texture(props), m_props(props) {
        if (!props.has_property("value"))
            Throw("RawConstantTexture: missing required parameter \"value\""
                  " (1D `float` or 3D `vector` expected).");
        props.mark_queried("value");
    }

    std::vector<ref<Object>> expand() const override {
        switch (m_props.type("value")) {
            case Properties::Type::Vector: {
                Vector3f value = m_props.get<ScalarVector3f>("value");
                return { ref<Object>(
                    new RawConstantTextureImpl<Float, Spectrum, 3>(m_props, value)) };
            }
            case Properties::Type::Float: {
                ScalarFloat value = m_props.get<ScalarFloat>("value");
                return { ref<Object>(
                    new RawConstantTextureImpl<Float, Spectrum, 1>(m_props, value)) };
            }

            default:
                Throw("RawConstantTexture: parameter \"value\" has incorrect "
                      "type, expected `float` or 3D `vector`.");
        }
    }

    MI_DECLARE_CLASS(RawConstantTextureImpl)
protected:
    Properties m_props;
};

MI_EXPORT_PLUGIN(RawConstantTexture)
NAMESPACE_END(mitsuba)
