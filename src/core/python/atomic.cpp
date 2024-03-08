#include <mitsuba/core/atomic.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(atomic) {
    using AtomicFloat = mitsuba::AtomicFloat<>;

    nb::class_<AtomicFloat>(m, "AtomicFloat", D(AtomicFloat))
        .def(nb::init<float>(), D(AtomicFloat, AtomicFloat))
        .def(nb::self += float(), D(AtomicFloat, operator, iadd), nb::rv_policy::none)
        .def(nb::self -= float(), D(AtomicFloat, operator, isub), nb::rv_policy::none)
        .def(nb::self *= float(), D(AtomicFloat, operator, imul), nb::rv_policy::none)
        .def(nb::self /= float(), D(AtomicFloat, operator, idiv), nb::rv_policy::none)
        .def("__float__", [](const AtomicFloat &af) { return (float) af; },
            D(AtomicFloat, operator, T0));
}
