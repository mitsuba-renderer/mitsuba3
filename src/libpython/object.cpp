#include "python.h"

MTS_PY_EXPORT(Object) {
    py::class_<Object, ref<Object>>(m, "Object", DM(Object))
        .def(py::init<>(), DM(Object, Object))
        .def(py::init<const Object &>(), DM(Object, Object, 2))
        .mdef(Object, refCount)
        .mdef(Object, incRef)
        .mdef(Object, decRef, py::arg("dealloc") = true)
        .def("__repr__", &Object::toString, DM(Object, toString));
}
