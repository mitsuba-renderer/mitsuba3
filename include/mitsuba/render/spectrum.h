#pragma once

#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/render/fwd.h>

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
class MTS_EXPORT_RENDER ContinuousSpectrum : public Object {
public:
    /**
     * Evaluate the value of the spectral power distribution
     * at a set of wavelengths
     *
     * \param wavelengths
     *     List of wavelengths specified in nanometers
     */
    virtual Spectrumf eval(const Spectrumf &wavelengths) const;

    /// Vectorized version of \ref eval()
    virtual SpectrumfP eval(const SpectrumfP &wavelengths,
                            MaskP active = true) const;

    /// Wrapper for scalar \ref eval() with a mask (which will be ignored)
    Spectrumf eval(const Spectrumf &wavelengths, bool /* active */) const {
        return eval(wavelengths);
    }

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
     *        distribution value divided by the sampling density)
     */
    virtual std::pair<Spectrumf, Spectrumf>
    sample(const Spectrumf &sample) const;

    /// Vectorized version of \ref sample()
    virtual std::pair<SpectrumfP, SpectrumfP>
    sample(const SpectrumfP &sample, MaskP active = true) const;

    /// Wrapper for scalar \ref sample() with a mask (which will be ignored)
    std::pair<Spectrumf, Spectrumf>
    sample(const Spectrumf &s, bool /* active */) const {
        return sample(s);
    }

    /**
     * \brief Return the probability distribution of the \ref sample() method
     * as a probability per unit wavelength (in units of 1/nm).
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     */
    virtual Spectrumf pdf(const Spectrumf &wavelengths) const;

    /// Vectorized version of \ref pdf()
    virtual SpectrumfP pdf(const SpectrumfP &wavelengths,
                           MaskP active = true) const;

    /// Wrapper for scalar \ref pdf() with a mask (which will be ignored)
    Spectrumf pdf(const Spectrumf &wavelengths, bool /* active */) const {
        return pdf(wavelengths);
    }

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
    static ref<ContinuousSpectrum> D65(Float scale = 1.f);

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

    /// Vectorized version of \ref eval()
    virtual SpectrumfP eval(const SurfaceInteraction3fP &si,
                            MaskP active = true) const;

    /// Wrapper for scalar \ref eval() with a mask (which will be ignored)
    Spectrumf eval(const SurfaceInteraction3f &si, bool /* active */) const {
        return eval(si);
    }

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

    /// Vectorized version of \ref sample()
    virtual std::pair<SpectrumfP, SpectrumfP>
    sample(const SurfaceInteraction3fP &si, const SpectrumfP &sample, MaskP active = true) const;

    /// Wrapper for scalar \ref sample() with a mask (which will be ignored)
    std::pair<Spectrumf, Spectrumf>
    sample(const SurfaceInteraction3f &si, const Spectrumf &sample_, bool /* active */) const {
        return sample(si, sample_);
    }

    /**
     * \brief Return the probability distribution of the \ref sample() method
     * as a probability per unit wavelength (in units of 1/nm).
     *
     * Not every implementation necessarily provides this function. The default
     * implementation throws an exception.
     */
    virtual Spectrumf pdf(const SurfaceInteraction3f &si) const;

    /// Vectorized version of \ref pdf()
    virtual SpectrumfP pdf(const SurfaceInteraction3fP &si,
                           MaskP active = true) const;

    /// Wrapper for scalar \ref pdf() with a mask (which will be ignored)
    Spectrumf pdf(const SurfaceInteraction3f &si, bool /* active */) const {
        return pdf(si);
    }

    //! @}
    // ======================================================================

    MTS_DECLARE_CLASS()

protected:
    virtual ~ContinuousSpectrum();
};

/*
 * \brief These macros should be used in the definition of Spectrum
 * plugins to instantiate concrete versions of the \c sample,
 * \c eval and \c pdf functions.
 */
#define MTS_IMPLEMENT_SPECTRUM()                                               \
    using ContinuousSpectrum::eval;                                            \
    using ContinuousSpectrum::pdf;                                             \
    using ContinuousSpectrum::sample;                                          \
    Spectrumf eval(const Spectrumf &wavelengths) const override {              \
        return eval_impl(wavelengths, true);                                   \
    }                                                                          \
    SpectrumfP eval(const SpectrumfP &wavelengths, MaskP active)               \
        const override {                                                       \
        return eval_impl(wavelengths, active);                                 \
    }                                                                          \
    Spectrumf pdf(const Spectrumf &wavelengths) const override {               \
        return pdf_impl(wavelengths, true);                                    \
    }                                                                          \
    SpectrumfP pdf(const SpectrumfP &wavelengths, MaskP active)                \
        const override {                                                       \
        return pdf_impl(wavelengths, active);                                  \
    }                                                                          \
    std::pair<Spectrumf, Spectrumf> sample(const Spectrumf &sample)            \
        const override {                                                       \
        return sample_impl(sample, true);                                      \
    }                                                                          \
    std::pair<SpectrumfP, SpectrumfP> sample(const SpectrumfP &sample,         \
                                             MaskP active) const override {    \
        return sample_impl(sample, active);                                    \
    }                                                                          \
    Spectrumf eval(const SurfaceInteraction3f &si) const override {            \
        ScopedPhase p(EProfilerPhase::ESpectrumEval);                          \
        return eval_impl(si.wavelengths, true);                                \
    }                                                                          \
    SpectrumfP eval(const SurfaceInteraction3fP &si, MaskP active)             \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::ESpectrumEvalP);                         \
        return eval_impl(si.wavelengths, active);                              \
    }

#define MTS_IMPLEMENT_TEXTURE()                                                \
    using ContinuousSpectrum::eval;                                            \
    Spectrumf eval(const SurfaceInteraction3f &si) const override {            \
        ScopedPhase p(EProfilerPhase::ESpectrumEval);                          \
        return eval_impl(si, true);                                            \
    }                                                                          \
    SpectrumfP eval(const SurfaceInteraction3fP &si, MaskP active)             \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::ESpectrumEvalP);                         \
        return eval_impl(si, active);                                          \
    }

NAMESPACE_END(mitsuba)
