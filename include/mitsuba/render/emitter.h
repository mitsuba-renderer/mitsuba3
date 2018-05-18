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

using EmitterPtr = like_t<FloatP, const Emitter *>;

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for packets of Emitter pointers
// -----------------------------------------------------------------------

// Enable usage of array pointers for our types
ENOKI_CALL_SUPPORT_BEGIN(mitsuba::EmitterPtr)
ENOKI_CALL_SUPPORT(eval)
ENOKI_CALL_SUPPORT(sample_direction)
ENOKI_CALL_SUPPORT(pdf_direction)
ENOKI_CALL_SUPPORT_END(mitsuba::EmitterPtr)

//! @}
// -----------------------------------------------------------------------

/// Instantiates concrete scalar and packet versions of the emitter plugin API
#define MTS_IMPLEMENT_EMITTER()                                                \
    MTS_IMPLEMENT_ENDPOINT()
