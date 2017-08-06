#pragma once

#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Emitter : public Object {
public:

    MTS_DECLARE_CLASS()

protected:
    virtual ~Emitter();
};

NAMESPACE_END(mitsuba)
