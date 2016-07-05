#pragma once

#include <mitsuba/core/object.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Scene : public Object {
public:
    Scene(const Properties &props);

    MTS_DECLARE_CLASS()
protected:
    virtual ~Scene();
};

NAMESPACE_END(mitsuba)
