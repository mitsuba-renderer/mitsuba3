#include <mitsuba/core/bbox.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>

template <typename T>
auto bind_transform(py::module &m, const char *name) {
    using Type = Transform<T>;

    return py::class_<Type>(m, name, D(Transform))
        /// Fields
        .def("inverse", &Type::inverse, D(Transform, inverse))
        .def("has_scale", &Type::has_scale, D(Transform, has_scale))
        .def_readwrite("matrix", &Type::matrix)
        .def_readwrite("inverse_transpose", &Type::inverse_transpose)
        .def("__repr__", [](const Type &t) {
            std::ostringstream oss;
            oss << t;
            return oss.str();
        });
}

MTS_PY_EXPORT(Transform) {
    bind_transform<Float>(m, "Transform4f")
        .def(py::init<>(), "Initialize with the identity matrix")
        .def(py::init<const Transform4f &>(), "Copy constructor")
        .def(py::init([](py::array a) {
            return new Transform4f(py::cast<Matrix4f>(a));
        }))
        .def(py::init<Matrix4f>(), D(Transform, Transform))
        .def(py::init<Matrix4f, Matrix4f>(), "Initialize from a matrix and its inverse transpose")
        .def("transform_point", [](const Transform4f &t, const Point3f &v) { return t*v; })
        .def("transform_point", [](const Transform4f &t, const Point3fX &v) {
            return vectorize_safe([](auto &&t, auto &&v) { return t * v; }, t, v);
        })
        .def("transform_vector", [](const Transform4f &t, const Vector3f &v) { return t*v; })
        .def("transform_vector", [](const Transform4f &t, const Vector3fX &v) {
            return vectorize_safe([](auto &&t, auto &&v) { return t * v; }, t, v);
        })
        .def("transform_normal", [](const Transform4f &t, const Normal3f &v) { return t*v; })
        .def("transform_normal", [](const Transform4f &t, const Normal3fX &v) {
            return vectorize_safe([](auto &&t, auto &&v) { return t * v; }, t, v);
        })
        .def_static("translate", &Transform4f::translate, "v"_a, D(Transform, translate))
        .def_static("scale", &Transform4f::scale, "v"_a, D(Transform, scale))
        .def_static("rotate", &Transform4f::rotate, "axis"_a, "angle"_a, D(Transform, rotate))
        .def_static("perspective", &Transform4f::perspective, "fov"_a, "near"_a, "far"_a, D(Transform, perspective))
        .def_static("orthographic", &Transform4f::orthographic, "near"_a, "far"_a, D(Transform, orthographic))
        .def_static("look_at", &Transform4f::look_at, "origin"_a, "target"_a, "up"_a, D(Transform, look_at))
        .def("has_scale", &Transform4f::has_scale, D(Transform, has_scale))
        /// Operators
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def(py::self * py::self);

    auto t4fx = bind_transform<FloatX>(m, "Transform4fX");
    bind_slicing_operators<Transform4fX, Transform4f>(t4fx);

    py::implicitly_convertible<py::array, Transform4f>();
}

MTS_PY_EXPORT(AnimatedTransform) {
    auto atrafo = MTS_PY_CLASS(AnimatedTransform, Object);

    py::class_<AnimatedTransform::Keyframe>(atrafo, "Keyframe")
        .def(py::init<Float, Matrix3f, Quaternion4f, Vector3f>())
        .def_readwrite("time", &AnimatedTransform::Keyframe::time, D(AnimatedTransform, Keyframe, time))
        .def_readwrite("scale", &AnimatedTransform::Keyframe::scale, D(AnimatedTransform, Keyframe, scale))
        .def_readwrite("quat", &AnimatedTransform::Keyframe::quat, D(AnimatedTransform, Keyframe, quat))
        .def_readwrite("trans", &AnimatedTransform::Keyframe::trans, D(AnimatedTransform, Keyframe, trans));

    atrafo
        .def(py::init<>())
        .def(py::init<const Transform4f &>())
        .mdef(AnimatedTransform, size)
        .def("__len__", &AnimatedTransform::size)
        .def("__getitem__", [](const AnimatedTransform &trafo, size_t index) {
            if (index >= trafo.size())
                throw py::index_error();
            return trafo[index];
        })
        .def("append",
             py::overload_cast<Float, const Transform4f &>(
                 &AnimatedTransform::append),
             D(AnimatedTransform, append))
        .def("append",
             py::overload_cast<const AnimatedTransform::Keyframe &>(
                 &AnimatedTransform::append))
        .def("lookup",
             py::overload_cast<const Float &, const bool &>(
                 &AnimatedTransform::lookup, py::const_),
             "time"_a, "unused"_a = true, D(AnimatedTransform, lookup))
        .def("lookup",
             vectorize_wrapper(
                 py::overload_cast<const FloatP &, const mask_t<FloatP> &>(
                     &AnimatedTransform::lookup, py::const_)),
             "time"_a, "active"_a = true)
        .mdef(AnimatedTransform, translation_bounds)
        ;
}
