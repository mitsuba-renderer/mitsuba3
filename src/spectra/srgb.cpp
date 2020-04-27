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

        if (any(color < 0 || color > 1) && !props.bool_("unbounded", false))
            Throw("Invalid RGB reflectance value %s, must be in the range [0, 1]!", color);

        if constexpr (is_spectral_v<Spectrum>) {
            m_value = srgb_model_fetch(color);
        } else if constexpr (is_rgb_v<Spectrum>) {
            m_value = color;
        } else {
            static_assert(is_monochromatic_v<Spectrum>);
            m_value = luminance(color);
        }
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (is_spectral_v<Spectrum>)
            return srgb_model_eval<UnpolarizedSpectrum>(m_value, si.wavelengths);
        else
            return m_value;
    }

    ScalarFloat mean() const override {
        if constexpr (is_spectral_v<Spectrum>)
            return scalar_cast(hmean(srgb_model_mean(m_value)));
        else
            return scalar_cast(hmean(hmean(m_value)));
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("value", m_value);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/) override {
        if constexpr (!is_spectral_v<Spectrum>)
            m_value = clamp(m_value, 0.f, 1.f);
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
