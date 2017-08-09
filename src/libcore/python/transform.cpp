#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Transform) {
    py::class_<Transform>(m, "Transform", D(Transform))
        .def(py::init<>(), D(Transform, Transform))
        .def(py::init<Matrix4f>(), D(Transform, Transform, 2))
        .def(py::init<Matrix4f, Matrix4f>(), D(Transform, Transform, 3))
        .def("transform_point", [](const Transform &t, const Point3f &v) -> Point3f { return t*v; })
        .def("transform_point", [](const Transform &t, const Point3fX &v) {
            return vectorize_safe([&](auto v) { return t * Point3fP(v); }, v);
        })
        .def("transform_vector", [](const Transform &t, const Vector3f &v) -> Vector3f { return t*v; })
        .def("transform_vector", [](const Transform &t, const Vector3fX &v) {
            return vectorize_safe([&](auto v) { return t * Vector3fP(v); }, v);
        })
        .def("transform_normal", [](const Transform &t, const Normal3f &v) -> Vector3f { return t*v; })
        .def("transform_normal", [](const Transform &t, const Normal3fX &v) {
            return vectorize_safe([&](auto v) { return t * Normal3fP(v); }, v);
        })
        .def_static("translate", &Transform::translate, "v"_a, D(Transform, translate))
        .def_static("scale", &Transform::scale, "v"_a, D(Transform, scale))
        .def_static("rotate", &Transform::rotate, "axis"_a, "angle"_a, D(Transform, rotate))
        .def("matrix", &Transform::matrix)
        .def("inverse_matrix", &Transform::inverse_matrix)
        /// Operators
        .def(py::self * py::self)
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__repr__", [](const Transform &t) {
            std::ostringstream oss;
            oss << t;
            return oss.str();
        });

    py::implicitly_convertible<Matrix4f, Transform>();
}
