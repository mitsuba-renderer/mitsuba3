#pragma once

#include <mitsuba/core/object.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Shape : public Object {
public:
    MTS_DECLARE_CLASS()
    virtual void doSomething() = 0;
        
protected:
    virtual ~Shape() { }
};

NAMESPACE_END(mitsuba)
