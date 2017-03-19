#include <mitsuba/core/spectrum.h>
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
class BlackBodySpectrum final : public ContinuousSpectrum {
public:
    /* A few natural constants */
    const Float c = 2.99792458e+8f;   /* Speed of light */
    const Float h = 6.62607004e-34f;  /* Planck constant */
    const Float k = 1.38064852e-23f;  /* Boltzmann constant */

    /* First and second radiation static constants */
    const Float c0 = 2 * h * c * c;
    const Float c1 = h * c / k;

    BlackBodySpectrum(const Properties &props) {
        m_temperature = props.float_("temperature");
        m_integral_min = cdf_and_pdf(MTS_WAVELENGTH_MIN).first;
        m_integral = cdf_and_pdf(MTS_WAVELENGTH_MAX).first - m_integral_min;
    }

    template <typename T>
    MTS_INLINE T eval_impl(T lambda_) const {
        auto mask_valid =
            lambda_ >= MTS_WAVELENGTH_MIN &&
            lambda_ <= MTS_WAVELENGTH_MAX;

        T lambda  = lambda_ * 1e-9f;
        T lambda2 = lambda * lambda;
        T lambda5 = lambda2 * lambda2 * lambda;

        /* Watts per unit surface area (m^-2)
                 per unit wavelength (nm^-1)
                 per unit steradian (sr^-1) */
        auto P = 1e-9f * c0 / (lambda5 *
                (exp(c1 / (lambda * m_temperature)) - 1.0f));

        return P & mask_valid;
    }

    template <typename T>
    MTS_INLINE T pdf_impl(T lambda_) const {
        const T lambda  = lambda_ * 1e-9f,
                lambda2 = lambda * lambda,
                lambda5 = lambda2 * lambda2 * lambda;

        /* Wien's approximation to Planck's law  */
        return 1e-9f * c0 * exp(-c1 / (lambda * m_temperature))
            / (lambda5 * m_integral);
    }

    template <typename T> std::pair<T, T> cdf_and_pdf(T lambda_) const {
        const Float c1_2 = c1 * c1,
                    c1_3 = c1_2 * c1,
                    c1_4 = c1_2 * c1_2;

        const Float K = m_temperature,
                    K2 = K*K, K3 = K2*K;

        const T lambda  = lambda_ * 1e-9f,
                lambda2 = lambda * lambda,
                lambda3 = lambda2 * lambda,
                lambda5 = lambda2 * lambda3;

        T expval = exp(-c1 / (K * lambda));

        T cdf = c0 * K * expval *
                (c1_3 + 3 * c1_2 * K * lambda + 6 * c1 * K2 * lambda2 +
                 6 * K3 * lambda3) / (c1_4 * lambda3);

        T pdf = 1e-9f * c0 * expval / lambda5;

        return std::make_pair(cdf, pdf);
    }

    template <typename T, typename Sample>
    std::tuple<T, T, T> sample_impl(Sample sample_) const {
        using Mask = mask_t<T>;

        T sample = sample_shifted<T>(sample_) * m_integral + m_integral_min;

        const Float eps        = 1e-5f,
                    eps_domain = eps * (MTS_WAVELENGTH_MAX - MTS_WAVELENGTH_MIN),
                    eps_value  = eps * m_integral;

        T a = MTS_WAVELENGTH_MIN,
          b = MTS_WAVELENGTH_MAX,
          t = 0.5f * (MTS_WAVELENGTH_MIN + MTS_WAVELENGTH_MAX),
          value, deriv;

        Mask active(true);
        do {
            /* Fall back to a bisection step when t is out of bounds */
            t = select(!(t > a & t < b) & active, 0.5f * (a + b), t);

            /* Evaluate the definite integral and its derivative
               (i.e. the spline) */
            std::tie(value, deriv) = cdf_and_pdf(t);
            value -= sample;

            /* Update which lanes are still active */
            active &= (abs(value) > eps_value) & (b - a > eps_domain);

            /* Stop the iteration if converged */
            if (none_nested(active))
                break;

            /* Update the bisection bounds */
            a = select(value <= 0, t, a);
            b = select(value >  0, t, b);

            /* Perform a Newton step */
            t = select(active, t - value / deriv, t);
        } while (true);

        auto pdf = deriv / m_integral;

        return std::make_tuple(t, eval(t) / pdf, pdf);
    }

    DiscreteSpectrum  eval(DiscreteSpectrum  lambda) const override { return eval_impl(lambda); }
    DiscreteSpectrumP eval(DiscreteSpectrumP lambda) const override { return eval_impl(lambda); }

    DiscreteSpectrum  pdf(DiscreteSpectrum  lambda) const override { return pdf_impl(lambda); }
    DiscreteSpectrumP pdf(DiscreteSpectrumP lambda) const override { return pdf_impl(lambda); }

    std::tuple<DiscreteSpectrum, DiscreteSpectrum, DiscreteSpectrum>
    sample(Float sample) const override {
        return sample_impl<DiscreteSpectrum>(sample);
    }

    std::tuple<DiscreteSpectrumP, DiscreteSpectrumP, DiscreteSpectrumP>
    sample(FloatP sample) const override {
        return sample_impl<DiscreteSpectrumP>(sample);
    }

    Float integral() const override { return m_integral; }

    MTS_DECLARE_CLASS()

private:
    Float m_temperature;
    Float m_integral_min;
    Float m_integral;
};

MTS_IMPLEMENT_CLASS(BlackBodySpectrum, ContinuousSpectrum)
MTS_EXPORT_PLUGIN(BlackBodySpectrum, "Black body spectrum")

NAMESPACE_END(mitsuba)
