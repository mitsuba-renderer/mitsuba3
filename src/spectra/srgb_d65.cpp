#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

class SRGBEmitterSpectrum final : public ContinuousSpectrum {
public:
    SRGBEmitterSpectrum(const Properties &props) {
        if (props.has_property("scale") && props.has_property("value"))
            Throw("Cannot specify both 'scale' and 'value'.");

        Color3f color = props.color("color");
        Float intensity = hmax(color) * 2.f;
        color /= intensity;

        m_coeff = srgb_model_fetch(color);

        Properties props2("d65");
        Float value =
            props.float_(props.has_property("scale") ? "scale" : "value", 1.0f);
        props2.set_float("value", value * intensity);
        m_d65 = (ContinuousSpectrum *) PluginManager::instance()
                    ->create_object<ContinuousSpectrum>(props2)
                    ->expand().front().get();

        #if defined(MTS_ENABLE_AUTODIFF)
            m_coeff_d = m_coeff;
        #endif
    }

    template <typename Value, typename Mask = mask_t<Value>>
    MTS_INLINE Value eval_impl(const Value &wavelengths, Mask active) const {
        return m_d65->eval(wavelengths, active) *
               srgb_model_eval(coeff<Value>(), wavelengths);
    }

#if defined(MTS_ENABLE_AUTODIFF)
    void put_parameters(DifferentiableParameters &dp) override {
        dp.put(this, "coeff", m_coeff_d);
    }
#endif

    MTS_IMPLEMENT_SPECTRUM_EVAL_ALL()
    MTS_AUTODIFF_GETTER(coeff, m_coeff, m_coeff_d)
    MTS_DECLARE_CLASS()

private:
    Vector3f m_coeff;
    ref<ContinuousSpectrum> m_d65;

#if defined(MTS_ENABLE_AUTODIFF)
    Vector3fD m_coeff_d;
#endif
};

MTS_IMPLEMENT_CLASS(SRGBEmitterSpectrum, ContinuousSpectrum)
MTS_EXPORT_PLUGIN(SRGBEmitterSpectrum, "sRGB x D65 spectrum")

NAMESPACE_END(mitsuba)
