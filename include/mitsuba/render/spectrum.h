#pragma once

#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Abstract continuous spectral power distribution data type,
 * which supports evaluation at arbitrary wavelengths.
 *
 * \remark The term 'continuous' does not imply that the underlying spectrum
 * must be continuous, but rather emphasizes that it is a function defined on
 * the set of real numbers (as opposed to the discretely sampled spectrum,
 * which only stores samples at a finite set of wavelengths).
 *
 * A continuous spectrum can also be vary with respect to a spatial position.
 * The (optional) texture interface at the bottom can be implemented to support
 * this. The default implementation strips the position information and falls
 * back the non-textured implementation.
 */
template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER ContinuousSpectrum : public Object {
public:
    MTS_IMPORT_TYPES();

    /**
     * Evaluate the value of the spectral power distribution
     * at a set of wavelengths
     *
     * \param wavelengths
     *     List of wavelengths specified in nanometers
     */
    virtual Spectrum eval(const Wavelength &wavelengths, Mask active = true) const;

    /**
     * \brief Importance sample the spectral power distribution
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     *
     * \param sample
     *     A uniform variate for each desired wavelength.
     *
     * \return
     *     1. Set of sampled wavelengths specified in nanometers
     *
     *     2. The Monte Carlo importance weight (Spectral power
     *        density value divided by the sampling density)
     */
    virtual std::pair<Wavelength, Spectrum>
    sample(const Wavelength &sample, Mask active = true) const;

    /**
     * \brief Return the probability distribution of the \ref sample() method
     * as a probability per unit wavelength (in units of 1/nm).
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     */
    virtual Spectrum pdf(const Wavelength &wavelengths, Mask active = true) const;

    /**
     * Return the mean value of the spectrum over the support
     * (MTS_WAVELENGTH_MIN..MTS_WAVELENGTH_MAX)
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     *
     * Even if the operation is provided, it may only return an approximation.
     */
    virtual Float mean() const;

    /**
     * Convenience method returning the standard D65 illuminant.
     */
    static ref<ContinuousSpectrum> D65(float scale = 1.f, bool monochrome = false);

    // ======================================================================
    //! @{ \name Texture interface implementation
    //!
    //! The texture interface maps a given surface position and set of
    //! wavelengths to spectral reflectance values in the [0, 1] range.
    //!
    //! The default implementations simply ignore the spatial information
    //! and fall back to the above non-textured implementations.
    // ======================================================================

    /// Evaluate the texture at the given surface interaction
    virtual Spectrum eval(const SurfaceInteraction3f &si, Mask active = true) const;

    /**
     * \brief Importance sample the (textured) spectral power distribution
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     *
     * \param si
     *     An interaction record describing the associated surface position
     *
     * \param sample
     *     A uniform variate for each desired wavelength.
     *
     * \return
     *     1. Set of sampled wavelengths specified in nanometers
     *
     *     2. The Monte Carlo importance weight (Spectral power
     *        distribution value divided by the sampling density)
     */
    virtual std::pair<Wavelength, Spectrum>
    sample(const SurfaceInteraction3f &si, const Spectrum &sample, Mask active = true) const;

    /**
     * \brief Return the probability distribution of the \ref sample() method
     * as a probability per unit wavelength (in units of 1/nm).
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     */
    virtual Spectrum pdf(const SurfaceInteraction3f &si, Mask active = true) const;

    //! @}
    // ======================================================================

    MTS_DECLARE_CLASS()

protected:
    virtual ~ContinuousSpectrum();
};

NAMESPACE_END(mitsuba)

#include "detail/spectrum.inl"
