#include <mitsuba/core/atomic.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(atomic) {
    using AtomicFloat = mitsuba::AtomicFloat<>;

    nb::class_<AtomicFloat>(m, "AtomicFloat", D(AtomicFloat))
        .def(nb::init<float>(), D(AtomicFloat, AtomicFloat))
        .def(nb::self += float(), D(AtomicFloat, operator, iadd))
        .def(nb::self -= float(), D(AtomicFloat, operator, isub))
        .def(nb::self *= float(), D(AtomicFloat, operator, imul))
        .def(nb::self /= float(), D(AtomicFloat, operator, idiv))
        .def("__float__", [](const AtomicFloat &af) { return (float) af; },
            D(AtomicFloat, operator, T0));
}
