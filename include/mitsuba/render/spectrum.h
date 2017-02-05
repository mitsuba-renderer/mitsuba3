#pragma once

#include <mitsuba/core/vector.h>

#define SPECTRUM_MIN_WAVELENGTH   360
#define SPECTRUM_MAX_WAVELENGTH   830
#define SPECTRUM_RANGE            (SPECTRUM_MAX_WAVELENGTH - SPECTRUM_MIN_WAVELENGTH)

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Abstract continuous spectral power distribution data type,
 * which supports evaluation at arbitrary wavelengths.
 *
 * \remark The term 'continuous' doesn't necessarily mean that the underlying
 * spectrum is continuous, but rather emphasizes the fact that it is a function
 * over the reals (as opposed to the discretely sampled spectrum, which only
 * stores samples at a finite set of wavelengths).
 */
class MTS_EXPORT_RENDER ContinuousSpectrum : public Object {
public:
    /**
     * Evaluate the value of the spectral power distribution
     * at a set of wavelengths.
     *
     * \param lambda
     *     Packet with wavelengths in nanometers
     */
    virtual FloatPacket eval(FloatPacket lambda) const = 0;

    /**
     * Importance sample the spectral power distribution
     *
     * Not all implementations may provide this function; the default
     * implementation throws an exception.
     *
     * \param sample
     *     Packet of uniform variates
     *
     * \param[out] lambda
     *     Sampled wavelengths in nanometers

     * \param[out] pdf
     *     Sample probability per unit wavelength (in units of 1/nm)
     *
     * \return
     *     The Monte Carlo sampling weight (SPD value divided
     *     by the sampling density)
     */
    virtual FloatPacket sample(FloatPacket sample,
                               FloatPacket &lambda,
                               FloatPacket &pdf) const;

    /**
     * \brief Return the probability distribution of the \ref sample() method
     * as a probability per unit wavelength
     */
    virtual FloatPacket pdf(FloatPacket lambda) const;

    MTS_DECLARE_CLASS()
};

NAMESPACE_END(mitsuba)
