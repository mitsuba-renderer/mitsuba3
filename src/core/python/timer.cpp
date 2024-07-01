#include <mitsuba/core/timer.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>

MI_PY_EXPORT(Timer) {
    nb::class_<Timer>(m, "Timer")
        .def(nb::init<>())
        .def_method(Timer, value)
        .def_method(Timer, reset)
        .def_method(Timer, begin_stage)
        .def_method(Timer, end_stage);
}
