#include <mitsuba/core/transform.h>
#include "python.h"

MTS_PY_EXPORT(Transform) {
    py::class_<Transform>(m, "Transform", D(Transform))
        .def(py::init<>(), D(Transform, Transform))
        .def(py::init<Matrix4f>(), D(Transform, Transform, 2))
        .def(py::init<Matrix4f, Matrix4f>(), D(Transform, Transform, 3))
        .def(py::self * py::self)
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__mul__", [](const Transform &t, const Point3f &v) -> Point3f { return t*v; })
        .def("__mul__", [](const Transform &t, const Point3fX &v) {
            return vectorize([&](auto v) { return t*Point3fP(v); }, v);
        });
}
