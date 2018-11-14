// -----------------------------------------------------------------------
//! @{ \name Enoki support for vectorized function calls
// -----------------------------------------------------------------------

// Enable usage of array pointers for our types
ENOKI_CALL_SUPPORT_BEGIN(mitsuba::Emitter)
    ENOKI_CALL_SUPPORT_METHOD(sample_ray)
    ENOKI_CALL_SUPPORT_METHOD(eval)
    ENOKI_CALL_SUPPORT_METHOD(sample_direction)
    ENOKI_CALL_SUPPORT_METHOD(pdf_direction)
    ENOKI_CALL_SUPPORT_METHOD(sample_ray_pol)
    ENOKI_CALL_SUPPORT_METHOD(eval_pol)
    ENOKI_CALL_SUPPORT_METHOD(sample_direction_pol)
ENOKI_CALL_SUPPORT_END(mitsuba::Emitter)

//! @}
// -----------------------------------------------------------------------

/// Instantiates concrete scalar and packet versions of the emitter plugin API
#define MTS_IMPLEMENT_EMITTER_SCALAR()                                         \
    MTS_IMPLEMENT_ENDPOINT_SCALAR()

#define MTS_IMPLEMENT_EMITTER_PACKET()                                         \
    MTS_IMPLEMENT_ENDPOINT_PACKET()

#define MTS_IMPLEMENT_EMITTER_AUTODIFF()                                       \
    MTS_IMPLEMENT_ENDPOINT_AUTODIFF()

#define MTS_IMPLEMENT_EMITTER_ALL()                                            \
    MTS_IMPLEMENT_EMITTER_SCALAR()                                             \
    MTS_IMPLEMENT_EMITTER_PACKET()                                             \
    MTS_IMPLEMENT_EMITTER_AUTODIFF()
