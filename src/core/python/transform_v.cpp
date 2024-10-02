#include <mitsuba/core/bbox.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/frame.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/list.h>
#include <nanobind/ndarray.h>

template <typename Float, typename Spectrum>
void bind_transform3(nb::module_ &m, const char *name) {
    MI_IMPORT_CORE_TYPES()
    using ScalarType = drjit::scalar_t<Float>;
    using NdMatrix = nb::ndarray<ScalarType, nb::shape<3,3>,
        nb::c_contig, nb::device::cpu>;
    using Ray2f = Ray<Point<Float, 2>, Spectrum>;

    auto trans3 = nb::class_<Transform3f>(m, name, D(Transform))
        .def(nb::init<>(), "Initialize with the identity matrix")
        .def(nb::init<const Transform3f &>(), "Copy constructor")
        .def("__init__", [](Transform3f *t, NdMatrix a) {
            auto v = a.view();
            ScalarMatrix3f m;
            for (size_t i = 0; i < 3; ++i)
                for (size_t j = 0; j < 3; ++j)
                    m.entry(i, j) = v(i, j);
            new (t) Transform3f(m);
        })
        .def("__init__", [](const nb::list &list) {
            size_t size = list.size();
            if (size != 3)
                throw nb::cast_error();
            ScalarMatrix3f m;
            for (size_t i = 0; i < size; ++i)
                m[i] = nb::cast<ScalarVector3f>(list[i]);
            new Transform3f(m);
        })
        .def(nb::init<Matrix3f>(), D(Transform, Transform))
        .def(nb::init<Matrix3f, Matrix3f>(), "Initialize from a matrix and its inverse transpose")
        /// Operators
        .def(nb::self == nb::self)
        .def(nb::self != nb::self)
        .def("__mul__", [](const Transform3f &, const Transform3f &) {
            throw std::runtime_error("mul(): please use the matrix "
                "multiplication operator '@' instead.");
        }, nb::is_operator())
        .def("__matmul__", [](const Transform3f &a, const Transform3f &b) {
            return a * b;
        }, nb::is_operator())
        .def("__matmul__", [](const Transform3f &a, const Point2f &b) {
            return a * b;
        }, nb::is_operator())
        .def("__matmul__", [](const Transform3f &a, const Vector2f &b) {
            return a * b;
        }, nb::is_operator())
        .def("__matmul__", [](const Transform3f &a, const Ray2f &b) {
            return a * b;
        }, nb::is_operator())
        .def("transform_affine", [](const Transform3f &a, const Point2f &b) {
            return a.transform_affine(b);
        }, "p"_a, D(Transform, transform_affine))
        .def("transform_affine", [](const Transform3f &a, const Vector2f &b) {
            return a.transform_affine(b);
        }, "v"_a, D(Transform, transform_affine))
        .def("transform_affine", [](const Transform3f &a, const Ray2f &b) {
            return a.transform_affine(b);
        }, "ray"_a, D(Transform, transform_affine))
        /// Chain transformations
        .def("translate", [](const Transform3f &t, const Point2f &v) {
            return Transform3f(t * Transform3f::translate(v));
        }, "v"_a, D(Transform, translate))
        .def("scale", [](const Transform3f &t, const Point2f &v) {
            return Transform3f(t * Transform3f::scale(v));
        }, "v"_a, D(Transform, scale))
        .def("rotate", [](const Transform3f &t, const Float &a) {
            return Transform3f(t * Transform3f::template rotate<3>(a));
        }, "angle"_a, D(Transform, rotate, 2))
        /// Fields
        .def("inverse",   &Transform3f::inverse, D(Transform, inverse))
        .def("translation", &Transform3f::translation, D(Transform, translation))
        .def("has_scale", &Transform3f::has_scale, D(Transform, has_scale))
        .def_rw("matrix", &Transform3f::matrix)
        .def_rw("inverse_transpose", &Transform3f::inverse_transpose)
        .def_repr(Transform3f);

    if constexpr (dr::is_dynamic_v<Float>)
        trans3.def(nb::init<const ScalarTransform3f &>(), "Broadcast constructor");

    MI_PY_DRJIT_STRUCT(trans3, Transform3f, matrix, inverse_transpose)
}

template <typename Float, typename Spectrum>
void bind_transform4(nb::module_ &m, const char *name) {
    MI_IMPORT_CORE_TYPES()
    using Ray3f = Ray<Point<Float, 3>, Spectrum>;
    using ScalarType = drjit::scalar_t<Float>;
    using NdMatrix = nb::ndarray<ScalarType, nb::shape<4,4>,
        nb::c_contig, nb::device::cpu>;

    auto trans4 = nb::class_<Transform4f>(m, name, D(Transform))
        .def(nb::init<>(), "Initialize with the identity matrix")
        .def(nb::init<const Transform4f &>(), "Copy constructor")
        .def("__init__",[](Transform4f* t, NdMatrix& a) {
            auto v = a.view();
            ScalarMatrix4f m;
            for (size_t i = 0; i < 4; ++i)
                for (size_t j = 0; j < 4; ++j)
                    m.entry(i, j) = v(i, j);
            new (t) Transform4f(m);
        })
        .def("__init__", [](Transform4f *t, const nb::list &list) {
            size_t size = list.size();
            if (size != 4)
                throw nb::cast_error();
            ScalarMatrix4f m;
            for (size_t i = 0; i < size; ++i)
                m[i] = nb::cast<ScalarVector4f>(list[i]);
            new (t) Transform4f(m);
        })
        .def(nb::init<Matrix4f>(), D(Transform, Transform))
        .def(nb::init<Matrix4f, Matrix4f>(), "Initialize from a matrix and its inverse transpose")
        .def("translation", &Transform4f::translation, D(Transform, translation))
        .def("extract", &Transform4f::template extract<3>, D(Transform, extract))
        /// Operators
        .def(nb::self == nb::self)
        .def(nb::self != nb::self)
        .def("__mul__", [](const Transform4f &, const Transform4f &) {
            throw std::runtime_error("mul(): please use the matrix "
                "multiplication operator '@' instead.");
        }, nb::is_operator())
        .def("__matmul__", [](const Transform4f &a, const Transform4f &b) {
            return a * b;
        }, nb::is_operator())
        .def("__matmul__", [](const Transform4f &a, const Point3f &b) {
            return a * b;
        }, nb::is_operator())
        .def("__matmul__", [](const Transform4f &a, const Vector3f &b) {
            return a * b;
        }, nb::is_operator())
        .def("__matmul__", [](const Transform4f &a, const Normal3f &b) {
            return a * b;
        }, nb::is_operator())
        .def("__matmul__", [](const Transform4f &a, const Ray3f &b) {
            return a * b;
        }, nb::is_operator())
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
        // Chain transformations
        .def("translate", [](const Transform4f &t, const Point3f &v) {
            return Transform4f(t * Transform4f::translate(v));
        }, "v"_a, D(Transform, translate))
        .def("scale", [](const Transform4f &t, const Point3f &v) {
            return Transform4f(t * Transform4f::scale(v));
        }, "v"_a, D(Transform, scale))
        .def("rotate", [](const Transform4f &t, const Point3f &v, const Float &a) {
            return Transform4f(t * Transform4f::rotate(v, a));
        }, "axis"_a, "angle"_a, D(Transform, rotate))
        .def("perspective", [](const Transform4f &t, const Float &fov, const Float &near, const Float &far) {
            return Transform4f(t * Transform4f::template perspective<4>(fov, near, far));
        }, "fov"_a, "near"_a, "far"_a, D(Transform, perspective))
        .def("orthographic", [](const Transform4f &t, const Float &near, const Float &far) {
            return Transform4f(t * Transform4f::orthographic(near, far));
        }, "near"_a, "far"_a, D(Transform, orthographic))
        .def("look_at", [](const Transform4f &t, const Point3f &origin, const Point3f &target, const Point3f &up) {
            return Transform4f(t * Transform4f::template look_at<4>(origin, target, up));
        }, "origin"_a, "target"_a, "up"_a, D(Transform, look_at))
        .def("from_frame", [](const Transform4f &t, const Frame3f &f) {
            return Transform4f(t * Transform4f::template from_frame<Float>(f));
        }, "frame"_a, D(Transform, from_frame))
        .def("to_frame", [](const Transform4f &t, const Frame3f &f) {
            return Transform4f(t * Transform4f::template to_frame<Float>(f));
        }, "frame"_a, D(Transform, to_frame))
        /// Fields
        .def("inverse",   &Transform4f::inverse, D(Transform, inverse))
        .def("has_scale", &Transform4f::has_scale, D(Transform, has_scale))
        .def_rw("matrix", &Transform4f::matrix)
        .def_rw("inverse_transpose", &Transform4f::inverse_transpose)
        .def_repr(Transform4f);

    if constexpr (dr::is_dynamic_v<Float>)
        trans4.def(nb::init<const ScalarTransform4f &>(), "Broadcast constructor");

    MI_PY_DRJIT_STRUCT(trans4, Transform4f, matrix, inverse_transpose)
}

MI_PY_EXPORT(Transform) {
    MI_PY_IMPORT_TYPES()
    using ScalarSpectrum = scalar_spectrum_t<Spectrum>;

    MI_PY_CHECK_ALIAS(Transform3f, "Transform3f") {
        bind_transform3<Float, Spectrum>(m, "Transform3f");
    }

    MI_PY_CHECK_ALIAS(Transform3d, "Transform3d") {
        bind_transform3<Float64, Spectrum>(m, "Transform3d");
    }

    MI_PY_CHECK_ALIAS(ScalarTransform3f, "ScalarTransform3f") {
        if constexpr (dr::is_dynamic_v<Float>) {
            bind_transform3<ScalarFloat, ScalarSpectrum>(m, "ScalarTransform3f");
            nb::implicitly_convertible<ScalarTransform3f, Transform3f>();
        }
    }

    MI_PY_CHECK_ALIAS(ScalarTransform3d, "ScalarTransform3d") {
        if constexpr (dr::is_dynamic_v<Float>) {
            bind_transform3<ScalarFloat64, ScalarSpectrum>(m, "ScalarTransform3d");
            nb::implicitly_convertible<ScalarTransform3d, ScalarTransform3f>();
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
            nb::implicitly_convertible<ScalarTransform4f, Transform4f>();
        }
    }

    MI_PY_CHECK_ALIAS(ScalarTransform4d, "ScalarTransform4d") {
        if constexpr (dr::is_dynamic_v<Float>) {
            bind_transform4<ScalarFloat64, ScalarSpectrum>(m, "ScalarTransform4d");
            nb::implicitly_convertible<ScalarTransform4d, ScalarTransform4f>();
        }
    }

    nb::implicitly_convertible<Matrix4f, Transform4f>();
}

#if 0
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

        nb::class_<Keyframe>(atrafo, "Keyframe")
            .def(nb::init<float, _Matrix3f, _Quaternion4f, _Vector3f>())
            .def_rw("time",  &Keyframe::time,  D(AnimatedTransform, Keyframe, time))
            .def_rw("scale", &Keyframe::scale, D(AnimatedTransform, Keyframe, scale))
            .def_rw("quat",  &Keyframe::quat,  D(AnimatedTransform, Keyframe, quat))
            .def_rw("trans", &Keyframe::trans, D(AnimatedTransform, Keyframe, trans));

        atrafo.def(nb::init<>())
            .def(nb::init<const _Transform4f &>())
            .def_method(AnimatedTransform, size)
            .def_method(AnimatedTransform, has_scale)
            .def("__len__", &AnimatedTransform::size)
            .def("__getitem__", [](const AnimatedTransform &trafo, size_t index) {
                if (index >= trafo.size())
                    throw nb::index_error();
                return trafo[index];
            })
            .def("append",
                nb::overload_cast<_Float, const _Transform4f &>(&AnimatedTransform::append),
                D(AnimatedTransform, append))
            .def("append", nb::overload_cast<const Keyframe &>( &AnimatedTransform::append))
            .def("eval", &AnimatedTransform::template eval<Float>,
                 "time"_a, "unused"_a = true, D(AnimatedTransform, eval))
            .def_method(AnimatedTransform, translation_bounds);
    }
}
#endif
