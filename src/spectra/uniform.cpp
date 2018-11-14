#include <mitsuba/render/spectrum.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/autodiff.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)
/**
 * \brief Spectrum that takes on a constant value between
 * \c MTS_WAVELENGTH_MIN * and \c MTS_WAVELENGTH_MAX
 */
class UniformSpectrum final : public ContinuousSpectrum {
public:
    UniformSpectrum(const Properties &props) {
        m_value = props.float_("value");

        #if defined(MTS_ENABLE_AUTODIFF)
            m_value_d = m_value;
        #endif
    }

    template <typename Value>
    MTS_INLINE Value eval_impl(Value lambda, mask_t<Value> /* unused */ ) const {
        mask_t<Value> active = (lambda >= MTS_WAVELENGTH_MIN) &&
                               (lambda <= MTS_WAVELENGTH_MAX);

        return select(active, Value(value<Value>()), Value(0.f));
    }

    template <typename Value>
    MTS_INLINE Value pdf_impl(Value lambda, mask_t<Value> /* unused */) const {
        mask_t<Value> active = (lambda >= MTS_WAVELENGTH_MIN) &&
                               (lambda <= MTS_WAVELENGTH_MAX);

        return select(active,
            Value(1.f / (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN)), Value(0.f));
    }

    template <typename Value>
    MTS_INLINE std::pair<Value, Value> sample_impl(Value sample, mask_t<Value> /* unused */) const {
        return {
            MTS_WAVELENGTH_MIN + (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN) * sample,
            Value(value<Value>() * (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN))
        };
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

    MTS_IMPLEMENT_SPECTRUM_ALL()
    MTS_AUTODIFF_GETTER(value, m_value, m_value_d)
    MTS_DECLARE_CLASS()

private:
    Float m_value;

#if defined(MTS_ENABLE_AUTODIFF)
    FloatD m_value_d;
#endif
};

MTS_IMPLEMENT_CLASS(UniformSpectrum, ContinuousSpectrum)
MTS_EXPORT_PLUGIN(UniformSpectrum, "Uniform spectrum")

NAMESPACE_END(mitsuba)
