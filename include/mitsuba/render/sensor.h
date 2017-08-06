#pragma once

#include <mitsuba/core/object.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Sensor : public Object {
public:

    MTS_DECLARE_CLASS()

protected:
    virtual ~Sensor();
};

NAMESPACE_END(mitsuba)
