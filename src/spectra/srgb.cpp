#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class SRGBSpectrum final : public Texture<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Texture)

    SRGBSpectrum(const Properties &props) : Texture(props) {
        ScalarColor3f color = props.color("color");

        if (any(color < 0 || color > 1))
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

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask /*active*/) const override {
        if constexpr (is_spectral_v<Spectrum>)
            return srgb_model_eval<UnpolarizedSpectrum>(m_value, si.wavelengths);
        else
            return m_value;
    }

    ScalarFloat mean() const override {
        if constexpr (is_spectral_v<Spectrum>) {
            return srgb_model_mean(m_value);
        } else {
            return hmean(m_value);
        }
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("value", m_value);
    }

    MTS_DECLARE_CLASS()
protected:
    /**
     * Depending on the compiled variant, this plugin either stores coefficients
     * for a spectral upsampling model, or a plain RGB/monochromatic value.
     */
    static constexpr size_t ChannelCount = is_monochromatic_v<Spectrum> ? 1 : 3;

    Array<ScalarFloat, ChannelCount> m_value;
};

MTS_IMPLEMENT_CLASS_VARIANT(SRGBSpectrum, Texture)
MTS_EXPORT_PLUGIN(SRGBSpectrum, "sRGB spectrum")
NAMESPACE_END(mitsuba)
