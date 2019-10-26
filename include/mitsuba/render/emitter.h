#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/endpoint.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Emitter : public Endpoint<Float, Spectrum> {
public:
    MTS_REGISTER_INTERFACE(Emitter, Endpoint)

    using Base = Endpoint<Float, Spectrum>;

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

#include "detail/emitter.inl"
