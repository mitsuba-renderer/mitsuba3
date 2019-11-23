#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)
/**
 * \brief Spectrum that takes on a constant value between
 * \c MTS_WAVELENGTH_MIN * and \c MTS_WAVELENGTH_MAX
 */
template <typename Float, typename Spectrum>
class UniformSpectrum final : public Texture<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(UniformSpectrum, Texture)
    MTS_IMPORT_TYPES()

    UniformSpectrum(const Properties &props) {
        m_value = props.float_("value");

        #if defined(MTS_ENABLE_AUTODIFF)
            m_value_d = m_value;
        #endif
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si,
                             Mask /*active*/) const override {
        if constexpr (is_spectral_v<Spectrum>) {
            auto active = (si.wavelengths >= MTS_WAVELENGTH_MIN) &&
                          (si.wavelengths <= MTS_WAVELENGTH_MAX);

            return select(active, UnpolarizedSpectrum(m_value),
                                  UnpolarizedSpectrum(0.f));
        } else {
            return m_value;
        }
    }

    Wavelength pdf(const SurfaceInteraction3f &si, Mask /*active*/) const override {
        if constexpr (is_spectral_v<Spectrum>) {
            auto active = (si.wavelengths >= MTS_WAVELENGTH_MIN) &&
                          (si.wavelengths <= MTS_WAVELENGTH_MAX);

            return select(active,
                Wavelength(1.f / (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN)), Wavelength(0.f));
        } else {
            Throw("Not implemented for non-spectral modes"); // TODO
        }
    }

    std::pair<Wavelength, UnpolarizedSpectrum> sample(const SurfaceInteraction3f &/*si*/,
                                                      const Wavelength &sample,
                                                      Mask /*active*/) const override {
        if constexpr (is_spectral_v<Spectrum>) {
            return { MTS_WAVELENGTH_MIN + (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN) * sample,
                     UnpolarizedSpectrum(m_value * (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN)) };
        } else {
            Throw("Not implemented for non-spectral modes"); // TODO
        }
    }

    ScalarFloat mean() const override { return m_value; }

#if defined(MTS_ENABLE_AUTODIFF)
    void put_parameters(DifferentiableParameters &dp) override {
        dp.put(this, "value", m_value_d);
    }
#endif

    std::string to_string() const override {
        return tfm::format("UniformSpectrum[value=%f]", m_value);
    }

private:
    ScalarFloat m_value;

#if defined(MTS_ENABLE_AUTODIFF)
    FloatD m_value_d;
#endif
};

MTS_EXPORT_PLUGIN(UniformSpectrum, "Uniform spectrum")

NAMESPACE_END(mitsuba)
