#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

class SRGBEmitterSpectrum final : public ContinuousSpectrum {
public:
    SRGBEmitterSpectrum(const Properties &props) {
        m_color = props.color("color");
        m_coeff = srgb_model_fetch(m_color);

        Properties props2("d65");
        props2.set_float("value", props.float_("scale", 1.0f));
        m_d65 = (ContinuousSpectrum *) PluginManager::instance()
                    ->create_object<ContinuousSpectrum>(props2)
                    ->expand().front().get();
    }

    template <typename Value, typename Mask = mask_t<Value>>
    MTS_INLINE Value eval_impl(Value wavelengths, Mask active) const {
        return m_d65->eval(wavelengths, active) *
               srgb_model_eval(m_coeff, wavelengths);
    }

    Spectrumf eval(const Spectrumf &wavelengths) const override {
        return eval_impl(wavelengths, true);
    }

    SpectrumfP eval(const SpectrumfP &wavelengths,
                    MaskP active) const override {
        return eval_impl(wavelengths, active);
    }

    MTS_DECLARE_CLASS()

private:
    Color3f m_color;
    Vector4f m_coeff;
    ref<ContinuousSpectrum> m_d65;
};

MTS_IMPLEMENT_CLASS(SRGBEmitterSpectrum, ContinuousSpectrum)
MTS_EXPORT_PLUGIN(SRGBEmitterSpectrum, "sRGB x D65 spectrum")

NAMESPACE_END(mitsuba)
