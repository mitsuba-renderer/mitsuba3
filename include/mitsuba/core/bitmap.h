#pragma once

#include <mitsuba/core/platform.h>

NAMESPACE_BEGIN(mitsuba)



/// Reference counted object base class
class MTS_EXPORT_CORE Properties {
public:
    /// Default constructor
    Properties() { }

    /// Copy constructor
    Properties(const Properties &) { }

private:
};

NAMESPACE_END(mitsuba)
