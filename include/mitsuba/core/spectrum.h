#pragma once

#include <mitsuba/core/simd.h>
#include <mitsuba/core/object.h>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

#if !defined(MTS_WAVELENGTH_SAMPLES)
#  define MTS_WAVELENGTH_SAMPLES 8
#endif

#if !defined(MTS_WAVELENGTH_MIN)
#  define MTS_WAVELENGTH_MIN Float(360)
#endif

#if !defined(MTS_WAVELENGTH_MAX)
#  define MTS_WAVELENGTH_MAX Float(830)
#endif

// =======================================================================
//! @{ \name Data types for discretized spectral data
// =======================================================================

/**
 * The following type is used for computations involving data that
 * is sampled at a fixed number of points in the wavelength domain.
 *
 * Note that this is not restricted to radiance data -- probabilities or
 * sampling weights often need to be expressed in a spectrally varying manner,
 * and this type also applies to these situations.
 */
using DiscreteSpectrum  = enoki::Array<Float,  MTS_WAVELENGTH_SAMPLES, true>;
using DiscreteSpectrumP = enoki::Array<FloatP, MTS_WAVELENGTH_SAMPLES, true>;
using DiscreteSpectrumX = enoki::Array<FloatX, MTS_WAVELENGTH_SAMPLES, true>;

//! @}
// =======================================================================

/**
 * \brief Abstract continuous spectral power distribution data type,
 * which supports evaluation at arbitrary wavelengths.
 *
 * \remark The term 'continuous' doesn't necessarily mean that the underlying
 * spectrum is continuous, but rather emphasizes the fact that it is a function
 * over the reals (as opposed to the discretely sampled spectrum, which only
 * stores samples at a finite set of wavelengths).
 */
class MTS_EXPORT_CORE ContinuousSpectrum : public Object {
public:
    /**
     * Evaluate the value of the spectral power distribution
     * at a set of wavelengths
     *
     * \param lambda
     *     List of wavelengths specified in nanometers
     */
    virtual DiscreteSpectrum eval(DiscreteSpectrum lambda) const = 0;

    /// Vectorized version of \ref eval()
    virtual DiscreteSpectrumP eval(DiscreteSpectrumP lambda) const = 0;

    /**
     * \brief Importance sample the spectral power distribution
     *
     * Not every implementation may provide this function; the default
     * implementation throws an exception.
     *
     * \param sample
     *     A uniform variate
     *
     * \return
     *     1. Set of sampled wavelengths specified in nanometers
     *     2. The Monte Carlo sampling weight (SPD value divided
     *        by the sampling density)
     *     3. Sample probability per unit wavelength (in units of 1/nm)
     */
    virtual std::tuple<DiscreteSpectrum, DiscreteSpectrum,
                       DiscreteSpectrum> sample(Float sample) const;

    /// Vectorized version of \ref sample()
    virtual std::tuple<DiscreteSpectrumP, DiscreteSpectrumP,
                       DiscreteSpectrumP> sample(FloatP sample) const;

    /**
     * \brief Return the probability distribution of the \ref sample() method
     * as a probability per unit wavelength (in nm).
     *
     * Not every implementation may provide this function; the default
     * implementation throws an exception.
     */
    virtual DiscreteSpectrum pdf(DiscreteSpectrum lambda) const;

    /// Vectorized version of \ref pdf()
    virtual DiscreteSpectrumP pdf(DiscreteSpectrumP lambda) const;

    /**
     * Return the integral over the spectrum over its support
     *
     * Not every implementation may provide this function; the default
     * implementation throws an exception.
     *
     * Even if the operation is provided, it may only return an approximation.
     */
    virtual Float integral() const;

    MTS_DECLARE_CLASS()

protected:
    virtual ~ContinuousSpectrum() = default;
};

/**
 * \brief Linear interpolant of a regularly sampled spectrum
 */
class MTS_EXPORT_CORE InterpolatedSpectrum : public ContinuousSpectrum {
public:
    InterpolatedSpectrum(Float lambda_min, Float lambda_max,
                         size_t size, const Float *data);

    // =======================================================================
    //! @{ \name Implementation of the ContinuousSpectrum interface
    // =======================================================================

    virtual DiscreteSpectrum eval(DiscreteSpectrum lambda) const override;

    virtual DiscreteSpectrumP eval(DiscreteSpectrumP lambda) const override;

    virtual std::tuple<DiscreteSpectrum, DiscreteSpectrum,
                       DiscreteSpectrum> sample(Float sample) const override;

    virtual std::tuple<DiscreteSpectrumP, DiscreteSpectrumP,
                       DiscreteSpectrumP> sample(FloatP sample) const override;

    virtual DiscreteSpectrum pdf(DiscreteSpectrum lambda) const override;

    virtual DiscreteSpectrumP pdf(DiscreteSpectrumP lambda) const override;

    virtual Float integral() const override;

    //! @}
    // =======================================================================

    MTS_DECLARE_CLASS()

protected:
    virtual ~InterpolatedSpectrum() = default;

    template <typename Value> auto eval(Value lambda) const;
    template <typename Value, typename Float> auto sample(Float sample) const;

private:
    std::vector<Float> m_data, m_cdf;
    uint32_t m_size_minus_2;
    Float m_lambda_min;
    Float m_lambda_max;
    Float m_interval_size;
    Float m_inv_interval_size;
    Float m_integral;
    Float m_normalization;
};

/// Table with fits for \ref cie1931_xyz and \ref cie1931_y
extern MTS_EXPORT_CORE const Float cie1931_data[7][4];

/**
 * \brief Compute the CIE 1931 XYZ color matching functions given a wavelength
 * in nanometers
 *
 * Based on "Simple Analytic Approximations to the CIE XYZ Color Matching
 * Functions" by Chris Wyman, Peter-Pike Sloan, and Peter Shirley
 * Journal of Computer Graphics Techniques Vol 2, No 2, 2013
 */
template <typename T, typename Expr = expr_t<T>>
std::tuple<Expr, Expr, Expr> cie1931_xyz(T lambda) {
    Expr result[7];

    for (int i=0; i<7; ++i) {
        /* Coefficients of Gaussian fits */
        Float alpha(cie1931_data[i][0]), beta (cie1931_data[i][1]),
              gamma(cie1931_data[i][2]), delta(cie1931_data[i][3]);

        Expr tmp = select(lambda < beta, Expr(gamma),
                                         Expr(delta)) * (lambda - beta);

        result[i] = alpha * exp(-.5f * tmp * tmp);
    }

    return { result[0] + result[1] + result[2],
             result[3] + result[4],
             result[5] + result[6] };
}

/**
 * \brief Compute the CIE 1931 Y color matching function given a wavelength
 * in nanometers
 *
 * Based on "Simple Analytic Approximations to the CIE XYZ Color Matching
 * Functions" by Chris Wyman, Peter-Pike Sloan, and Peter Shirley
 * Journal of Computer Graphics Techniques Vol 2, No 2, 2013
 */
template <typename T, typename Expr = expr_t<T>>
Expr cie1931_y(T lambda) {
    Expr result[2];

    for (int i=0; i<2; ++i) {
        /* Coefficients of Gaussian fits */
        int j = 3 + i;
        Float alpha(cie1931_data[j][0]), beta (cie1931_data[j][1]),
              gamma(cie1931_data[j][2]), delta(cie1931_data[j][3]);

        Expr tmp = select(lambda < beta, T(gamma),
                                         T(delta)) * (lambda - beta);

        result[i] = alpha * exp(-.5f * tmp * tmp);
    }

    return result[0] + result[1];
}

NAMESPACE_END(mitsuba)
