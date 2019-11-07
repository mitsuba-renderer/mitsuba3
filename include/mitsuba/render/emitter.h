#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Emitter : public Endpoint<Float, Spectrum> {
public:
    MTS_DECLARE_CLASS_VARIANT(Emitter, Endpoint, "emitter")
    MTS_USING_BASE(Endpoint)

    /// Is this an environment map light emitter?
    virtual bool is_environment() const {
        return false;
    }

protected:
    Emitter(const Properties &props)
        : Base(props) { }

    virtual ~Emitter();
};

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for vectorized function calls
// -----------------------------------------------------------------------

// Enable usage of array pointers for our types
// TODO: re-enable this
// ENOKI_CALL_SUPPORT_BEGIN(mitsuba::Emitter)
//     ENOKI_CALL_SUPPORT_METHOD(sample_ray)
//     ENOKI_CALL_SUPPORT_METHOD(eval)
//     ENOKI_CALL_SUPPORT_METHOD(sample_direction)
//     ENOKI_CALL_SUPPORT_METHOD(pdf_direction)
//     ENOKI_CALL_SUPPORT_METHOD(sample_ray_pol)
//     ENOKI_CALL_SUPPORT_METHOD(eval_pol)
//     ENOKI_CALL_SUPPORT_METHOD(sample_direction_pol)
//     ENOKI_CALL_SUPPORT_METHOD(is_environment)
// ENOKI_CALL_SUPPORT_END(mitsuba::Emitter)

//! @}
// -----------------------------------------------------------------------
