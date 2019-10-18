#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class SRGBSpectrum final : public ContinuousSpectrum<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN()
    using Base = ContinuousSpectrum<Float, Spectrum>;

    SRGBSpectrum(const Properties &props) {
        Color3f color = props.color("color");
        if (any(color < 0 || color > 1))
            Throw("Invalid RGB reflectance value %s, must be in the range [0, 1]!", color);

        m_coeff = srgb_model_fetch(color);

#if defined(MTS_ENABLE_AUTODIFF)
        m_coeff_d = m_coeff;
#endif
    }

    MTS_INLINE Spectrum eval(const Wavelength &wavelengths, Mask /* active */) const override {
        return srgb_model_eval(m_coeff, wavelengths);
    }

    Float mean() const override { return srgb_model_mean(m_coeff); }

#if defined(MTS_ENABLE_AUTODIFF)
    void put_parameters(DifferentiableParameters &dp) override {
        dp.put(this, "coeff", m_coeff_d);
    }
#endif

private:
    Vector3f m_coeff;

#if defined(MTS_ENABLE_AUTODIFF)
    Vector3fD m_coeff_d;
#endif
};

MTS_IMPLEMENT_PLUGIN(SRGBSpectrum, ContinuousSpectrum, "sRGB spectrum")

NAMESPACE_END(mitsuba)
