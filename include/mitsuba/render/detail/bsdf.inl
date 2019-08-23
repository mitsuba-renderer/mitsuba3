// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for dynamic vectorization
// -----------------------------------------------------------------------

ENOKI_STRUCT_SUPPORT(mitsuba::BSDFSample, wo, pdf, eta,
                     sampled_type, sampled_component)

//! @}
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
//! @{ \name Enoki support for vectorized function calls
// -----------------------------------------------------------------------

ENOKI_CALL_SUPPORT_BEGIN(mitsuba::BSDF)
    ENOKI_CALL_SUPPORT_METHOD(sample)
    ENOKI_CALL_SUPPORT_METHOD(eval)
    ENOKI_CALL_SUPPORT_METHOD(pdf)
    ENOKI_CALL_SUPPORT_METHOD(eval_transmission)
    ENOKI_CALL_SUPPORT_METHOD(sample_pol)
    ENOKI_CALL_SUPPORT_METHOD(eval_pol)
    ENOKI_CALL_SUPPORT_METHOD(eval_transmission_pol)
    ENOKI_CALL_SUPPORT_GETTER(flags, m_flags)

    auto needs_differentials() const {
        return neq(flags() & mitsuba::BSDF::ENeedsDifferentials, 0);
    }
ENOKI_CALL_SUPPORT_END(mitsuba::BSDF)

//! @}
// -----------------------------------------------------------------------

/*
 * \brief This macro should be used in the definition of BSDF
 * plugins to instantiate concrete versions of the \c sample,
 * \c eval and \c pdf functions.
 */
#define MTS_IMPLEMENT_BSDF_SCALAR()                                            \
    using BSDF::sample;                                                        \
    using BSDF::eval;                                                          \
    using BSDF::pdf;                                                           \
    std::pair<BSDFSample3f, Spectrumf> sample(                                 \
            const BSDFContext &ctx, const SurfaceInteraction3f &si,            \
            Float sample1, const Point2f &sample2) const override {            \
        ScopedPhase p(EProfilerPhase::EBSDFSample);                            \
        return sample_impl(ctx, si, sample1, sample2, true);                   \
    }                                                                          \
    Spectrumf eval(const BSDFContext &ctx, const SurfaceInteraction3f &si,     \
                   const Vector3f &wo) const override {                        \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluate);                          \
        return eval_impl(ctx, si, wo, true);                                   \
    }                                                                          \
    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si,          \
              const Vector3f &wo) const override {                             \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluate);                          \
        return pdf_impl(ctx, si, wo, true);                                    \
    }

#define MTS_IMPLEMENT_BSDF_PACKET()                                            \
    std::pair<BSDFSample3fP, SpectrumfP> sample(                               \
            const BSDFContext &ctx, const SurfaceInteraction3fP &si,           \
            FloatP sample1, const Point2fP &sample2,                           \
            MaskP active = true) const override {                              \
        ScopedPhase p(EProfilerPhase::EBSDFSampleP);                           \
        return sample_impl(ctx, si, sample1, sample2, active);                 \
    }                                                                          \
    SpectrumfP eval(const BSDFContext &ctx, const SurfaceInteraction3fP &si,   \
                    const Vector3fP &wo, MaskP active) const override {        \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluateP);                         \
        return eval_impl(ctx, si, wo, active);                                 \
    }                                                                          \
    FloatP pdf(const BSDFContext &ctx, const SurfaceInteraction3fP &si,        \
               const Vector3fP &wo, MaskP active) const override {             \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluateP);                         \
        return pdf_impl(ctx, si, wo, active);                                  \
    }

#if !defined(MTS_ENABLE_AUTODIFF)
#  define MTS_IMPLEMENT_BSDF_AUTODIFF()
#else
#  define MTS_IMPLEMENT_BSDF_AUTODIFF()                                        \
    std::pair<BSDFSample3fD, SpectrumfD> sample(                               \
            const BSDFContext &ctx, const SurfaceInteraction3fD &si,           \
            FloatD sample1, const Point2fD &sample2,                           \
            MaskD active = true) const override {                              \
        ScopedPhase p(EProfilerPhase::EBSDFSample);                            \
        return sample_impl(ctx, si, sample1, sample2, active);                 \
    }                                                                          \
    SpectrumfD eval(const BSDFContext &ctx, const SurfaceInteraction3fD &si,   \
                    const Vector3fD &wo, MaskD active) const override {        \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluate);                          \
        return eval_impl(ctx, si, wo, active);                                 \
    }                                                                          \
    FloatD pdf(const BSDFContext &ctx, const SurfaceInteraction3fD &si,        \
               const Vector3fD &wo, MaskD active) const override {             \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluate);                          \
        return pdf_impl(ctx, si, wo, active);                                  \
    }
#endif

/// Instantiates concrete scalar and packet versions of the polarized BSDF plugin API
#define MTS_IMPLEMENT_BSDF_POLARIZED_SCALAR()                                  \
    std::pair<BSDFSample3f, MuellerMatrixSf> sample_pol(                       \
        const BSDFContext &ctx, const SurfaceInteraction3f &si, Float sample1, \
        const Point2f &sample2) const override {                               \
        ScopedPhase p(EProfilerPhase::EBSDFSample);                            \
        return sample_pol_impl(ctx, si, sample1, sample2, true);               \
    }                                                                          \
    MuellerMatrixSf eval_pol(const BSDFContext &ctx,                           \
                             const SurfaceInteraction3f &si,                   \
                             const Vector3f &wo) const override {              \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluate);                          \
        return eval_pol_impl(ctx, si, wo, true);                               \
    }

#define MTS_IMPLEMENT_BSDF_POLARIZED_PACKET()                                  \
    std::pair<BSDFSample3fP, MuellerMatrixSfP> sample_pol(                     \
        const BSDFContext &ctx, const SurfaceInteraction3fP &si,               \
        FloatP sample1, const Point2fP &sample2, MaskP active = true)          \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::EBSDFSampleP);                           \
        return sample_pol_impl(ctx, si, sample1, sample2, active);             \
    }                                                                          \
    MuellerMatrixSfP eval_pol(                                                 \
        const BSDFContext &ctx, const SurfaceInteraction3fP &si,               \
        const Vector3fP &wo, MaskP active) const override {                    \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluateP);                         \
        return eval_pol_impl(ctx, si, wo, active);                             \
    }

#if !defined(MTS_ENABLE_AUTODIFF)
#  define MTS_IMPLEMENT_BSDF_POLARIZED_AUTODIFF()
#else
#  define MTS_IMPLEMENT_BSDF_POLARIZED_AUTODIFF()                              \
    std::pair<BSDFSample3fD, MuellerMatrixSfD> sample_pol(                     \
        const BSDFContext &ctx, const SurfaceInteraction3fD &si,               \
        FloatD sample1, const Point2fD &sample2, MaskD active = true)          \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::EBSDFSample);                            \
        return sample_pol_impl(ctx, si, sample1, sample2, active);             \
    }                                                                          \
    MuellerMatrixSfD eval_pol(                                                 \
        const BSDFContext &ctx, const SurfaceInteraction3fD &si,               \
        const Vector3fD &wo, MaskD active) const override {                    \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluate);                          \
        return eval_pol_impl(ctx, si, wo, active);                             \
    }
#endif

#define MTS_IMPLEMENT_BSDF_ALL()                                               \
    MTS_IMPLEMENT_BSDF_SCALAR()                                                \
    MTS_IMPLEMENT_BSDF_PACKET()                                                \
    MTS_IMPLEMENT_BSDF_AUTODIFF()

#define MTS_IMPLEMENT_BSDF_POLARIZED_ALL()                                     \
    MTS_IMPLEMENT_BSDF_POLARIZED_SCALAR()                                      \
    MTS_IMPLEMENT_BSDF_POLARIZED_PACKET()                                      \
    MTS_IMPLEMENT_BSDF_POLARIZED_AUTODIFF()

#define MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_SCALAR()                          \
    Spectrumf eval_transmission(const SurfaceInteraction3f &si,                \
                                const Vector3f &wo) const override {           \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluate);                          \
        return eval_transmission_impl(si, wo, true);                           \
    }

#define MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_PACKET()                          \
    SpectrumfP eval_transmission(const SurfaceInteraction3fP &si,              \
                                 const Vector3fP &wo,                          \
                                 MaskP active) const override {                \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluateP);                         \
        return eval_transmission_impl(si, wo, active);                         \
    }

#if !defined(MTS_ENABLE_AUTODIFF)
#  define MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_AUTODIFF()
#else
#  define MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_AUTODIFF()                      \
    SpectrumfD eval_transmission(const SurfaceInteraction3fD &si,              \
                                 const Vector3fD &wo,                          \
                                 MaskD active) const override {                \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluate);                          \
        return eval_transmission_impl(si, wo, active);                         \
    }
#endif

#define MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_ALL()                             \
    MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_SCALAR()                              \
    MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_PACKET()                              \
    MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_AUTODIFF()

#define MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_POLARIZED_SCALAR()                \
    MuellerMatrixSf eval_transmission_pol(const SurfaceInteraction3f &si,      \
                                          const Vector3f &wo) const override { \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluate);                          \
        return eval_transmission_pol_impl(si, wo, true);                       \
    }

#define MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_POLARIZED_PACKET()                \
    MuellerMatrixSfP eval_transmission_pol(const SurfaceInteraction3fP &si,    \
                                           const Vector3fP &wo,                \
                                           MaskP active) const override {      \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluateP);                         \
        return eval_transmission_pol_impl(si, wo, active);                     \
    }

#if !defined(MTS_ENABLE_AUTODIFF)
#  define MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_POLARIZED_AUTODIFF()
#else
#  define MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_POLARIZED_AUTODIFF()            \
    MuellerMatrixSfD eval_transmission_pol(const SurfaceInteraction3fD &si,    \
                                     const Vector3fD &wo,                      \
                                     MaskD active) const override {            \
        ScopedPhase p(EProfilerPhase::EBSDFEvaluate);                          \
        return eval_transmission_pol_impl(si, wo, active);                     \
    }
#endif

#define MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_POLARIZED_ALL()                   \
    MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_POLARIZED_SCALAR()                    \
    MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_POLARIZED_PACKET()                    \
    MTS_IMPLEMENT_BSDF_EVAL_TRANSMISSION_POLARIZED_AUTODIFF()