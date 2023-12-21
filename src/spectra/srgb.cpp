#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _spectrum-srgb:

sRGB spectrum (:monosp:`srgb`)
------------------------------

.. pluginparameters::
 :extra-rows: 1

 * - color
   - :paramtype:`color`
   - The corresponding sRGB color value.

 * - value
   - :paramtype:`color`
   - Spectral upsampling model coefficients of the srgb color value.
   - |exposed|, |differentiable|

In spectral render modes, this smooth spectrum is the result of the
*spectral upsampling* process :cite:`Jakob2019Spectral` used by the system.
In RGB render modes, this spectrum represents a constant RGB value.
In monochrome modes, this spectrum represents a constant luminance value.

.. tabs::
    .. code-tab:: xml
        :name: srgb

        <spectrum type="srgb">
            <rgb name="color" value="10, 20, 250"/>
        </spectrum>

    .. code-tab:: python

        'type': 'srgb',
        'color': [10, 20, 250]

 */

template <typename Float, typename Spectrum>
class SRGBReflectanceSpectrum final : public Texture<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Texture)

    SRGBReflectanceSpectrum(const Properties &props) : Texture(props) {
        ScalarColor3f color = props.get<ScalarColor3f>("color");

        if (dr::any(color < 0 || color > 1) && !props.get<bool>("unbounded", false))
            Throw("Invalid RGB reflectance value %s, must be in the range [0, 1]!", color);
        props.mark_queried("unbounded");

        if constexpr (is_spectral_v<Spectrum>)
            m_value = srgb_model_fetch(color);
        else if constexpr (is_rgb_v<Spectrum>)
            m_value = color;
        else
            m_value = luminance(color);

        dr::make_opaque(m_value);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("value", m_value, +ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        dr::make_opaque(m_value);
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>)
            return srgb_model_eval<UnpolarizedSpectrum>(m_value, si.wavelengths);
        else
            return m_value;
    }

    Color3f eval_3(const SurfaceInteraction3f &/*si*/, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        if constexpr (is_monochromatic_v<Spectrum>)
            return Color3f(m_value[0]);
        else
            return m_value;
    }

    Float eval_1(const SurfaceInteraction3f & /*it*/, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);
        return mean();
    }

    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f &_si,
                    const Wavelength &sample,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::TextureSample, active);

        if constexpr (is_spectral_v<Spectrum>) {
            // TODO: better sampling strategy
            SurfaceInteraction3f si(_si);
            si.wavelengths = MI_CIE_MIN + (MI_CIE_MAX - MI_CIE_MIN) * sample;
            return { si.wavelengths, eval(si, active) * (MI_CIE_MAX - MI_CIE_MIN) };
        } else {
            DRJIT_MARK_USED(sample);
            UnpolarizedSpectrum value = eval(_si, active);
            return { dr::empty<Wavelength>(), value };
        }
    }

    Float mean() const override {
        if constexpr (is_spectral_v<Spectrum>)
            return dr::mean(srgb_model_mean(m_value));
        else
            return dr::mean(dr::mean(m_value));
    }

    ScalarFloat max() const override {
        if constexpr (is_spectral_v<Spectrum>)
            return dr::max_nested(srgb_model_mean(m_value));
        else
            return dr::max_nested(m_value);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SRGBReflectanceSpectrum[" << std::endl
            << "  value = " << string::indent(m_value) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
protected:
    /**
     * Depending on the compiled variant, this plugin either stores coefficients
     * for a spectral upsampling model, or a plain RGB/monochromatic value.
     */
    static constexpr size_t ChannelCount = is_monochromatic_v<Spectrum> ? 1 : 3;

    Color<Float, ChannelCount> m_value;
};

MI_IMPLEMENT_CLASS_VARIANT(SRGBReflectanceSpectrum, Texture)
MI_EXPORT_PLUGIN(SRGBReflectanceSpectrum, "sRGB spectrum")
NAMESPACE_END(mitsuba)
