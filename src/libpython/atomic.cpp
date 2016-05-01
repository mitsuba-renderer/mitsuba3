#include <mitsuba/core/atomic.h>
#include "python.h"

MTS_PY_EXPORT(atomic) {
    py::class_<AtomicFloat<>>(m, "AtomicFloat", DM(AtomicFloat))
        .def(py::init<Float>(), DM(AtomicFloat, AtomicFloat))
        .def(py::self += Float(), DM(AtomicFloat, operator, iadd))
        .def(py::self -= Float(), DM(AtomicFloat, operator, imul))
        .def(py::self *= Float(), DM(AtomicFloat, operator, imul))
        .def(py::self /= Float(), DM(AtomicFloat, operator, idiv))
        .def("__float__", [](const AtomicFloat<> &af) { return (Float) af; },
             DM(AtomicFloat, operator, T0));
}
