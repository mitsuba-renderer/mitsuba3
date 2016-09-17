#pragma once

#include <mitsuba/core/object.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER BSDF : public Object {
public:
    virtual void dummy() = 0;

    MTS_DECLARE_CLASS()

protected:
    virtual ~BSDF() { }
};

NAMESPACE_END(mitsuba)
