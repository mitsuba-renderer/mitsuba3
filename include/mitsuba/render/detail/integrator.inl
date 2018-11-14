// -----------------------------------------------------------------------
//! @{ \name Macro for template implementation of integrator methods
// -----------------------------------------------------------------------

#define MTS_IMPLEMENT_INTEGRATOR_SCALAR()                                      \
    using SamplingIntegrator::eval;                                            \
    Spectrumf eval(const RayDifferential3f &ray, RadianceSample3f &rs)         \
        const override {                                                       \
        return eval_impl(ray, rs, true);                                       \
    }

#define MTS_IMPLEMENT_INTEGRATOR_PACKET()                                      \
    SpectrumfP eval(const RayDifferential3fP &ray, RadianceSample3fP &rs,      \
                    MaskP active) const override {                             \
        return eval_impl(ray, rs, active);                                     \
    }

#if !defined(MTS_ENABLE_AUTODIFF)
#  define MTS_IMPLEMENT_INTEGRATOR_AUTODIFF()
#else
#  define MTS_IMPLEMENT_INTEGRATOR_AUTODIFF()                                  \
    SpectrumfD eval(const RayDifferential3fD &ray, RadianceSample3fD &rs,      \
                    MaskD active) const override {                             \
        return eval_impl(ray, rs, active);                                     \
    }
#endif

#define MTS_IMPLEMENT_INTEGRATOR_ALL()                                         \
    MTS_IMPLEMENT_INTEGRATOR_SCALAR()                                          \
    MTS_IMPLEMENT_INTEGRATOR_PACKET()                                          \
    MTS_IMPLEMENT_INTEGRATOR_AUTODIFF()

//! @}
// -----------------------------------------------------------------------
