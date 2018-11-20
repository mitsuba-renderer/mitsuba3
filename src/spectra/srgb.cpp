#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

class SRGBSpectrum final : public ContinuousSpectrum {
public:
    SRGBSpectrum(const Properties &props) {
        m_color = props.color("color");
        if (any(m_color < 0 || m_color > 1))
            Throw("Invalid RGB reflectance value %s, must be in the range "
                  "[0, 1]!", m_color);

        m_coeff = srgb_model_fetch(m_color);
    }

    template <typename Value, typename Mask = mask_t<Value>>
    MTS_INLINE Value eval_impl(Value wavelengths, Mask /* active */) const {
        return srgb_model_eval(m_coeff, wavelengths);
    }

    Spectrumf eval(const Spectrumf &wavelengths) const override {
        return eval_impl(wavelengths, true);
    }

    SpectrumfP eval(const SpectrumfP &wavelengths,
                    MaskP active) const override {
        return eval_impl(wavelengths, active);
    }

    Float mean() const override { return srgb_model_mean(m_coeff); }

    MTS_DECLARE_CLASS()

private:
    Color3f m_color;
    Vector4f m_coeff;
};

MTS_IMPLEMENT_CLASS(SRGBSpectrum, ContinuousSpectrum)
MTS_EXPORT_PLUGIN(SRGBSpectrum, "sRGB spectrum")

NAMESPACE_END(mitsuba)
