#include <mitsuba/core/bbox.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/frame.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT_VARIANTS(Transform) {
    MTS_IMPORT_CORE_TYPES()

    using Transform3f = Transform<Float, 3>;
    using Matrix3f = enoki::Matrix<Float, 3>;
    py::class_<Transform3f>(m, "Transform3f", D(Transform3f))
        .def(py::init<>(), "Initialize with the identity matrix")
        .def(py::init<const Transform3f &>(), "Copy constructor")
        .def(py::init([](py::array a) {
            return new Transform3f(py::cast<Matrix3f>(a));
        }))
        .def(py::init<Matrix3f>(), D(Transform3f, Transform3f))
        .def(py::init<Matrix3f, Matrix3f>(), "Initialize from a matrix and its inverse transpose")
        .def("transform_point",
            vectorize<Float>([](const Transform<ScalarFloat, 3> &t, const Point2f &v) {
                 return t*v;
            }))
        .def("transform_vector",
            vectorize<Float>([](const Transform<ScalarFloat, 3> &t, const Vector2f &v) {
                 return t*v;
            }))
        .def_static("translate", &Transform3f::translate, "v"_a, D(Transform3f, translate))
        .def_static("scale", &Transform3f::scale, "v"_a, D(Transform3f, scale))
        .def_static("rotate", &Transform3f::template rotate<3>,
                    "angle"_a, D(Transform3f, rotate, 2))
        .def("has_scale", &Transform3f::has_scale, D(Transform3f, has_scale))
        /// Operators
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def(py::self * py::self)
        /// Fields
        .def("inverse",   &Transform3f::inverse, D(Transform3f, inverse))
        .def("has_scale", &Transform3f::has_scale, D(Transform3f, has_scale))
        .def_readwrite("matrix", &Transform3f::matrix)
        .def_readwrite("inverse_transpose", &Transform3f::inverse_transpose)
        .def_repr(Transform3f)
        ;

    using Transform4f = Transform<Float, 4>;
    using Matrix4f = enoki::Matrix<Float, 4>;
    py::class_<Transform4f>(m, "Transform4f", D(Transform4f))
        .def(py::init<>(), "Initialize with the identity matrix")
        .def(py::init<const Transform4f &>(), "Copy constructor")
        .def(py::init([](py::array a) {
            return new Transform4f(py::cast<Matrix4f>(a));
        }))
        .def(py::init<Matrix4f>(), D(Transform4f, Transform4f))
        .def(py::init<Matrix4f, Matrix4f>(), "Initialize from a matrix and its inverse transpose")
        .def("transform_point",
            vectorize<Float>([](const Transform<ScalarFloat, 4> &t, const Point3f &v) {
                return t*v;
            }))
        .def("transform_vector",
            vectorize<Float>([](const Transform<ScalarFloat, 4> &t, const Vector3f &v) {
                return t*v;
            }))
        .def("transform_normal",
            vectorize<Float>([](const Transform<ScalarFloat, 4> &t, const Normal3f &v) {
                return t*v;
            }))
        .def_static("translate", &Transform4f::translate, "v"_a, D(Transform4f, translate))
        .def_static("scale", &Transform4f::scale, "v"_a, D(Transform4f, scale))
        .def_static("rotate", &Transform4f::template rotate<4>,
            "axis"_a, "angle"_a, D(Transform4f, rotate))
        .def_static("perspective", &Transform4f::template perspective<4>,
            "fov"_a, "near"_a, "far"_a, D(Transform4f, perspective))
        .def_static("orthographic", &Transform4f::template orthographic<4>,
            "near"_a, "far"_a, D(Transform4f, orthographic))
        .def_static("look_at", &Transform4f::template look_at<4>,
            "origin"_a, "target"_a, "up"_a, D(Transform4f, look_at))
        .def_static("from_frame", &Transform4f::template from_frame<Float>,
            "frame"_a, D(Transform4f, from_frame))
        .def_static("to_frame", &Transform4f::template to_frame<Float>,
            "frame"_a, D(Transform4f, to_frame))
        .def("has_scale", &Transform4f::has_scale, D(Transform4f, has_scale))
        .def("extract", &Transform4f::template extract<3>, D(Transform4f, extract))
        /// Operators
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def(py::self * py::self)
        /// Fields
        .def("inverse",   &Transform4f::inverse, D(Transform4f, inverse))
        .def("has_scale", &Transform4f::has_scale, D(Transform4f, has_scale))
        .def_readwrite("matrix", &Transform4f::matrix)
        .def_readwrite("inverse_transpose", &Transform4f::inverse_transpose)
        .def_repr(Transform4f)
        ;

    py::implicitly_convertible<py::array, Transform4f>();
}

MTS_PY_EXPORT_VARIANTS(AnimatedTransform) {
    MTS_IMPORT_CORE_TYPES()
    using AnimatedTransform = AnimatedTransform<Float>;
    using Keyframe = typename AnimatedTransform::Keyframe;

    auto atrafo = MTS_PY_CLASS(AnimatedTransform, Object);

    py::class_<Keyframe>(atrafo, "Keyframe")
        .def(py::init<float, ScalarMatrix3f, ScalarQuaternion4f, ScalarVector3f>())
        .def_readwrite("time", &Keyframe::time, D(AnimatedTransform, Keyframe, time))
        .def_readwrite("scale", &Keyframe::scale, D(AnimatedTransform, Keyframe, scale))
        .def_readwrite("quat", &Keyframe::quat, D(AnimatedTransform, Keyframe, quat))
        .def_readwrite("trans", &Keyframe::trans, D(AnimatedTransform, Keyframe, trans));

    atrafo.def(py::init<>())
        .def(py::init<const ScalarTransform4f &>())
        .def_method(AnimatedTransform, size)
        .def_method(AnimatedTransform, has_scale)
        .def("__len__", &AnimatedTransform::size)
        .def("__getitem__", [](const AnimatedTransform &trafo, size_t index) {
            if (index >= trafo.size())
                throw py::index_error();
            return trafo[index];
        })
        .def("append",
             py::overload_cast<ScalarFloat, const ScalarTransform4f &>(&AnimatedTransform::append),
             D(AnimatedTransform, append))
        .def("append", py::overload_cast<const Keyframe &>( &AnimatedTransform::append))
        .def("eval", vectorize<Float>(&AnimatedTransform::eval),
             "time"_a, "unused"_a = true, D(AnimatedTransform, eval))
        .def_method(AnimatedTransform, translation_bounds)
        ;
}
