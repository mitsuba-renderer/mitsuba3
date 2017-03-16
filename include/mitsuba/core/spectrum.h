#pragma once

#include <mitsuba/core/simd.h>
#include <mitsuba/core/object.h>

NAMESPACE_BEGIN(mitsuba)

#if !defined(MTS_WAVELENGTH_SAMPLES)
#  define MTS_WAVELENGTH_SAMPLES 8
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
     * Not all implementations provide this function; the default
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
     * Not all implementations provide this function; the default
     * implementation throws an exception.
     */
    virtual DiscreteSpectrum pdf(DiscreteSpectrum lambda) const;

    /// Vectorized version of \ref pdf()
    virtual DiscreteSpectrumP pdf(DiscreteSpectrumP lambda) const;

    MTS_DECLARE_CLASS()
};

NAMESPACE_END(mitsuba)
