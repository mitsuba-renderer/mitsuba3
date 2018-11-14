#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/autodiff.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

class SRGBSpectrum final : public ContinuousSpectrum {
public:
    SRGBSpectrum(const Properties &props) {
        Color3f color = props.color("color");
        if (any(color < 0 || color > 1))
            Throw("Invalid RGB reflectance value %s, must be in the range "
                  "[0, 1]!", color);

        m_coeff = srgb_model_fetch(color);

        #if defined(MTS_ENABLE_AUTODIFF)
            m_coeff_d = m_coeff;
        #endif
    }

    template <typename Value, typename Mask = mask_t<Value>>
    MTS_INLINE Value eval_impl(Value wavelengths, Mask /* active */) const {
        return srgb_model_eval(coeff<Value>(), wavelengths);
    }

    Float mean() const override { return srgb_model_mean(m_coeff); }

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

#if defined(MTS_ENABLE_AUTODIFF)
    Vector3fD m_coeff_d;
#endif
};

MTS_IMPLEMENT_CLASS(SRGBSpectrum, ContinuousSpectrum)
MTS_EXPORT_PLUGIN(SRGBSpectrum, "sRGB spectrum")

NAMESPACE_END(mitsuba)
