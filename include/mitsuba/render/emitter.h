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

#include "detail/emitter.inl"
