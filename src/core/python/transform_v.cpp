#include <mitsuba/core/bbox.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/frame.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/list.h>
#include <nanobind/ndarray.h>

inline void transform_affine_is_deprecated_warning() {
    if (PyErr_WarnEx(PyExc_DeprecationWarning,
            "transform_affine() is deprecated and will be removed in a future version. "
            "Use the @ operator instead, which is now optimized for affine transforms.", 1) < 0) {
        nb::raise_python_error();
    }
}

template <typename Transform, typename Float, typename Spectrum, size_t Dimension>
void bind_transform(nb::module_ &m, const char *name) {
    // Check if already bound, if so just return existing handle
    if (auto h = nb::type<Transform>(); h.is_valid()) {
        m.attr(name) = h;
        return;
    }

    MI_IMPORT_CORE_TYPES()

    using ScalarType = dr::scalar_t<Float>;
    using PointType = Point<Float, Dimension - 1>;
    using VectorType = Vector<Float, Dimension - 1>;
    using MatrixType = dr::Matrix<Float, Dimension>;
    using RayType = Ray<PointType, Spectrum>;
    using ScalarMatrix = dr::Matrix<ScalarType, Dimension>;
    using ScalarPoint = Point<ScalarType, Dimension>;
    using NdMatrix = nb::ndarray<ScalarType, nb::shape<Dimension, Dimension>,
                                 nb::c_contig, nb::device::cpu>;

    constexpr bool IsAffine = Transform::IsAffine;
    constexpr bool IsProjective = !IsAffine;

    auto cls = nb::class_<Transform>(m, name, D(Transform))
        .def(nb::init<>(), "Initialize with the identity matrix")
        .def(nb::init<const MatrixType &>(), "Construct from a matrix")
        .def(nb::init<const MatrixType &, const MatrixType &>(), "Construct from a matrix and its inverse transpose")
        .def(nb::init<const AffineTransform<typename Transform::Point> &>(), "Construct from an affine transformation")
        .def(nb::init<const ProjectiveTransform<typename Transform::Point> &>(), "Construct from an projective transformation")

        // Matrix initialization
        .def("__init__", [](Transform *t, NdMatrix a) {
            auto v = a.view();
            ScalarMatrix m;
            for (size_t i = 0; i < Dimension; ++i)
                for (size_t j = 0; j < Dimension; ++j)
                    m.entry(i, j) = v(i, j);
            new (t) Transform(m);
        })
        .def(nb::init<MatrixType>(), D(Transform, Transform))
        .def(nb::init<MatrixType, MatrixType>(), "Initialize from a matrix and its inverse transpose")

        // List initialization
        .def("__init__", [](Transform *t, const nb::sequence &seq) {
            using Row = dr::value_t<MatrixType>;

            size_t size = nb::len(seq);
            if (size != Dimension)
                throw nb::next_overload();

            MatrixType m;
            for (size_t i = 0; i < size; ++i) {
                if (!nb::try_cast<Row>(seq[i], m.entry(i)))
                    throw nb::next_overload();
            }

            new (t) Transform(m);
        })

        /// Operators
        .def(nb::self == nb::self)
        .def(nb::self != nb::self)

        // Matrix multiplication operator
        .def("__mul__", [](nb::handle, nb::handle) {
            throw std::runtime_error("mul(): please use the matrix "
                "multiplication operator '@' instead.");
        }, nb::is_operator())

        // Transform concatenation
        .def("__matmul__", [](const Transform &a, const Transform &b) {
            return a * b;
        }, nb::is_operator())

        // Point transformation
        .def("__matmul__", [](const Transform &a, const PointType &b) {
            return a * b;
        }, nb::is_operator())

        // Vector transformation
        .def("__matmul__", [](const Transform &a, const VectorType &b) {
            return a * b;
        }, nb::is_operator())

        // Ray transformation
        .def("__matmul__", [](const Transform &a, const RayType &b) {
            return a * b;
        }, nb::is_operator());

    // Deprecated transform_affine methods with warnings
    cls.def("transform_affine", [](const Transform &a, const PointType &b) {
            transform_affine_is_deprecated_warning();
            return a * b;
        }, "p"_a, D(Transform, transform_affine))
        .def("transform_affine", [](const Transform &a, const VectorType &b) {
            transform_affine_is_deprecated_warning();
            return a * b;
        }, "v"_a, D(Transform, transform_affine))
        .def("transform_affine", [](const Transform &a, const RayType &b) {
            transform_affine_is_deprecated_warning();
            return a * b;
        }, "ray"_a, D(Transform, transform_affine));

    if constexpr (Dimension == 4) {
        // Normal transformation
        cls.def("__matmul__", [](const Transform &a, const Normal3f &b) {
            return a * b;
        }, nb::is_operator());

        cls.def("transform_affine", [](const Transform &a, const Normal3f &b) {
            transform_affine_is_deprecated_warning();
            return a * b;
        }, "n"_a, D(Transform, transform_affine));
    }

    // Chain transformations
    cls.def("translate", [](const Transform &t, const VectorType &v) {
            return Transform(t * Transform::translate(v));
        }, "v"_a, D(Transform, translate))
        .def("scale", [](const Transform &t, const VectorType &v) {
            return Transform(t * Transform::scale(v));
        }, "v"_a, D(Transform, scale));

    // Rotation methods
    if constexpr (Dimension == 3) {
        cls.def("rotate", [](const Transform &t, const Float &a) {
            return Transform(t * Transform::template rotate<3>(a));
        }, "angle"_a, D(Transform, rotate, 2));
    } else if constexpr (Dimension == 4) {
        cls.def("rotate", [](const Transform &t, const VectorType &v, const Float &a) {
            return Transform(t * Transform::rotate(v, a));
        }, "axis"_a, "angle"_a, D(Transform, rotate));
    }

    // 4D-specific methods
    if constexpr (Dimension == 4) {
        cls.def("orthographic", [](const Transform &t, const Float &near, const Float &far) {
                return Transform(t * Transform::orthographic(near, far));
            }, "near"_a, "far"_a, D(Transform, orthographic))
            .def("look_at", [](const Transform &t, const PointType &origin,
                              const PointType &target, const VectorType &up) {
                return Transform(t * Transform::template look_at<4>(origin, target, up));
            }, "origin"_a, "target"_a, "up"_a, D(Transform, look_at))
            .def("from_frame", [](const Transform &t, const Frame3f &f) {
                return Transform(t * Transform::template from_frame<Float>(f));
            }, "frame"_a, D(Transform, from_frame))
            .def("to_frame", [](const Transform &t, const Frame3f &f) {
                return Transform(t * Transform::template to_frame<Float>(f));
            }, "frame"_a, D(Transform, to_frame));

        // Add perspective method only for ProjectiveTransform4
        if constexpr (IsProjective && Dimension == 4) {
            cls.def("perspective", [](const Transform &t, Float fov, Float near, Float far) {
                    return Transform(t * Transform::perspective(fov, near, far));
                }, "fov"_a, "near"_a, "far"_a, D(Transform, perspective));
        }

    }

    if constexpr (IsAffine)
        cls.def("extract", [](const Transform &t) { return t.extract(); }, D(Transform, extract));

    // Common methods
    cls.def("inverse", &Transform::inverse, D(Transform, inverse))
        .def("translation", &Transform::translation, D(Transform, translation))
        .def("has_scale", &Transform::has_scale, D(Transform, has_scale))
        .def_rw("matrix", &Transform::matrix)
        .def_rw("inverse_transpose", &Transform::inverse_transpose)
        .def_repr(Transform);

    cls.def("update", &Transform::update, nb::rv_policy::reference,
            "Update the inverse transpose part following a modification to 'matrix'");

    // Dynamic variant constructor
    if constexpr (dr::is_dynamic_v<Float>) {
        using ScalarTransform = std::conditional_t<
            IsAffine,
            AffineTransform<ScalarPoint>,
            ProjectiveTransform<ScalarPoint>
        >;
        cls.def(nb::init<const ScalarTransform &>(), "Broadcast constructor");
    }

    // Patch methods to be callable as Transform().f() and Transform.f()
    nb::module_::import_("mitsuba.detail").attr("patch_transform")(cls);

    nb::implicitly_convertible<MatrixType, Transform>();

    MI_PY_DRJIT_STRUCT(cls, Transform, matrix, inverse_transpose)
}

MI_PY_EXPORT(Transform) {
    MI_PY_IMPORT_TYPES()
    using ScalarSpectrum = scalar_spectrum_t<Spectrum>;

    // Vectorized types
    bind_transform<AffineTransform3f, Float, Spectrum, 3>(m, "AffineTransform3f");
    bind_transform<AffineTransform3d, Float64, Spectrum, 3>(m, "AffineTransform3d");
    bind_transform<AffineTransform4f, Float, Spectrum, 4>(m, "AffineTransform4f");
    bind_transform<AffineTransform4d, Float64, Spectrum, 4>(m, "AffineTransform4d");
    bind_transform<ProjectiveTransform3f, Float, Spectrum, 3>(m, "ProjectiveTransform3f");
    bind_transform<ProjectiveTransform4f, Float, Spectrum, 4>(m, "ProjectiveTransform4f");
    bind_transform<ProjectiveTransform3d, Float64, Spectrum, 3>(m, "ProjectiveTransform3d");
    bind_transform<ProjectiveTransform4d, Float64, Spectrum, 4>(m, "ProjectiveTransform4d");

    // Scalar types
    bind_transform<ScalarAffineTransform3f, ScalarFloat, ScalarSpectrum, 3>(m, "ScalarAffineTransform3f");
    bind_transform<ScalarAffineTransform3d, ScalarFloat64, ScalarSpectrum, 3>(m, "ScalarAffineTransform3d");
    bind_transform<ScalarAffineTransform4f, ScalarFloat, ScalarSpectrum, 4>(m, "ScalarAffineTransform4f");
    bind_transform<ScalarAffineTransform4d, ScalarFloat64, ScalarSpectrum, 4>(m, "ScalarAffineTransform4d");
    bind_transform<ScalarProjectiveTransform3f, ScalarFloat, ScalarSpectrum, 3>(m, "ScalarProjectiveTransform3f");
    bind_transform<ScalarProjectiveTransform3d, ScalarFloat64, ScalarSpectrum, 3>(m, "ScalarProjectiveTransform3d");
    bind_transform<ScalarProjectiveTransform4f, ScalarFloat, ScalarSpectrum, 4>(m, "ScalarProjectiveTransform4f");
    bind_transform<ScalarProjectiveTransform4d, ScalarFloat64, ScalarSpectrum, 4>(m, "ScalarProjectiveTransform4d");

    // Implicit conversions: scalar->vector
    nb::implicitly_convertible<ScalarAffineTransform3f, AffineTransform3f>();
    nb::implicitly_convertible<ScalarAffineTransform3d, AffineTransform3d>();
    nb::implicitly_convertible<ScalarAffineTransform4f, AffineTransform4f>();
    nb::implicitly_convertible<ScalarAffineTransform4d, AffineTransform4d>();
    nb::implicitly_convertible<ScalarProjectiveTransform3f, ProjectiveTransform3f>();
    nb::implicitly_convertible<ScalarProjectiveTransform3d, ProjectiveTransform3d>();
    nb::implicitly_convertible<ScalarProjectiveTransform4f, ProjectiveTransform4f>();
    nb::implicitly_convertible<ScalarProjectiveTransform4d, ProjectiveTransform4d>();

    // Create aliases for backward compatibility
    m.attr("Transform3f") = m.attr("AffineTransform3f");
    m.attr("Transform3d") = m.attr("AffineTransform3d");
    m.attr("Transform4f") = m.attr("AffineTransform4f");
    m.attr("Transform4d") = m.attr("AffineTransform4d");
    m.attr("ScalarTransform3f") = m.attr("ScalarAffineTransform3f");
    m.attr("ScalarTransform3d") = m.attr("ScalarAffineTransform3d");
    m.attr("ScalarTransform4f") = m.attr("ScalarAffineTransform4f");
    m.attr("ScalarTransform4d") = m.attr("ScalarAffineTransform4d");
}
