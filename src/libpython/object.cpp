#include "python.h"

MTS_PY_EXPORT(object) {
    py::class_<Object, ref<Object>>(m, "Object", DM(Object))
        .def(py::init<>(), DM(Object, Object))
        .def(py::init<const Object &>(), DM(Object, Object, 2))
        .def("getRefCount", &Object::getRefCount, DM(Object, getRefCount))
        .def("incRef", &Object::incRef, DM(Object, incRef))
        .def("decRef", &Object::decRef, py::arg("dealloc") = true, DM(Object, decRef))
        .def("__repr__", &Object::toString, DM(Object, toString));
}
