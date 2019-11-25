#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class SRGBSpectrum final : public Texture<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(SRGBSpectrum, Texture)
    MTS_IMPORT_TYPES()

    SRGBSpectrum(const Properties &props) {
        ScalarColor3f color = props.color("color");

        if (any(color < 0 || color > 1))
            Throw("Invalid RGB reflectance value %s, must be in the range [0, 1]!", color);

        if constexpr (is_spectral_v<Spectrum>) {
            m_coeff = srgb_model_fetch(color);
        } else if constexpr (is_rgb_v<Spectrum>) {
            m_coeff = color;
        } else {
            static_assert(is_monochrome_v<Spectrum>);
            m_coeff = luminance(color);
        }
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask /*active*/) const override {
        if constexpr (is_spectral_v<Spectrum>)
            return srgb_model_eval<UnpolarizedSpectrum>(m_coeff, si.wavelengths);
        else
            return m_coeff;
    }

    ScalarFloat mean() const override {
        if constexpr (is_spectral_v<Spectrum>) {
            if constexpr (is_array_v<Float>)
                return srgb_model_mean(hmean_inner(m_coeff));
            else
                return srgb_model_mean(m_coeff);
        } else {
            return hmean_nested(m_coeff);
        }
    }

#if defined(MTS_ENABLE_AUTODIFF)
    void put_parameters(DifferentiableParameters &dp) override {
        dp.put(this, "coeff", m_coeff_d);
    }
#endif

protected:
    /**
     * Depending on the compiled variant, this plugin either stores coefficients
     * for a spectral upsampling model, or a plain RGB/monochromatic value.
     */
    static constexpr size_t ChannelCount = is_monochrome_v<Spectrum> ? 1 : 3;

    Array<Float, ChannelCount> m_coeff;
};

MTS_EXPORT_PLUGIN(SRGBSpectrum, "sRGB spectrum")

NAMESPACE_END(mitsuba)
