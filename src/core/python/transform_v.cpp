#include <mitsuba/core/bbox.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/frame.h>
#include <mitsuba/core/properties.h>
#include <pybind11/numpy.h>
#include <mitsuba/python/python.h>


template <typename Float>
void bind_transform3(py::module &m, const char *name) {
    MI_IMPORT_CORE_TYPES()

    auto trans3 = py::class_<Transform3f>(m, name, D(Transform))
        .def(py::init<>(), "Initialize with the identity matrix")
        .def(py::init<const Transform3f &>(), "Copy constructor")
        .def(py::init([](py::array a) {
            if (a.size() == 9)
                return new Transform3f(py::cast<ScalarMatrix3f>(a));
            else
                return new Transform3f(py::cast<Matrix3f>(a));
        }))
        .def(py::init([](const py::list &list) {
            size_t size = list.size();
            if (size != 3)
                throw py::reference_cast_error();
            ScalarMatrix3f m;
            for (size_t i = 0; i < size; ++i)
                m[i] = py::cast<ScalarVector3f>(list[i]);
            return new Transform3f(transpose(m));
        }))
        .def(py::init<Matrix3f>(), D(Transform, Transform))
        .def(py::init<Matrix3f, Matrix3f>(), "Initialize from a matrix and its inverse transpose")
        .def_static("translate", &Transform3f::translate, "v"_a, D(Transform, translate))
        .def_static("scale", &Transform3f::scale, "v"_a, D(Transform, scale))
        .def_static("rotate", &Transform3f::template rotate<3>,
                    "angle"_a, D(Transform, rotate, 2))
        .def("has_scale", &Transform3f::has_scale, D(Transform, has_scale))
        /// Operators
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__mul__", [](const Transform3f &, const Transform3f &) {
            throw std::runtime_error("mul(): please use the matrix "
                "multiplication operator '@' instead.");
        }, py::is_operator())
        .def("__matmul__", [](const Transform3f &a, const Transform3f &b) {
            return a * b;
        }, py::is_operator())
        .def("__matmul__", [](const Transform3f &a, const Point2f &b) {
            return a * b;
        }, py::is_operator())
        .def("__matmul__", [](const Transform3f &a, const Vector2f &b) {
            return a * b;
        }, py::is_operator())
        .def("transform_affine", [](const Transform3f &a, const Point2f &b) {
            return a.transform_affine(b);
        }, "p"_a, D(Transform, transform_affine))
        .def("transform_affine", [](const Transform3f &a, const Vector2f &b) {
            return a.transform_affine(b);
        }, "v"_a, D(Transform, transform_affine))
        /// Fields
        .def("inverse",   &Transform3f::inverse, D(Transform, inverse))
        .def("translation", &Transform3f::translation, D(Transform, translation))
        .def("has_scale", &Transform3f::has_scale, D(Transform, has_scale))
        .def_readwrite("matrix", &Transform3f::matrix)
        .def_readwrite("inverse_transpose", &Transform3f::inverse_transpose)
        .def_repr(Transform3f);

    if constexpr (dr::is_dynamic_v<Float>)
        trans3.def(py::init<const ScalarTransform3f &>(), "Broadcast constructor");

    MI_PY_DRJIT_STRUCT(trans3, Transform3f, matrix, inverse_transpose)
}

template <typename Float, typename Spectrum>
void bind_transform4(py::module &m, const char *name) {
    MI_IMPORT_CORE_TYPES()
    using Ray3f = Ray<Point<Float, 3>, Spectrum>;
    auto trans4 = py::class_<Transform4f>(m, name, D(Transform))
        .def(py::init<>(), "Initialize with the identity matrix")
        .def(py::init<const Transform4f &>(), "Copy constructor")
        .def(py::init([](py::array a) {
            if (a.size() == 16)
                return new Transform4f(py::cast<ScalarMatrix4f>(a));
            else
                return new Transform4f(py::cast<Matrix4f>(a));
        }))
        .def(py::init([](const py::list &list) {
            size_t size = list.size();
            if (size != 4)
                throw py::reference_cast_error();
            ScalarMatrix4f m;
            for (size_t i = 0; i < size; ++i)
                m[i] = py::cast<ScalarVector4f>(list[i]);
            return new Transform4f(transpose(m));
        }))
        .def(py::init<Matrix4f>(), D(Transform, Transform))
        .def(py::init<Matrix4f, Matrix4f>(), "Initialize from a matrix and its inverse transpose")
        .def_static("translate", &Transform4f::translate, "v"_a, D(Transform, translate))
        .def_static("scale", &Transform4f::scale, "v"_a, D(Transform, scale))
        .def_static("rotate", &Transform4f::template rotate<4>,
            "axis"_a, "angle"_a, D(Transform, rotate))
        .def_static("perspective", &Transform4f::template perspective<4>,
            "fov"_a, "near"_a, "far"_a, D(Transform, perspective))
        .def_static("orthographic", &Transform4f::template orthographic<4>,
            "near"_a, "far"_a, D(Transform, orthographic))
        .def_static("look_at", &Transform4f::template look_at<4>,
            "origin"_a, "target"_a, "up"_a, D(Transform, look_at))
        .def_static("from_frame", &Transform4f::template from_frame<Float>,
            "frame"_a, D(Transform, from_frame))
        .def_static("to_frame", &Transform4f::template to_frame<Float>,
            "frame"_a, D(Transform, to_frame))
        .def("translation", &Transform4f::translation, D(Transform, translation))
        .def("has_scale", &Transform4f::has_scale, D(Transform, has_scale))
        .def("extract", &Transform4f::template extract<3>, D(Transform, extract))
        /// Operators
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__mul__", [](const Transform4f &, const Transform4f &) {
            throw std::runtime_error("mul(): please use the matrix "
                "multiplication operator '@' instead.");
        }, py::is_operator())
        .def("__matmul__", [](const Transform4f &a, const Transform4f &b) {
            return a * b;
        }, py::is_operator())
        .def("__matmul__", [](const Transform4f &a, const Point3f &b) {
            return a * b;
        }, py::is_operator())
        .def("__matmul__", [](const Transform4f &a, const Vector3f &b) {
            return a * b;
        }, py::is_operator())
        .def("__matmul__", [](const Transform4f &a, const Normal3f &b) {
            return a * b;
        }, py::is_operator())
        .def("__matmul__", [](const Transform4f &a, const Ray3f &b) {
            return a * b;
        }, py::is_operator())
        .def("transform_affine", [](const Transform4f &a, const Point3f &b) {
            return a.transform_affine(b);
        }, "p"_a, D(Transform, transform_affine))
        .def("transform_affine", [](const Transform4f &a, const Ray3f &b) {
            return a.transform_affine(b);
        }, "ray"_a, D(Transform, transform_affine))
        .def("transform_affine", [](const Transform4f &a, const Vector3f &b) {
            return a.transform_affine(b);
        }, "v"_a, D(Transform, transform_affine))
        .def("transform_affine", [](const Transform4f &a, const Normal3f &b) {
            return a.transform_affine(b);
        }, "n"_a, D(Transform, transform_affine))
        /// Fields
        .def("inverse",   &Transform4f::inverse, D(Transform, inverse))
        .def("has_scale", &Transform4f::has_scale, D(Transform, has_scale))
        .def_readwrite("matrix", &Transform4f::matrix)
        .def_readwrite("inverse_transpose", &Transform4f::inverse_transpose)
        .def_repr(Transform4f);

    if constexpr (dr::is_dynamic_v<Float>)
        trans4.def(py::init<const ScalarTransform4f &>(), "Broadcast constructor");

    MI_PY_DRJIT_STRUCT(trans4, Transform4f, matrix, inverse_transpose)
}

MI_PY_EXPORT(Transform) {
    MI_PY_IMPORT_TYPES()
    using ScalarSpectrum = scalar_spectrum_t<Spectrum>;

    MI_PY_CHECK_ALIAS(Transform3f, "Transform3f") {
        bind_transform3<Float>(m, "Transform3f");
    }

    MI_PY_CHECK_ALIAS(Transform3d, "Transform3d") {
        bind_transform3<Float64>(m, "Transform3d");
    }

    MI_PY_CHECK_ALIAS(ScalarTransform3f, "ScalarTransform3f") {
        if constexpr (dr::is_dynamic_v<Float>) {
            bind_transform3<ScalarFloat>(m, "ScalarTransform3f");
            py::implicitly_convertible<ScalarTransform3f, Transform3f>();
        }
    }

    MI_PY_CHECK_ALIAS(ScalarTransform3d, "ScalarTransform3d") {
        if constexpr (dr::is_dynamic_v<Float>) {
            bind_transform3<ScalarFloat64>(m, "ScalarTransform3d");
            py::implicitly_convertible<ScalarTransform3d, ScalarTransform3f>();
        }
    }

    MI_PY_CHECK_ALIAS(Transform4f, "Transform4f") {
        bind_transform4<Float, Spectrum>(m, "Transform4f");
    }

    MI_PY_CHECK_ALIAS(Transform4d, "Transform4d") {
        bind_transform4<Float64, Spectrum>(m, "Transform4d");
    }

    MI_PY_CHECK_ALIAS(ScalarTransform4f, "ScalarTransform4f") {
        if constexpr (dr::is_dynamic_v<Float>) {
            bind_transform4<ScalarFloat, ScalarSpectrum>(m, "ScalarTransform4f");
            py::implicitly_convertible<ScalarTransform4f, Transform4f>();
        }
    }

    MI_PY_CHECK_ALIAS(ScalarTransform4d, "ScalarTransform4d") {
        if constexpr (dr::is_dynamic_v<Float>) {
            bind_transform4<ScalarFloat64, ScalarSpectrum>(m, "ScalarTransform4d");
            py::implicitly_convertible<ScalarTransform4d, ScalarTransform4f>();
        }
    }

    py::implicitly_convertible<py::array, Transform4f>();
    py::implicitly_convertible<Matrix4f, Transform4f>();
}

MI_PY_EXPORT(AnimatedTransform) {
    MI_PY_IMPORT_TYPES()
    using Keyframe      = typename AnimatedTransform::Keyframe;
    using _Float        = typename AnimatedTransform::Float;
    using _Matrix3f     = typename AnimatedTransform::Matrix3f;
    using _Quaternion4f = typename AnimatedTransform::Quaternion4f;
    using _Vector3f     = typename AnimatedTransform::Vector3f;
    using _Transform4f  = typename AnimatedTransform::Transform4f;

    MI_PY_CHECK_ALIAS(AnimatedTransform, "AnimatedTransform") {
        auto atrafo = MI_PY_CLASS(AnimatedTransform, Object);

        py::class_<Keyframe>(atrafo, "Keyframe")
            .def(py::init<float, _Matrix3f, _Quaternion4f, _Vector3f>())
            .def_readwrite("time",  &Keyframe::time,  D(AnimatedTransform, Keyframe, time))
            .def_readwrite("scale", &Keyframe::scale, D(AnimatedTransform, Keyframe, scale))
            .def_readwrite("quat",  &Keyframe::quat,  D(AnimatedTransform, Keyframe, quat))
            .def_readwrite("trans", &Keyframe::trans, D(AnimatedTransform, Keyframe, trans));

        atrafo.def(py::init<>())
            .def(py::init<const _Transform4f &>())
            .def_method(AnimatedTransform, size)
            .def_method(AnimatedTransform, has_scale)
            .def("__len__", &AnimatedTransform::size)
            .def("__getitem__", [](const AnimatedTransform &trafo, size_t index) {
                if (index >= trafo.size())
                    throw py::index_error();
                return trafo[index];
            })
            .def("append",
                py::overload_cast<_Float, const _Transform4f &>(&AnimatedTransform::append),
                D(AnimatedTransform, append))
            .def("append", py::overload_cast<const Keyframe &>( &AnimatedTransform::append))
            .def("eval", &AnimatedTransform::template eval<Float>,
                 "time"_a, "unused"_a = true, D(AnimatedTransform, eval))
            .def_method(AnimatedTransform, translation_bounds);
    }
}
