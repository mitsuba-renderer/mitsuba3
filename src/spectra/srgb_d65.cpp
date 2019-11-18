#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class SRGBEmitterSpectrum final : public ContinuousSpectrum<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(SRGBEmitterSpectrum, ContinuousSpectrum)
    MTS_IMPORT_BASE(ContinuousSpectrum)
    MTS_IMPORT_TYPES()

    SRGBEmitterSpectrum(const Properties &props) {
        if constexpr (is_rgb_v<Spectrum>)
            Throw("SRGBEmitterSpectrum being used in RGB mode. An SRGBSpectrum should "
                  " most likely be used instead");
        if (props.has_property("scale") && props.has_property("value"))
            Throw("Cannot specify both 'scale' and 'value'.");

        ScalarColor3f color = props.color("color");
        ScalarFloat intensity = hmax(color) * 2.f;
        color /= intensity;

        m_coeff = srgb_model_fetch(color);

        Properties props2("d65");
        ScalarFloat value =
            props.float_(props.has_property("scale") ? "scale" : "value", 1.f);
        props2.set_float("value", value * intensity);
        m_d65 = (Base *) PluginManager::instance()
                    ->create_object<Base>(props2)
                    ->expand().front().get();

        #if defined(MTS_ENABLE_AUTODIFF)
            m_coeff_d = m_coeff;
        #endif
    }

    Spectrum eval(const Wavelength &wavelengths, Mask active) const override {
        return m_d65->eval(wavelengths, active) *
               srgb_model_eval<Spectrum>(m_coeff, wavelengths);
    }

#if defined(MTS_ENABLE_AUTODIFF)
    void put_parameters(DifferentiableParameters &dp) override {
        dp.put(this, "coeff", m_coeff_d);
    }
#endif

private:
    Array<ScalarFloat, 3> m_coeff;
    ref<Base> m_d65;

#if defined(MTS_ENABLE_AUTODIFF)
    Vector3fD m_coeff_d;
#endif
};

MTS_EXPORT_PLUGIN(SRGBEmitterSpectrum, "sRGB x D65 spectrum")
NAMESPACE_END(mitsuba)
