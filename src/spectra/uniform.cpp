#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)
/**
 * \brief Spectrum that takes on a constant value between
 * \c MTS_WAVELENGTH_MIN * and \c MTS_WAVELENGTH_MAX
 */
template <typename Float, typename Spectrum>
class UniformSpectrum final : public ContinuousSpectrum<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN(UniformSpectrum, ContinuousSpectrum)
    using Base = ContinuousSpectrum<Float, Spectrum>;

    UniformSpectrum(const Properties &props) {
        m_value = props.float_("value");

        #if defined(MTS_ENABLE_AUTODIFF)
            m_value_d = m_value;
        #endif
    }

    MTS_INLINE Spectrum eval(const Wavelength &lambda, Mask /*active*/) const override {
        auto active = (lambda >= MTS_WAVELENGTH_MIN) &&
                      (lambda <= MTS_WAVELENGTH_MAX);

        return select(active, Spectrum(m_value), Spectrum(0.f));
    }

    MTS_INLINE Spectrum pdf(const Wavelength &lambda, Mask /*active*/) const override {
        auto active = (lambda >= MTS_WAVELENGTH_MIN) &&
                      (lambda <= MTS_WAVELENGTH_MAX);

        return select(active,
            Spectrum(1.f / (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN)), Spectrum(0.f));
    }

    MTS_INLINE std::pair<Wavelength, Spectrum> sample(const Wavelength &sample, Mask /*active*/) const override {
        return { MTS_WAVELENGTH_MIN + (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN) * sample,
                 Spectrum(m_value * (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN)) };
    }

    Float mean() const override { return m_value; }

#if defined(MTS_ENABLE_AUTODIFF)
    void put_parameters(DifferentiableParameters &dp) override {
        dp.put(this, "value", m_value_d);
    }
#endif

    std::string to_string() const override {
        return tfm::format("UniformSpectrum[value=%f]", m_value);
    }

private:
    Float m_value;

#if defined(MTS_ENABLE_AUTODIFF)
    FloatD m_value_d;
#endif
};

MTS_IMPLEMENT_PLUGIN(UniformSpectrum, ContinuousSpectrum, "Uniform spectrum")

NAMESPACE_END(mitsuba)
