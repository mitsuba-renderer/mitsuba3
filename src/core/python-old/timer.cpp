#include <mitsuba/core/timer.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(Timer) {
    py::class_<Timer>(m, "Timer")
        .def(py::init<>())
        .def_method(Timer, value)
        .def_method(Timer, reset)
        .def_method(Timer, begin_stage)
        .def_method(Timer, end_stage);
}
