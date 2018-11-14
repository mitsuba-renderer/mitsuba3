
/// Instantiates concrete scalar and packet versions of the sensor plugin API
#define MTS_IMPLEMENT_SENSOR_SCALAR()                                          \
    MTS_IMPLEMENT_ENDPOINT_SCALAR()                                            \
    std::pair<RayDifferential3f, Spectrumf> sample_ray_differential(           \
        Float time, Float sample1, const Point2f &sample2,                     \
        const Point2f &sample3) const override {                               \
        ScopedPhase p(EProfilerPhase::EEndpointSampleRay);                     \
        return sample_ray_differential_impl(time, sample1, sample2, sample3,   \
                                            true);                             \
    }                                                                          \

#define MTS_IMPLEMENT_SENSOR_PACKET()                                          \
    MTS_IMPLEMENT_ENDPOINT_PACKET()                                            \
    std::pair<RayDifferential3fP, SpectrumfP> sample_ray_differential(         \
        FloatP time, FloatP sample1, const Point2fP &sample2,                  \
        const Point2fP &sample3, MaskP active) const override {                \
        ScopedPhase p(EProfilerPhase::EEndpointSampleRayP);                    \
        return sample_ray_differential_impl(time, sample1, sample2, sample3,   \
                                            active);                           \
    }

#if 0
#if !defined(MTS_ENABLE_AUTODIFF)
#  define MTS_IMPLEMENT_SENSOR_AUTODIFF()
#else
#  define MTS_IMPLEMENT_SENSOR_AUTODIFF()                                      \
    MTS_IMPLEMENT_ENDPOINT_AUTODIFF()                                          \
    std::pair<RayDifferential3fD, SpectrumfD> sample_ray_differential(         \
        FloatD time, FloatD sample1, const Point2fD &sample2,                  \
        const Point2fD &sample3, MaskD active) const override {                \
        ScopedPhase p(EProfilerPhase::EEndpointSampleRay);                     \
        return sample_ray_differential_impl(time, sample1, sample2, sample3,   \
                                            active);                           \
    }
#endif
#endif

#if !defined(MTS_ENABLE_AUTODIFF)
#  define MTS_IMPLEMENT_SENSOR_AUTODIFF()
#else
#  define MTS_IMPLEMENT_SENSOR_AUTODIFF()                                      \
    MTS_IMPLEMENT_ENDPOINT_AUTODIFF()
#endif

#define MTS_IMPLEMENT_SENSOR_ALL()                                             \
    MTS_IMPLEMENT_SENSOR_SCALAR()                                              \
    MTS_IMPLEMENT_SENSOR_PACKET()                                              \
    MTS_IMPLEMENT_SENSOR_AUTODIFF()

/// Instantiates concrete scalar and packet versions of the polarized sensor plugin API
#define MTS_IMPLEMENT_SENSOR_POLARIZED_SCALAR()                                \
    MTS_IMPLEMENT_ENDPOINT()                                                   \
    std::pair<RayDifferential3f, MuellerMatrixSf> sample_ray_differential_pol( \
        Float time, Float sample1, const Point2f &sample2,                     \
        const Point2f &sample3) const override {                               \
        ScopedPhase p(EProfilerPhase::EEndpointSampleRay);                     \
        return sample_ray_differential_pol_impl(time, sample1, sample2,        \
                                                sample3, true);                \
    }

#define MTS_IMPLEMENT_SENSOR_POLARIZED_PACKET()                                \
    std::pair<RayDifferential3fP, MuellerMatrixSfP>                            \
    sample_ray_differential_pol(FloatP time, FloatP sample1,                   \
                                const Point2fP &sample2,                       \
                                const Point2fP &sample3, MaskP active)         \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::EEndpointSampleRayP);                    \
        return sample_ray_differential_pol_impl(time, sample1, sample2,        \
                                                sample3, active);              \
    }

#if !defined(MTS_ENABLE_AUTODIFF)
#  define MTS_IMPLEMENT_SENSOR_POLARIZED_AUTODIFF()
#else
#  define MTS_IMPLEMENT_SENSOR_POLARIZED_AUTODIFF()                            \
    std::pair<RayDifferential3fD, MuellerMatrixSfD>                            \
    sample_ray_differential_pol(FloatD time, FloatD sample1,                   \
                                const Point2fD &sample2,                       \
                                const Point2fD &sample3, MaskD active)         \
        const override {                                                       \
        ScopedPhase p(EProfilerPhase::EEndpointSampleRay);                     \
        return sample_ray_differential_pol_impl(time, sample1, sample2,        \
                                                sample3, active);              \
    }
#endif

#define MTS_IMPLEMENT_SENSOR_POLARIZED_ALL()                                   \
    MTS_IMPLEMENT_SENSOR_POLARIZED_SCALAR()                                    \
    MTS_IMPLEMENT_SENSOR_POLARIZED_PACKET()                                    \
    MTS_IMPLEMENT_SENSOR_POLARIZED_AUTODIFF()
