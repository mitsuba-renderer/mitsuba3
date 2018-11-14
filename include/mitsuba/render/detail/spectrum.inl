/*
 * \brief These macros should be used in the definition of Spectrum
 * plugins to instantiate concrete versions of the \c sample,
 * \c eval and \c pdf functions.
 */
#define MTS_IMPLEMENT_SPECTRUM_EVAL_SCALAR()                                   \
    Spectrumf eval(const Spectrumf &wavelengths) const override {              \
        return eval_impl(wavelengths, true);                                   \
    }                                                                          \

#define MTS_IMPLEMENT_SPECTRUM_EVAL_PACKET()                                   \
    SpectrumfP eval(const SpectrumfP &wavelengths, MaskP active)               \
        const override {                                                       \
        return eval_impl(wavelengths, active);                                 \
    }

#if !defined(MTS_ENABLE_AUTODIFF)
#  define MTS_IMPLEMENT_SPECTRUM_EVAL_AUTODIFF()
#else
#  define MTS_IMPLEMENT_SPECTRUM_EVAL_AUTODIFF()                               \
    SpectrumfD eval(const SpectrumfD &wavelengths, MaskD active)               \
        const override {                                                       \
        return eval_impl(wavelengths, active);                                 \
    }
#endif

#define MTS_IMPLEMENT_SPECTRUM_SCALAR()                                        \
    MTS_IMPLEMENT_SPECTRUM_EVAL_SCALAR()                                       \
    using ContinuousSpectrum::eval;                                            \
    using ContinuousSpectrum::pdf;                                             \
    using ContinuousSpectrum::sample;                                          \
    Spectrumf pdf(const Spectrumf &wavelengths) const override {               \
        return pdf_impl(wavelengths, true);                                    \
    }                                                                          \
    std::pair<Spectrumf, Spectrumf> sample(const Spectrumf &sample)            \
        const override {                                                       \
        return sample_impl(sample, true);                                      \
    }                                                                          \
    Spectrumf eval(const SurfaceInteraction3f &si) const override {            \
        ScopedPhase p(EProfilerPhase::ESpectrumEval);                          \
        return eval_impl(si.wavelengths, true);                                \
    }

#define MTS_IMPLEMENT_SPECTRUM_PACKET()                                        \
    MTS_IMPLEMENT_SPECTRUM_EVAL_PACKET()                                       \
    SpectrumfP pdf(const SpectrumfP &wavelengths, MaskP active)                \
        const override {                                                       \
        return pdf_impl(wavelengths, active);                                  \
    }                                                                          \
    std::pair<SpectrumfP, SpectrumfP> sample(const SpectrumfP &sample,         \
                                             MaskP active) const override {    \
        return sample_impl(sample, active);                                    \
    }                                                                          \
    SpectrumfP eval(const SurfaceInteraction3fP &si, MaskP active)             \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::ESpectrumEvalP);                         \
        return eval_impl(si.wavelengths, active);                              \
    }

#if !defined(MTS_ENABLE_AUTODIFF)
#  define MTS_IMPLEMENT_SPECTRUM_AUTODIFF()
#else
#  define MTS_IMPLEMENT_SPECTRUM_AUTODIFF()                                    \
    MTS_IMPLEMENT_SPECTRUM_EVAL_AUTODIFF()                                     \
    SpectrumfD pdf(const SpectrumfD &wavelengths, MaskD active)                \
        const override {                                                       \
        return pdf_impl(wavelengths, active);                                  \
    }                                                                          \
    std::pair<SpectrumfD, SpectrumfD> sample(const SpectrumfD &sample,         \
                                             MaskD active) const override {    \
        return sample_impl(sample, active);                                    \
    }                                                                          \
    SpectrumfD eval(const SurfaceInteraction3fD &si, MaskD active)             \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::ESpectrumEval);                          \
        return eval_impl(si.wavelengths, active);                              \
    }
#endif

#define MTS_IMPLEMENT_TEXTURE_SCALAR()                                         \
    using ContinuousSpectrum::eval;                                            \
    Spectrumf eval(const SurfaceInteraction3f &si) const override {            \
        ScopedPhase p(EProfilerPhase::ESpectrumEval);                          \
        return eval_impl(si, true);                                            \
    }

#define MTS_IMPLEMENT_TEXTURE_PACKET()                                         \
    SpectrumfP eval(const SurfaceInteraction3fP &si, MaskP active)             \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::ESpectrumEvalP);                         \
        return eval_impl(si, active);                                          \
    }                                                                          \

#if !defined(MTS_ENABLE_AUTODIFF)
#  define MTS_IMPLEMENT_TEXTURE_AUTODIFF()
#else
#  define MTS_IMPLEMENT_TEXTURE_AUTODIFF()                                     \
    SpectrumfD eval(const SurfaceInteraction3fD &si, MaskD active)             \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::ESpectrumEval);                          \
        return eval_impl(si, active);                                          \
    }
#endif

#define MTS_IMPLEMENT_SPECTRUM_EVAL_ALL()                                      \
    MTS_IMPLEMENT_SPECTRUM_EVAL_SCALAR()                                       \
    MTS_IMPLEMENT_SPECTRUM_EVAL_PACKET()                                       \
    MTS_IMPLEMENT_SPECTRUM_EVAL_AUTODIFF()

#define MTS_IMPLEMENT_SPECTRUM_ALL()                                           \
    MTS_IMPLEMENT_SPECTRUM_SCALAR()                                            \
    MTS_IMPLEMENT_SPECTRUM_PACKET()                                            \
    MTS_IMPLEMENT_SPECTRUM_AUTODIFF()

#define MTS_IMPLEMENT_TEXTURE_ALL()                                            \
    MTS_IMPLEMENT_TEXTURE_SCALAR()                                             \
    MTS_IMPLEMENT_TEXTURE_PACKET()                                             \
    MTS_IMPLEMENT_TEXTURE_AUTODIFF()
