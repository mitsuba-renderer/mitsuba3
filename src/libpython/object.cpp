#include "python.h"

MTS_PY_EXPORT(Object) {
    py::class_<Object, ref<Object>>(m, "Object", D(Object))
        .def(py::init<>(), D(Object, Object))
        .def(py::init<const Object &>(), D(Object, Object, 2))
        .mdef(Object, ref_count)
        .mdef(Object, inc_ref)
        .mdef(Object, dec_ref, "dealloc"_a = true)
        .def("__repr__", &Object::to_string, D(Object, to_string));
}
