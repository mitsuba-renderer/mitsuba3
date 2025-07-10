#pragma once

#include <mitsuba/core/any.h>
#include <nanobind/nanobind.h>

NAMESPACE_BEGIN(mitsuba)

/// Create an Any object that stores a Python object
extern Any any_wrap(nb::handle obj);

NAMESPACE_END(mitsuba)
