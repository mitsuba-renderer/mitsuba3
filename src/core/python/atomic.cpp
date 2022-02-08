#include <mitsuba/core/atomic.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(atomic) {
    using AtomicFloat = mitsuba::AtomicFloat<>;

    py::class_<AtomicFloat>(m, "AtomicFloat", D(AtomicFloat))
        .def(py::init<float>(), D(AtomicFloat, AtomicFloat))
        .def(py::self += float(), D(AtomicFloat, operator, iadd))
        .def(py::self -= float(), D(AtomicFloat, operator, isub))
        .def(py::self *= float(), D(AtomicFloat, operator, imul))
        .def(py::self /= float(), D(AtomicFloat, operator, idiv))
        .def("__float__", [](const AtomicFloat &af) { return (float) af; },
            D(AtomicFloat, operator, T0));
}
