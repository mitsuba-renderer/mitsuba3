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
class MTS_EXPORT_RENDER ContinuousSpectrum : public DifferentiableObject {
public:
    /**
     * Evaluate the value of the spectral power distribution
     * at a set of wavelengths
     *
     * \param wavelengths
     *     List of wavelengths specified in nanometers
     */
    virtual Spectrumf eval(const Spectrumf &wavelengths) const;

    /// Wrapper for scalar \ref eval() with a mask (which will be ignored)
    Spectrumf eval(const Spectrumf &wavelengths, bool /* active */) const {
        return eval(wavelengths);
    }

    /// Vectorized version of \ref eval()
    virtual SpectrumfP eval(const SpectrumfP &wavelengths,
                            MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref eval()
    virtual SpectrumfD eval(const SpectrumfD &wavelengths,
                            MaskD active = true) const;
#endif

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
    virtual std::pair<Spectrumf, Spectrumf>
    sample(const Spectrumf &sample) const;

    /// Wrapper for scalar \ref sample() with a mask (which will be ignored)
    std::pair<Spectrumf, Spectrumf>
    sample(const Spectrumf &s, bool /* active */) const {
        return sample(s);
    }

    /// Vectorized version of \ref sample()
    virtual std::pair<SpectrumfP, SpectrumfP>
    sample(const SpectrumfP &sample, MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref sample()
    virtual std::pair<SpectrumfD, SpectrumfD>
    sample(const SpectrumfD &sample, MaskD active = true) const;
#endif

    /**
     * \brief Return the probability distribution of the \ref sample() method
     * as a probability per unit wavelength (in units of 1/nm).
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     */
    virtual Spectrumf pdf(const Spectrumf &wavelengths) const;

    /// Wrapper for scalar \ref pdf() with a mask (which will be ignored)
    Spectrumf pdf(const Spectrumf &wavelengths, bool /* active */) const {
        return pdf(wavelengths);
    }

    /// Vectorized version of \ref pdf()
    virtual SpectrumfP pdf(const SpectrumfP &wavelengths,
                           MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref pdf()
    virtual SpectrumfD pdf(const SpectrumfD &wavelengths,
                           MaskD active = true) const;
#endif

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
    static ref<ContinuousSpectrum> D65(Float scale = 1.f, bool monochrome = false);

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
    virtual Spectrumf eval(const SurfaceInteraction3f &si) const;

    /// Wrapper for scalar \ref eval() with a mask (which will be ignored)
    Spectrumf eval(const SurfaceInteraction3f &si, bool /* active */) const {
        return eval(si);
    }

    /// Vectorized version of \ref eval()
    virtual SpectrumfP eval(const SurfaceInteraction3fP &si,
                            MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref eval()
    virtual SpectrumfD eval(const SurfaceInteraction3fD &si,
                            MaskD active = true) const;
#endif

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
    virtual std::pair<Spectrumf, Spectrumf>
    sample(const SurfaceInteraction3f &si, const Spectrumf &sample) const;

    /// Wrapper for scalar \ref sample() with a mask (which will be ignored)
    std::pair<Spectrumf, Spectrumf>
    sample(const SurfaceInteraction3f &si, const Spectrumf &sample_, bool /* active */) const {
        return sample(si, sample_);
    }

    /// Vectorized version of \ref sample()
    virtual std::pair<SpectrumfP, SpectrumfP>
    sample(const SurfaceInteraction3fP &si, const SpectrumfP &sample,
           MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref sample()
    virtual std::pair<SpectrumfD, SpectrumfD>
    sample(const SurfaceInteraction3fD &si, const SpectrumfD &sample,
           MaskD active = true) const;
#endif

    /**
     * \brief Return the probability distribution of the \ref sample() method
     * as a probability per unit wavelength (in units of 1/nm).
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     */
    virtual Spectrumf pdf(const SurfaceInteraction3f &si) const;

    /// Wrapper for scalar \ref pdf() with a mask (which will be ignored)
    Spectrumf pdf(const SurfaceInteraction3f &si, bool /* active */) const {
        return pdf(si);
    }

    /// Vectorized version of \ref pdf()
    virtual SpectrumfP pdf(const SurfaceInteraction3fP &si,
                           MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref pdf()
    virtual SpectrumfD pdf(const SurfaceInteraction3fD &si,
                           MaskD active = true) const;
#endif

    //! @}
    // ======================================================================

    MTS_DECLARE_CLASS()

protected:
    virtual ~ContinuousSpectrum();
};

NAMESPACE_END(mitsuba)

#include "detail/spectrum.inl"
