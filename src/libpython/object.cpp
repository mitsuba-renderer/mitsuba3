#include "python.h"

MTS_PY_EXPORT(Object) {
    MTS_PY_CLASS(Object)
        .def(py::init<>(), DM(Object, Object))
        .def(py::init<const Object &>(), DM(Object, Object, 2))
        .qdef(Object, getRefCount)
        .qdef(Object, incRef)
        .qdef(Object, decRef, py::arg("dealloc") = true)
        .def("__repr__", &Object::toString, DM(Object, toString));
}
