// -----------------------------------------------------------------------
//! @{ \name Macro for template implementation of endpoint methods
// -----------------------------------------------------------------------

#define MTS_IMPLEMENT_ENDPOINT_SCALAR()                                        \
    std::pair<Ray3f, Spectrumf> sample_ray(                                    \
        Float time, Float sample1, const Point2f &sample2,                     \
        const Point2f &sample3) const override {                               \
        ScopedPhase p(EProfilerPhase::EEndpointSampleRay);                     \
        return sample_ray_impl(time, sample1, sample2, sample3, true);         \
    }                                                                          \
    std::pair<DirectionSample3f, Spectrumf> sample_direction(                  \
        const Interaction3f &ref, const Point2f &sample) const override {      \
        ScopedPhase p(EProfilerPhase::EEndpointSampleDirection);               \
        return sample_direction_impl(ref, sample, true);                       \
    }                                                                          \
    Float pdf_direction(const Interaction3f &ref, const DirectionSample3f &ds) \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::EEndpointEvaluate);                      \
        return pdf_direction_impl(ref, ds, true);                              \
    }                                                                          \
    Spectrumf eval(const SurfaceInteraction3f &si) const override {            \
        ScopedPhase p(EProfilerPhase::EEndpointEvaluate);                      \
        return eval_impl(si, true);                                            \
    }

#define MTS_IMPLEMENT_ENDPOINT_PACKET()                                        \
    std::pair<Ray3fP, SpectrumfP> sample_ray(                                  \
        FloatP time, FloatP sample1, const Point2fP &sample2,                  \
        const Point2fP &sample3, MaskP active) const override {                \
        ScopedPhase p(EProfilerPhase::EEndpointSampleRayP);                    \
        return sample_ray_impl(time, sample1, sample2, sample3, active);       \
    }                                                                          \
    std::pair<DirectionSample3fP, SpectrumfP> sample_direction(                \
        const Interaction3fP &ref, const Point2fP &sample, MaskP active)       \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::EEndpointSampleDirectionP);              \
        return sample_direction_impl(ref, sample, active);                     \
    }                                                                          \
    FloatP pdf_direction(const Interaction3fP &ref,                            \
                         const DirectionSample3fP &ds, MaskP active)           \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::EEndpointEvaluateP);                     \
        return pdf_direction_impl(ref, ds, active);                            \
    }                                                                          \
    SpectrumfP eval(const SurfaceInteraction3fP &si, MaskP active)             \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::EEndpointEvaluateP);                     \
        return eval_impl(si, active);                                          \
    }

#if !defined(MTS_ENABLE_AUTODIFF)
#  define MTS_IMPLEMENT_ENDPOINT_AUTODIFF()
#else
#  define MTS_IMPLEMENT_ENDPOINT_AUTODIFF()                                    \
    std::pair<Ray3fD, SpectrumfD> sample_ray(                                  \
        FloatD time, FloatD sample1, const Point2fD &sample2,                  \
        const Point2fD &sample3, MaskD active) const override {                \
        ScopedPhase p(EProfilerPhase::EEndpointSampleRay);                     \
        return sample_ray_impl(time, sample1, sample2, sample3, active);       \
    }                                                                          \
    std::pair<DirectionSample3fD, SpectrumfD> sample_direction(                \
        const Interaction3fD &ref, const Point2fD &sample, MaskD active)       \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::EEndpointSampleDirection);               \
        return sample_direction_impl(ref, sample, active);                     \
    }                                                                          \
    FloatD pdf_direction(const Interaction3fD &ref,                            \
                         const DirectionSample3fD &ds, MaskD active)           \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::EEndpointEvaluate);                      \
        return pdf_direction_impl(ref, ds, active);                            \
    }                                                                          \
    SpectrumfD eval(const SurfaceInteraction3fD &si, MaskD active)             \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::EEndpointEvaluate);                      \
        return eval_impl(si, active);                                          \
    }
#endif

#define MTS_IMPLEMENT_ENDPOINT_ALL()                                           \
    MTS_IMPLEMENT_ENDPOINT_SCALAR()                                            \
    MTS_IMPLEMENT_ENDPOINT_PACKET()                                            \
    MTS_IMPLEMENT_ENDPOINT_AUTODIFF()

#define MTS_IMPLEMENT_ENDPOINT_POLARIZED_SCALAR()                              \
    std::pair<Ray3f, MuellerMatrixSf> sample_ray_pol(                          \
        Float time, Float sample1, const Point2f &sample2,                     \
        const Point2f &sample3) const override {                               \
        ScopedPhase p(EProfilerPhase::EEndpointSampleRay);                     \
        return sample_ray_pol_impl(time, sample1, sample2, sample3, true);     \
    }                                                                          \
    std::pair<DirectionSample3f, MuellerMatrixSf> sample_direction_pol(        \
        const Interaction3f &it, const Point2f &sample) const override {       \
        ScopedPhase p(EProfilerPhase::EEndpointSampleDirection);               \
        return sample_direction_pol_impl(it, sample, true);                    \
    }                                                                          \
    MuellerMatrixSf eval_pol(const SurfaceInteraction3f &si) const override {  \
        ScopedPhase p(EProfilerPhase::EEndpointEvaluate);                      \
        return eval_pol_impl(si, true);                                        \
    }

#define MTS_IMPLEMENT_ENDPOINT_POLARIZED_PACKET()                              \
    std::pair<Ray3fP, MuellerMatrixSfP> sample_ray_pol(                        \
        FloatP time, FloatP sample1, const Point2fP &sample2,                  \
        const Point2fP &sample3, MaskP active) const override {                \
        ScopedPhase p(EProfilerPhase::EEndpointSampleRay);                     \
        return sample_ray_pol_impl(time, sample1, sample2, sample3, active);   \
    }                                                                          \
    std::pair<DirectionSample3fP, MuellerMatrixSfP> sample_direction_pol(      \
        const Interaction3fP &it, const Point2fP &sample, MaskP active)        \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::EEndpointSampleDirection);               \
        return sample_direction_pol_impl(it, sample, active);                  \
    }                                                                          \
    MuellerMatrixSfP eval_pol(const SurfaceInteraction3fP &si, MaskP active)   \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::EEndpointEvaluateP);                     \
        return eval_pol_impl(si, active);                                      \
    }

#if !defined(MTS_ENABLE_AUTODIFF)
#  define MTS_IMPLEMENT_ENDPOINT_POLARIZED_AUTODIFF()
#else
#  define MTS_IMPLEMENT_ENDPOINT_POLARIZED_AUTODIFF()                          \
    std::pair<Ray3fD, MuellerMatrixSfD> sample_ray_pol(                        \
        FloatP time, FloatP sample1, const Point2fD &sample2,                  \
        const Point2fD &sample3, MaskD active) const override {                \
        ScopedPhase p(EProfilerPhase::EEndpointSampleRay);                     \
        return sample_ray_pol_impl(time, sample1, sample2, sample3, active);   \
    }                                                                          \
    std::pair<DirectionSample3fD, MuellerMatrixSfD> sample_direction_pol(      \
        const Interaction3fD &it, const Point2fD &sample, MaskD active)        \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::EEndpointSampleDirection);               \
        return sample_direction_pol_impl(it, sample, active);                  \
    }                                                                          \
    MuellerMatrixSfD eval_pol(const SurfaceInteraction3fD &si, MaskD active)   \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::EEndpointEvaluate);                      \
        return eval_pol_impl(si, active);                                      \
    }
#endif

#define MTS_IMPLEMENT_ENDPOINT_POLARIZED_ALL()                                 \
    MTS_IMPLEMENT_ENDPOINT_POLARIZED_SCALAR()                                  \
    MTS_IMPLEMENT_ENDPOINT_POLARIZED_PACKET()                                  \
    MTS_IMPLEMENT_ENDPOINT_POLARIZED_AUTODIFF()

//! @}
// -----------------------------------------------------------------------
