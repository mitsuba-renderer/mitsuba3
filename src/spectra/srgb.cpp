#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _spectrum-srgb:

sRGB spectrum (:monosp:`srgb`)
------------------------------

In spectral render modes, this smooth spectrum is the result of the
*spectral upsampling* process :cite:`Jakob2019Spectral` used by the system.
In RGB render modes, this spectrum represents a constant RGB value.
In monochrome modes, this spectrum represents a constant luminance value.

 */

template <typename Float, typename Spectrum>
class SRGBReflectanceSpectrum final : public Texture<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Texture)

    SRGBReflectanceSpectrum(const Properties &props) : Texture(props) {
        ScalarColor3f color = props.color("color");

        if (ek::any(color < 0 || color > 1) && !props.bool_("unbounded", false))
            Throw("Invalid RGB reflectance value %s, must be in the range [0, 1]!", color);

        if constexpr (is_spectral_v<Spectrum>)
            m_value = srgb_model_fetch(color);
        else if constexpr (is_rgb_v<Spectrum>)
            m_value = color;
        else
            m_value = luminance(color);

        ek::make_opaque(m_value);
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>)
            return srgb_model_eval<UnpolarizedSpectrum>(m_value, si.wavelengths);
        else
            return m_value;
    }

    std::pair<Wavelength, UnpolarizedSpectrum>
    sample_spectrum(const SurfaceInteraction3f &_si, const Wavelength &sample,
                    Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureSample, active);

        if constexpr (is_spectral_v<Spectrum>) {
            // TODO: better sampling strategy
            SurfaceInteraction3f si(_si);
            si.wavelengths = MTS_WAVELENGTH_MIN +
                             (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN) * sample;
            return { si.wavelengths, eval(si, active) * (MTS_WAVELENGTH_MAX -
                                                         MTS_WAVELENGTH_MIN) };
        } else {
            ENOKI_MARK_USED(sample);
            UnpolarizedSpectrum value = eval(_si, active);
            return { ek::empty<Wavelength>(), value };
        }
    }

    Float mean() const override {
        if constexpr (is_spectral_v<Spectrum>)
            return ek::hmean(srgb_model_mean(m_value));
        else
            return ek::hmean(hmean(m_value));
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override {
        ek::make_opaque(m_value);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("value", m_value);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SRGBReflectanceSpectrum[" << std::endl
            << "  value = " << string::indent(m_value) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    /**
     * Depending on the compiled variant, this plugin either stores coefficients
     * for a spectral upsampling model, or a plain RGB/monochromatic value.
     */
    static constexpr size_t ChannelCount = is_monochromatic_v<Spectrum> ? 1 : 3;

    Color<Float, ChannelCount> m_value;
};

MTS_IMPLEMENT_CLASS_VARIANT(SRGBReflectanceSpectrum, Texture)
MTS_EXPORT_PLUGIN(SRGBReflectanceSpectrum, "sRGB spectrum")
NAMESPACE_END(mitsuba)
