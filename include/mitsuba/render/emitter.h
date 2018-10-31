#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/common.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Emitter : public Endpoint {
public:
    /// Is this an environment map light emitter?
    virtual bool is_environment() const;

    MTS_DECLARE_CLASS()

protected:
    Emitter(const Properties &props);

    virtual ~Emitter();
};

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for packets of Emitter pointers
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
#define MTS_IMPLEMENT_EMITTER()                                                \
    MTS_IMPLEMENT_ENDPOINT()
