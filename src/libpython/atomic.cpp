#include <mitsuba/core/atomic.h>
#include "python.h"

MTS_PY_EXPORT(atomic) {
    py::class_<AtomicFloat<>>(m, "AtomicFloat", D(AtomicFloat))
        .def(py::init<Float>(), D(AtomicFloat, AtomicFloat))
        .def(py::self += Float(), D(AtomicFloat, operator, iadd))
        .def(py::self -= Float(), D(AtomicFloat, operator, imul))
        .def(py::self *= Float(), D(AtomicFloat, operator, imul))
        .def(py::self /= Float(), D(AtomicFloat, operator, idiv))
        .def("__float__", [](const AtomicFloat<> &af) { return (Float) af; },
             D(AtomicFloat, operator, T0));
}
