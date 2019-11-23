#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Spectral power distribution based on Planck's black body law
 *
 * Computes the spectral power distribution of a black body of the specified
 * temperature in Kelvins.
 *
 * The scale factors of 1e-9f are needed to perform a conversion between
 * densities per unit nanometer and per unit meter.
 */
template <typename Float, typename Spectrum>
class BlackBodySpectrum final : public Texture<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(BlackBodySpectrum, Texture)
    MTS_IMPORT_BASE(Texture)
    MTS_IMPORT_TYPES()

    // A few natural constants
    constexpr static ScalarFloat c = ScalarFloat(2.99792458e+8);   /// Speed of light
    constexpr static ScalarFloat h = ScalarFloat(6.62607004e-34);  /// Planck constant
    constexpr static ScalarFloat k = ScalarFloat(1.38064852e-23);  /// Boltzmann constant

    /// First and second radiation static constants
    constexpr static ScalarFloat c0 = 2 * h * c * c;
    constexpr static ScalarFloat c1 = h * c / k;

    BlackBodySpectrum(const Properties &props) {
        m_temperature = props.float_("temperature");
        m_integral_min = cdf_and_pdf(ScalarFloat(MTS_WAVELENGTH_MIN)).first;
        m_integral = cdf_and_pdf(ScalarFloat(MTS_WAVELENGTH_MAX)).first - m_integral_min;
    }

    UnpolarizedSpectrum eval_impl(const Wavelength &wavelengths, Mask active_) const {
        if constexpr (is_spectral_v<Spectrum>) {
            mask_t<Wavelength> active = active_;

            active &= wavelengths >= MTS_WAVELENGTH_MIN
                   && wavelengths <= MTS_WAVELENGTH_MAX;

            Wavelength lambda  = wavelengths * 1e-9f,
                       lambda2 = sqr(lambda),
                       lambda5 = sqr(lambda2) * lambda;

            /* Watts per unit surface area (m^-2)
                     per unit wavelength (nm^-1)
                     per unit steradian (sr^-1) */
            UnpolarizedSpectrum P = 1e-9f * c0 / (lambda5 *
                    (exp(c1 / (lambda * m_temperature)) - 1.f));

            return select(active, P, 0.f);
        } else {
            /// TODO : implement reasonable thing to do in mono/RGB mode
            Throw("Not implemented for non-spectral modes");
        }
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        return eval_impl(si.wavelengths, active);
    }

    Wavelength pdf(const SurfaceInteraction3f &si, Mask active_) const override {
        if constexpr (is_spectral_v<Spectrum>) {
            mask_t<Wavelength> active = active_;

            active &= si.wavelengths >= MTS_WAVELENGTH_MIN
                   && si.wavelengths <= MTS_WAVELENGTH_MAX;

            Wavelength lambda  = si.wavelengths * 1e-9f,
                       lambda2 = sqr(lambda),
                       lambda5 = sqr(lambda2) * lambda;

            // Wien's approximation to Planck's law
            Wavelength pdf = 1e-9f * c0 * exp(-c1 / (lambda * m_temperature))
                / (lambda5 * m_integral);

            return select(active, pdf, 0.f);
        } else {
            /// TODO : implement reasonable thing to do in mono/RGB mode
            Throw("Not implemented for non-spectral modes");
        }
    }

    template <typename Value>
    std::pair<Value, Value> cdf_and_pdf(Value lambda) const {
        Value c1_2 = sqr(c1),
              c1_3 = c1_2 * c1,
              c1_4 = sqr(c1_2);

        const Value K  = m_temperature,
                    K2 = sqr(K),
                    K3 = K2*K;

        lambda *= 1e-9f;

        Value lambda2 = sqr(lambda),
              lambda3 = lambda2 * lambda,
              lambda5 = lambda2 * lambda3;

        Value expval = exp(-c1 / (K * lambda));

        Value cdf = c0 * K * expval *
                (c1_3 + 3 * c1_2 * K * lambda + 6 * c1 * K2 * lambda2 +
                 6 * K3 * lambda3) / (c1_4 * lambda3);

        Value pdf = 1e-9f * c0 * expval / lambda5;

        return { cdf, pdf };
    }

    std::pair<Wavelength, UnpolarizedSpectrum> sample(const SurfaceInteraction3f &/* si */,
                                                      const Wavelength &sample_,
                                                      Mask active_) const override {
        using WavelengthMask = mask_t<Wavelength>;

        if constexpr (is_spectral_v<Spectrum>) {
            WavelengthMask active = active_;

            Wavelength sample = fmadd(sample_, Wavelength(m_integral), Wavelength(m_integral_min));

            const ScalarFloat eps        = 1e-5f,
                              eps_domain = eps * (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN),
                              eps_value  = eps * m_integral;

            Wavelength a = MTS_WAVELENGTH_MIN,
                       b = MTS_WAVELENGTH_MAX,
                       t = .5f * (MTS_WAVELENGTH_MIN + MTS_WAVELENGTH_MAX),
                       value, deriv;

            do {
                // Fall back to a bisection step when t is out of bounds
                WavelengthMask bisect_mask = !((t > a) && (t < b));
                masked(t, bisect_mask && active) = .5f * (a + b);

                // Evaluate the definite integral and its derivative (i.e. the spline)
                std::tie(value, deriv) = cdf_and_pdf(t);
                value -= sample;

                // Update which lanes are still active
                active = active && (abs(value) > eps_value) && (b - a > eps_domain);

                // Stop the iteration if converged
                if (none_nested(active))
                    break;

                // Update the bisection bounds
                WavelengthMask update_mask = value <= 0.f;
                masked(a,  update_mask) = t;
                masked(b, !update_mask) = t;

                // Perform a Newton step
                masked(t, active) = t - value / deriv;
            } while (true);

            Wavelength pdf = deriv / m_integral;

            return { t, eval_impl(t, active_) / pdf };
        } else {
            Throw("Not implemented for non-spectral modes");
        }
    }

    ScalarFloat mean() const override {
        return m_integral / (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN);
    }

private:
    ScalarFloat m_temperature;
    ScalarFloat m_integral_min;
    ScalarFloat m_integral;
};

MTS_EXPORT_PLUGIN(BlackBodySpectrum, "Black body spectrum")

NAMESPACE_END(mitsuba)
