#include <mitsuba/core/warp.h>
#include <mitsuba/python/python.h>
#include <enoki/stl.h>
#include <pybind11/iostream.h>

MTS_PY_EXPORT(warp) {
    MTS_PY_IMPORT_MODULE(warp, "mitsuba.core.warp");

    using FloatArray = py::array_t<Float, py::array::c_style | py::array::forcecast>;

    warp.def(
        "square_to_uniform_disk",
        warp::square_to_uniform_disk<Point2f>,
        "sample"_a, D(warp, square_to_uniform_disk));

    warp.def(
        "square_to_uniform_disk",
        vectorize_wrapper(warp::square_to_uniform_disk<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "uniform_disk_to_square",
        warp::uniform_disk_to_square<Point2f>,
        "p"_a, D(warp, uniform_disk_to_square));

    warp.def(
        "uniform_disk_to_square",
        vectorize_wrapper(warp::uniform_disk_to_square<Point2fP>),
        "p"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_disk_pdf",
        warp::square_to_uniform_disk_pdf<true, Point2f>,
        "p"_a, D(warp, square_to_uniform_disk_pdf));

    warp.def(
        "square_to_uniform_disk_pdf",
        vectorize_wrapper(warp::square_to_uniform_disk_pdf<true, Point2fP>),
        "p"_a);

    // =======================================================================

    warp.def(
        "uniform_disk_to_square_concentric",
        warp::uniform_disk_to_square_concentric<Point2f>,
        "p"_a, D(warp, uniform_disk_to_square_concentric));

    warp.def(
        "uniform_disk_to_square_concentric",
        vectorize_wrapper(warp::uniform_disk_to_square_concentric<Point2fP>),
        "p"_a);

    // =======================================================================
    warp.def(
        "square_to_uniform_disk_concentric",
        warp::square_to_uniform_disk_concentric<Point2f>,
        "sample"_a, D(warp, square_to_uniform_disk_concentric));

    warp.def(
        "square_to_uniform_disk_concentric",
        vectorize_wrapper(warp::square_to_uniform_disk_concentric<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_square_concentric",
        warp::square_to_uniform_square_concentric<Point2f>,
        "sample"_a, D(warp, square_to_uniform_square_concentric));

    warp.def(
        "square_to_uniform_square_concentric",
        vectorize_wrapper(warp::square_to_uniform_square_concentric<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_disk_concentric_pdf",
        warp::square_to_uniform_disk_concentric_pdf<true, Point2f>,
        "p"_a, D(warp, square_to_uniform_disk_concentric_pdf));

    warp.def(
        "square_to_uniform_disk_concentric_pdf",
        vectorize_wrapper(warp::square_to_uniform_disk_concentric_pdf<true, Point2fP>),
        "p"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_triangle",
        warp::square_to_uniform_triangle<Point2f>,
        "sample"_a, D(warp, square_to_uniform_triangle));

    warp.def(
        "square_to_uniform_triangle",
        vectorize_wrapper(warp::square_to_uniform_triangle<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "uniform_triangle_to_square",
        warp::uniform_triangle_to_square<Point2f>,
        "p"_a, D(warp, uniform_triangle_to_square));

    warp.def(
        "uniform_triangle_to_square",
        vectorize_wrapper(warp::uniform_triangle_to_square<Point2fP>),
        "p"_a);

    // =======================================================================

    warp.def(
        "interval_to_tent",
        warp::interval_to_tent<Float>,
        "sample"_a, D(warp, interval_to_tent));

    warp.def(
        "interval_to_tent",
        vectorize_wrapper(warp::interval_to_tent<FloatP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "tent_to_interval",
        warp::tent_to_interval<Float>,
        "value"_a, D(warp, tent_to_interval));

    warp.def(
        "tent_to_interval",
        vectorize_wrapper(warp::tent_to_interval<FloatP>),
        "value"_a);

    // =======================================================================

    warp.def(
        "interval_to_nonuniform_tent",
        warp::interval_to_nonuniform_tent<Float>,
        "a"_a, "b"_a, "c"_a, "d"_a, D(warp, interval_to_nonuniform_tent));

    warp.def(
        "interval_to_nonuniform_tent",
        vectorize_wrapper(warp::interval_to_nonuniform_tent<FloatP>),
        "a"_a, "b"_a, "c"_a, "d"_a);

    // =======================================================================

    warp.def(
        "square_to_tent",
        warp::square_to_tent<Point2f>,
        "sample"_a, D(warp, square_to_tent));

    warp.def(
        "square_to_tent",
        vectorize_wrapper(warp::square_to_tent<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "tent_to_square",
        warp::tent_to_square<Point2f>,
        "value"_a, D(warp, tent_to_square));

    warp.def(
        "tent_to_square",
        vectorize_wrapper(warp::tent_to_square<Point2fP>),
        "value"_a);

    // =======================================================================

    warp.def(
        "square_to_tent_pdf",
        warp::square_to_tent_pdf<Point2f>,
        "v"_a, D(warp, square_to_tent_pdf));

    warp.def(
        "square_to_tent_pdf",
        vectorize_wrapper(warp::square_to_tent_pdf<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_triangle_pdf",
        warp::square_to_uniform_triangle_pdf<true, Point2f>,
        "p"_a, D(warp, square_to_uniform_triangle_pdf));

    warp.def("square_to_uniform_triangle_pdf",
        vectorize_wrapper(warp::square_to_uniform_triangle_pdf<true, Point2fP>),
        "p"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_sphere",
        warp::square_to_uniform_sphere<Point2f>,
        "sample"_a, D(warp, square_to_uniform_sphere));

    warp.def(
        "square_to_uniform_sphere",
        vectorize_wrapper(warp::square_to_uniform_sphere<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "uniform_sphere_to_square",
        warp::uniform_sphere_to_square<Vector3f>,
        "sample"_a, D(warp, uniform_sphere_to_square));

    warp.def(
        "uniform_sphere_to_square",
        vectorize_wrapper(warp::uniform_sphere_to_square<Vector3fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_sphere_pdf",
        warp::square_to_uniform_sphere_pdf<true, Vector3f>,
        "v"_a, D(warp, square_to_uniform_sphere_pdf));

    warp.def(
        "square_to_uniform_sphere_pdf",
        vectorize_wrapper(warp::square_to_uniform_sphere_pdf<true, Vector3fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_hemisphere",
        warp::square_to_uniform_hemisphere<Point2f>,
        "sample"_a, D(warp, square_to_uniform_hemisphere));

    warp.def(
        "square_to_uniform_hemisphere",
        vectorize_wrapper(warp::square_to_uniform_hemisphere<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "uniform_hemisphere_to_square",
        warp::uniform_hemisphere_to_square<Vector3f>,
        "v"_a, D(warp, uniform_hemisphere_to_square));

    warp.def(
        "uniform_hemisphere_to_square",
        vectorize_wrapper(warp::uniform_hemisphere_to_square<Vector3fP>),
        "v"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_hemisphere_pdf",
        warp::square_to_uniform_hemisphere_pdf<true, Vector3f>,
        "v"_a, D(warp, square_to_uniform_hemisphere_pdf));

    warp.def(
        "square_to_uniform_hemisphere_pdf",
        vectorize_wrapper(warp::square_to_uniform_hemisphere_pdf<true, Vector3fP>),
        "v"_a);

    // =======================================================================

    warp.def(
        "square_to_cosine_hemisphere",
        warp::square_to_cosine_hemisphere<Point2f>,
        "sample"_a, D(warp, square_to_cosine_hemisphere));

    warp.def(
        "square_to_cosine_hemisphere",
        vectorize_wrapper(warp::square_to_cosine_hemisphere<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "cosine_hemisphere_to_square",
        warp::cosine_hemisphere_to_square<Vector3f>,
        "v"_a, D(warp, cosine_hemisphere_to_square));

    warp.def(
        "cosine_hemisphere_to_square",
        vectorize_wrapper(warp::cosine_hemisphere_to_square<Vector3fP>),
        "v"_a);

    // =======================================================================

    warp.def(
        "square_to_cosine_hemisphere_pdf",
        warp::square_to_cosine_hemisphere_pdf<true, Vector3f>,
        "v"_a, D(warp, square_to_cosine_hemisphere_pdf));

    warp.def(
        "square_to_cosine_hemisphere_pdf",
        vectorize_wrapper(warp::square_to_cosine_hemisphere_pdf<true, Vector3fP>),
        "v"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_cone",
        warp::square_to_uniform_cone<Point2f>,
        "v"_a, "cos_cutoff"_a, D(warp, square_to_uniform_cone));

    warp.def(
        "square_to_uniform_cone",
        vectorize_wrapper(warp::square_to_uniform_cone<Point2fP>),
        "sample"_a, "cos_cutoff"_a);

    // =======================================================================

    warp.def(
        "uniform_cone_to_square",
        warp::uniform_cone_to_square<Vector3f>,
        "v"_a, "cos_cutoff"_a, D(warp, uniform_cone_to_square));

    warp.def(
        "uniform_cone_to_square",
        vectorize_wrapper(warp::uniform_cone_to_square<Vector3fP>),
        "v"_a, "cos_cutoff"_a);

    // =======================================================================

    warp.def(
        "square_to_uniform_cone_pdf",
        warp::square_to_uniform_cone_pdf<true, Vector3f>,
        "v"_a, "cos_cutoff"_a, D(warp, square_to_uniform_cone_pdf));

    warp.def(
        "square_to_uniform_cone_pdf",
        vectorize_wrapper(warp::square_to_uniform_cone_pdf<true, Vector3fP>),
        "v"_a, "cos_cutoff"_a);

    // =======================================================================

    warp.def(
        "square_to_beckmann",
        warp::square_to_beckmann<Point2f>,
        "sample"_a, "alpha"_a, D(warp, square_to_beckmann));

    warp.def(
        "square_to_beckmann",
        vectorize_wrapper(warp::square_to_beckmann<Point2fP>),
        "sample"_a, "alpha"_a);

    // =======================================================================

    warp.def(
        "beckmann_to_square",
        warp::beckmann_to_square<Vector3f>,
        "v"_a, "alpha"_a, D(warp, beckmann_to_square));

    warp.def(
        "beckmann_to_square",
        vectorize_wrapper(warp::beckmann_to_square<Vector3fP>),
        "v"_a, "alpha"_a);

    // =======================================================================

    warp.def(
        "square_to_beckmann_pdf",
        warp::square_to_beckmann_pdf<Vector3f>,
        "v"_a, "alpha"_a, D(warp, square_to_beckmann_pdf));

    warp.def(
        "square_to_beckmann_pdf",
        vectorize_wrapper(warp::square_to_beckmann_pdf<Vector3fP>),
        "v"_a, "alpha"_a);

    // =======================================================================

    warp.def(
        "square_to_von_mises_fisher",
        warp::square_to_von_mises_fisher<Point2f>,
        "sample"_a, "kappa"_a, D(warp, square_to_von_mises_fisher));

    warp.def(
        "square_to_von_mises_fisher",
        vectorize_wrapper(warp::square_to_von_mises_fisher<Point2fP>),
        "sample"_a, "kappa"_a);

    // =======================================================================

    warp.def(
        "von_mises_fisher_to_square",
        warp::von_mises_fisher_to_square<Vector3f>,
        "v"_a, "kappa"_a, D(warp, von_mises_fisher_to_square));

    warp.def(
        "von_mises_fisher_to_square",
        vectorize_wrapper(warp::von_mises_fisher_to_square<Vector3fP>),
        "v"_a, "kappa"_a);

    // =======================================================================

    warp.def(
        "square_to_von_mises_fisher_pdf",
        warp::square_to_von_mises_fisher_pdf<Vector3f>,
        "v"_a, "kappa"_a, D(warp, square_to_von_mises_fisher_pdf));

    warp.def(
        "square_to_von_mises_fisher_pdf",
        vectorize_wrapper(warp::square_to_von_mises_fisher_pdf<Vector3fP>),
        "v"_a, "kappa"_a);

    // =======================================================================

    warp.def(
        "square_to_rough_fiber",
        warp::square_to_rough_fiber<Point3f, Vector3f>,
        "sample"_a, "wi"_a, "tangent"_a, "kappa"_a,
        D(warp, square_to_rough_fiber));

    warp.def(
        "square_to_rough_fiber",
        vectorize_wrapper(warp::square_to_rough_fiber<Point3fP, Vector3fP>),
        "sample"_a, "wi"_a, "tangent"_a, "kappa"_a);

    // =======================================================================

    warp.def(
        "square_to_rough_fiber_pdf",
        warp::square_to_rough_fiber_pdf<Vector3f>,
        "v"_a, "wi"_a, "tangent"_a, "kappa"_a,
        D(warp, square_to_rough_fiber_pdf));

    warp.def(
        "square_to_rough_fiber_pdf",
        vectorize_wrapper(warp::square_to_rough_fiber_pdf<Vector3fP>),
        "sample"_a, "wi"_a, "tangent"_a, "kappa"_a);

    // =======================================================================

    warp.def(
        "square_to_std_normal",
        warp::square_to_std_normal<Point2f>,
        "v"_a, D(warp, square_to_std_normal));

    warp.def(
        "square_to_std_normal",
        vectorize_wrapper(warp::square_to_std_normal<Point2fP>),
        "sample"_a);

    // =======================================================================

    warp.def(
        "square_to_std_normal_pdf",
        warp::square_to_std_normal_pdf<Point2f>,
        "v"_a, D(warp, square_to_std_normal_pdf));

    warp.def(
        "square_to_std_normal_pdf",
        vectorize_wrapper(warp::square_to_std_normal_pdf<Point2fP>),
        "sample"_a);

    // =======================================================================

    using Marginal2D0 = warp::Marginal2D<0>;
    using Marginal2D1 = warp::Marginal2D<1>;
    using Marginal2D2 = warp::Marginal2D<2>;
    using Marginal2D3 = warp::Marginal2D<3>;

    py::class_<Marginal2D0>(warp, "Marginal2D0", D(warp, Marginal2D))
        .def(py::init([](FloatArray data, bool normalize, bool build_cdf) {
            if (data.ndim() != 2)
                throw std::domain_error("data array has incorrect dimension");
            return Marginal2D0(
                Vector2u((uint32_t) data.shape(1), (uint32_t) data.shape(0)),
                data.data(), {}, {}, normalize, build_cdf
            );
        }), "data"_a, "normalize"_a = true, "build_cdf"_a = true)
        .def("sample", [](const Marginal2D0 *lw, const Vector2f &sample, bool active) {
                return lw->sample(sample, (const Float *) nullptr, active);
            }, "sample"_a, "active"_a = true, D(warp, Marginal2D, sample))
        .def("sample", enoki::vectorize_wrapper(
            [](const Marginal2D0 *lw, const Vector2fP &sample, MaskP active) {
                return lw->sample(sample, (const FloatP *) nullptr, active);
            }), "sample"_a, "active"_a = true)
        .def("invert", [](const Marginal2D0 *lw, const Vector2f &invert, bool active) {
                return lw->invert(invert, (const Float *) nullptr, active);
            }, "sample"_a, "active"_a = true, D(warp, Marginal2D, invert))
        .def("invert", enoki::vectorize_wrapper(
            [](const Marginal2D0 *lw, const Vector2fP &invert, MaskP active) {
                return lw->invert(invert, (const FloatP *) nullptr, active);
            }), "sample"_a, "active"_a = true)
        .def("eval", [](const Marginal2D0 *lw, const Vector2f &pos, bool active) {
                return lw->eval(pos, (const Float *) nullptr, active);
            }, "pos"_a, "active"_a = true, D(warp, Marginal2D, eval))
        .def("eval", enoki::vectorize_wrapper(
            [](const Marginal2D0 *lw, const Vector2fP &pos, MaskP active) {
                return lw->eval(pos, (const FloatP *) nullptr, active);
            }), "pos"_a, "active"_a = true)
        .def("__repr__", &Marginal2D0::to_string);
        ;

    py::class_<Marginal2D1>(warp, "Marginal2D1")
        .def(py::init([](FloatArray data, std::vector<std::vector<Float>> param_values,
                         bool normalize, bool build_cdf) {
            if (data.ndim() != 3)
                throw std::domain_error("data array has incorrect dimension");
            if (param_values.size() == 1 || (uint32_t) param_values[0].size() != (uint32_t) data.shape(0))
                throw std::domain_error("param_values array has incorrect dimension");
            return Marginal2D1(
                Vector2u((uint32_t) data.shape(2), (uint32_t) data.shape(1)),
                data.data(), {{ (uint32_t) param_values[0].size() }},
                {{ param_values[0].data() }}, normalize, build_cdf
            );
        }), "data"_a, "param_values"_a, "normalize"_a = true, "build_cdf"_a = true, D(warp, Marginal2D))
        .def("sample", [](const Marginal2D1 *lw, const Vector2f &sample,
                          Float param1, bool active) {
                Float params[1] = { param1 };
                return lw->sample(sample, params, active);
            }, "sample"_a, "param1"_a, "active"_a = true, D(warp, Marginal2D, sample))
        .def("sample", enoki::vectorize_wrapper(
            [](const Marginal2D1 *lw, const Vector2fP &sample,
                FloatP param1, MaskP active) {
                FloatP params[1] = { param1 };
                return lw->sample(sample, params, active);
            }), "sample"_a, "param1"_a, "active"_a = true)
        .def("invert", [](const Marginal2D1 *lw, const Vector2f &invert,
                          Float param1, bool active) {
                Float params[1] = { param1 };
                return lw->invert(invert, params, active);
            }, "sample"_a, "param1"_a, "active"_a = true, D(warp, Marginal2D, invert))
        .def("invert", enoki::vectorize_wrapper(
            [](const Marginal2D1 *lw, const Vector2fP &invert,
                FloatP param1, MaskP active) {
                FloatP params[1] = { param1 };
                return lw->invert(invert, params, active);
            }), "sample"_a, "param1"_a, "active"_a = true)
        .def("eval", [](const Marginal2D1 *lw, const Vector2f &pos,
                       Float param1, bool active) {
                Float params[1] = { param1 };
                return lw->eval(pos, params, active);
            }, "pos"_a, "param1"_a, "active"_a = true, D(warp, Marginal2D, eval))
        .def("eval", enoki::vectorize_wrapper(
            [](const Marginal2D1 *lw, const Vector2fP &pos,
                FloatP param1, MaskP active) {
                FloatP params[1] = { param1 };
                return lw->eval(pos, params, active);
            }), "pos"_a, "param1"_a, "active"_a = true)
        .def("__repr__", &Marginal2D1::to_string);

    py::class_<Marginal2D2>(warp, "Marginal2D2")
        .def(py::init([](FloatArray data, std::vector<std::vector<Float>> param_values,
                         bool normalize, bool build_cdf) {
            if (data.ndim() != 4)
                throw std::domain_error("data array has incorrect dimension");
            if (param_values.size() != 2 ||
                (uint32_t) param_values[0].size() != (uint32_t) data.shape(0) ||
                (uint32_t) param_values[1].size() != (uint32_t) data.shape(1))
                throw std::domain_error("param_values array has incorrect dimension");
            return Marginal2D2(
                Vector2u((uint32_t) data.shape(3), (uint32_t) data.shape(2)),
                data.data(), {{ (uint32_t) param_values[0].size(), (uint32_t) param_values[1].size() }},
                {{ param_values[0].data(), param_values[1].data() }}, normalize, build_cdf
            );
        }), "data"_a, "param_values"_a, "normalize"_a = true, "build_cdf"_a = true, D(warp, Marginal2D))
        .def("sample", [](const Marginal2D2 *lw, const Vector2f &sample,
                          Float param1, Float param2, bool active) {
                Float params[2] = { param1, param2 };
                return lw->sample(sample, params, active);
            }, "sample"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Marginal2D, sample))
        .def("sample", enoki::vectorize_wrapper(
            [](const Marginal2D2 *lw, const Vector2fP &sample,
                FloatP param1, FloatP param2, MaskP active) {
                FloatP params[2] = { param1, param2 };
                return lw->sample(sample, params, active);
            }), "sample"_a, "param1"_a, "param2"_a, "active"_a = true)
        .def("invert", [](const Marginal2D2 *lw, const Vector2f &invert,
                          Float param1, Float param2, bool active) {
                Float params[2] = { param1, param2 };
                return lw->invert(invert, params, active);
            }, "sample"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Marginal2D, invert))
        .def("invert", enoki::vectorize_wrapper(
            [](const Marginal2D2 *lw, const Vector2fP &invert,
                FloatP param1, FloatP param2, MaskP active) {
                FloatP params[2] = { param1, param2 };
                return lw->invert(invert, params, active);
            }), "sample"_a, "param1"_a, "param2"_a, "active"_a = true)
        .def("eval", [](const Marginal2D2 *lw, const Vector2f &pos,
                       Float param1, Float param2, bool active) {
                Float params[2] = { param1, param2 };
                return lw->eval(pos, params, active);
            }, "pos"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Marginal2D, eval))
        .def("eval", enoki::vectorize_wrapper(
            [](const Marginal2D2 *lw, const Vector2fP &pos,
                FloatP param1, FloatP param2, MaskP active) {
                FloatP params[2] = { param1, param2 };
                return lw->eval(pos, params, active);
            }), "pos"_a, "param1"_a, "param2"_a, "active"_a = true)
        .def("__repr__", &Marginal2D2::to_string);

    py::class_<Marginal2D3>(warp, "Marginal2D3")
        .def(py::init([](FloatArray data, std::vector<std::vector<Float>> param_values,
                         bool normalize, bool build_cdf) {
            if (data.ndim() != 5)
                throw std::domain_error("data array has incorrect dimension");
            if (param_values.size() != 3 ||
                (uint32_t) param_values[0].size() != (uint32_t) data.shape(0) ||
                (uint32_t) param_values[1].size() != (uint32_t) data.shape(1) ||
                (uint32_t) param_values[2].size() != (uint32_t) data.shape(2))
                throw std::domain_error("param_values array has incorrect dimension");
            return Marginal2D3(
                Vector2u((uint32_t) data.shape(4), (uint32_t) data.shape(3)),
                data.data(),
                { { (uint32_t) param_values[0].size(),
                    (uint32_t) param_values[1].size(),
                    (uint32_t) param_values[2].size() } },
                { { param_values[0].data(), param_values[1].data(),
                    param_values[2].data() } },
                normalize, build_cdf);
        }), "data"_a, "param_values"_a, "normalize"_a = true, "build_cdf"_a = true, D(warp, Marginal2D))
        .def("invert", [](const Marginal2D3 *lw, const Vector2f &invert,
                          Float param1, Float param2, Float param3, bool active) {
                Float params[3] = { param1, param2, param3 };
                return lw->invert(invert, params, active);
            }, "sample"_a, "param1"_a, "param2"_a, "param3"_a,
               "active"_a = true, D(warp, Marginal2D, invert))
        .def("invert", enoki::vectorize_wrapper(
            [](const Marginal2D3 *lw, const Vector2fP &invert,
                FloatP param1, FloatP param2, FloatP param3, MaskP active) {
                FloatP params[3] = { param1, param2, param3 };
                return lw->invert(invert, params, active);
            }), "sample"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true)
        .def("sample", [](const Marginal2D3 *lw, const Vector2f &sample,
                          Float param1, Float param2, Float param3, bool active) {
                Float params[3] = { param1, param2, param3 };
                return lw->sample(sample, params, active);
            }, "sample"_a, "param1"_a, "param2"_a, "param3"_a,
               "active"_a = true, D(warp, Marginal2D, sample))
        .def("sample", enoki::vectorize_wrapper(
            [](const Marginal2D3 *lw, const Vector2fP &sample,
                FloatP param1, FloatP param2, FloatP param3, MaskP active) {
                FloatP params[3] = { param1, param2, param3 };
                return lw->sample(sample, params, active);
            }), "sample"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true)
        .def("eval", [](const Marginal2D3 *lw, const Vector2f &pos,
                       Float param1, Float param2, Float param3, bool active) {
                Float params[3] = { param1, param2, param3 };
                return lw->eval(pos, params, active);
            }, "pos"_a, "param1"_a, "param2"_a, "param3"_a,
               "active"_a = true, D(warp, Marginal2D, eval))
        .def("eval", enoki::vectorize_wrapper(
            [](const Marginal2D3 *lw, const Vector2fP &pos,
                FloatP param1, FloatP param2, FloatP param3, MaskP active) {
                FloatP params[3] = { param1, param2, param3 };
                return lw->eval(pos, params, active);
            }), "pos"_a, "param1"_a, "param2"_a,
                "param3"_a, "active"_a = true)
        .def("__repr__", &Marginal2D3::to_string);

    using Hierarchical2D0 = warp::Hierarchical2D<0>;
    using Hierarchical2D1 = warp::Hierarchical2D<1>;
    using Hierarchical2D2 = warp::Hierarchical2D<2>;
    using Hierarchical2D3 = warp::Hierarchical2D<3>;

    py::class_<Hierarchical2D0>(warp, "Hierarchical2D0", D(warp, Hierarchical2D))
        .def(py::init([](FloatArray data, bool normalize, bool build_hierarchy) {
            if (data.ndim() != 2)
                throw std::domain_error("data array has incorrect dimension");
            return Hierarchical2D0(
                Vector2u((uint32_t) data.shape(1), (uint32_t) data.shape(0)),
                data.data(), {}, {}, normalize, build_hierarchy
            );
        }), "data"_a, "normalize"_a = true, "build_hierarchy"_a = true)
        .def("sample", [](const Hierarchical2D0 *lw, const Vector2f &sample, bool active) {
                return lw->sample(sample, (const Float *) nullptr, active);
            }, "sample"_a, "active"_a = true, D(warp, Hierarchical2D, sample))
        .def("sample", enoki::vectorize_wrapper(
            [](const Hierarchical2D0 *lw, const Vector2fP &sample, MaskP active) {
                return lw->sample(sample, (const FloatP *) nullptr, active);
            }), "sample"_a, "active"_a = true)
        .def("invert", [](const Hierarchical2D0 *lw, const Vector2f &invert, bool active) {
                return lw->invert(invert, (const Float *) nullptr, active);
            }, "sample"_a, "active"_a = true, D(warp, Hierarchical2D, invert))
        .def("invert", enoki::vectorize_wrapper(
            [](const Hierarchical2D0 *lw, const Vector2fP &invert, MaskP active) {
                return lw->invert(invert, (const FloatP *) nullptr, active);
            }), "sample"_a, "active"_a = true)
        .def("eval", [](const Hierarchical2D0 *lw, const Vector2f &pos, bool active) {
                return lw->eval(pos, (const Float *) nullptr, active);
            }, "pos"_a, "active"_a = true, D(warp, Hierarchical2D, eval))
        .def("eval", enoki::vectorize_wrapper(
            [](const Hierarchical2D0 *lw, const Vector2fP &pos, MaskP active) {
                return lw->eval(pos, (const FloatP *) nullptr, active);
            }), "pos"_a, "active"_a = true)
        .def("__repr__", &Hierarchical2D0::to_string);
        ;

    py::class_<Hierarchical2D1>(warp, "Hierarchical2D1")
        .def(py::init([](FloatArray data, std::vector<std::vector<Float>> param_values,
                         bool normalize, bool build_hierarchy) {
            if (data.ndim() != 3)
                throw std::domain_error("data array has incorrect dimension");
            if (param_values.size() == 1 || (uint32_t) param_values[0].size() != (uint32_t) data.shape(0))
                throw std::domain_error("param_values array has incorrect dimension");
            return Hierarchical2D1(
                Vector2u((uint32_t) data.shape(2), (uint32_t) data.shape(1)),
                data.data(), {{ (uint32_t) param_values[0].size() }},
                {{ param_values[0].data() }}, normalize, build_hierarchy
            );
        }), "data"_a, "param_values"_a, "normalize"_a = true, "build_hierarchy"_a = true, D(warp, Hierarchical2D))
        .def("sample", [](const Hierarchical2D1 *lw, const Vector2f &sample,
                          Float param1, bool active) {
                Float params[1] = { param1 };
                return lw->sample(sample, params, active);
            }, "sample"_a, "param1"_a, "active"_a = true, D(warp, Hierarchical2D, sample))
        .def("sample", enoki::vectorize_wrapper(
            [](const Hierarchical2D1 *lw, const Vector2fP &sample,
                FloatP param1, MaskP active) {
                FloatP params[1] = { param1 };
                return lw->sample(sample, params, active);
            }), "sample"_a, "param1"_a, "active"_a = true)
        .def("invert", [](const Hierarchical2D1 *lw, const Vector2f &invert,
                          Float param1, bool active) {
                Float params[1] = { param1 };
                return lw->invert(invert, params, active);
            }, "sample"_a, "param1"_a, "active"_a = true, D(warp, Hierarchical2D, invert))
        .def("invert", enoki::vectorize_wrapper(
            [](const Hierarchical2D1 *lw, const Vector2fP &invert,
                FloatP param1, MaskP active) {
                FloatP params[1] = { param1 };
                return lw->invert(invert, params, active);
            }), "sample"_a, "param1"_a, "active"_a = true)
        .def("eval", [](const Hierarchical2D1 *lw, const Vector2f &pos,
                       Float param1, bool active) {
                Float params[1] = { param1 };
                return lw->eval(pos, params, active);
            }, "pos"_a, "param1"_a, "active"_a = true, D(warp, Hierarchical2D, eval))
        .def("eval", enoki::vectorize_wrapper(
            [](const Hierarchical2D1 *lw, const Vector2fP &pos,
                FloatP param1, MaskP active) {
                FloatP params[1] = { param1 };
                return lw->eval(pos, params, active);
            }), "pos"_a, "param1"_a, "active"_a = true)
        .def("__repr__", &Hierarchical2D1::to_string);

    py::class_<Hierarchical2D2>(warp, "Hierarchical2D2")
        .def(py::init([](FloatArray data, std::vector<std::vector<Float>> param_values,
                         bool normalize, bool build_hierarchy) {
            if (data.ndim() != 4)
                throw std::domain_error("data array has incorrect dimension");
            if (param_values.size() != 2 ||
                (uint32_t) param_values[0].size() != (uint32_t) data.shape(0) ||
                (uint32_t) param_values[1].size() != (uint32_t) data.shape(1))
                throw std::domain_error("param_values array has incorrect dimension");
            return Hierarchical2D2(
                Vector2u((uint32_t) data.shape(3), (uint32_t) data.shape(2)),
                data.data(), {{ (uint32_t) param_values[0].size(), (uint32_t) param_values[1].size() }},
                {{ param_values[0].data(), param_values[1].data() }}, normalize, build_hierarchy
            );
        }), "data"_a, "param_values"_a, "normalize"_a = true, "build_hierarchy"_a = true, D(warp, Hierarchical2D))
        .def("sample", [](const Hierarchical2D2 *lw, const Vector2f &sample,
                          Float param1, Float param2, bool active) {
                Float params[2] = { param1, param2 };
                return lw->sample(sample, params, active);
            }, "sample"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Hierarchical2D, sample))
        .def("sample", enoki::vectorize_wrapper(
            [](const Hierarchical2D2 *lw, const Vector2fP &sample,
                FloatP param1, FloatP param2, MaskP active) {
                FloatP params[2] = { param1, param2 };
                return lw->sample(sample, params, active);
            }), "sample"_a, "param1"_a, "param2"_a, "active"_a = true)
        .def("invert", [](const Hierarchical2D2 *lw, const Vector2f &invert,
                          Float param1, Float param2, bool active) {
                Float params[2] = { param1, param2 };
                return lw->invert(invert, params, active);
            }, "sample"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Hierarchical2D, invert))
        .def("invert", enoki::vectorize_wrapper(
            [](const Hierarchical2D2 *lw, const Vector2fP &invert,
                FloatP param1, FloatP param2, MaskP active) {
                FloatP params[2] = { param1, param2 };
                return lw->invert(invert, params, active);
            }), "sample"_a, "param1"_a, "param2"_a, "active"_a = true)
        .def("eval", [](const Hierarchical2D2 *lw, const Vector2f &pos,
                       Float param1, Float param2, bool active) {
                Float params[2] = { param1, param2 };
                return lw->eval(pos, params, active);
            }, "pos"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Hierarchical2D, eval))
        .def("eval", enoki::vectorize_wrapper(
            [](const Hierarchical2D2 *lw, const Vector2fP &pos,
                FloatP param1, FloatP param2, MaskP active) {
                FloatP params[2] = { param1, param2 };
                return lw->eval(pos, params, active);
            }), "pos"_a, "param1"_a, "param2"_a, "active"_a = true)
        .def("__repr__", &Hierarchical2D2::to_string);

    py::class_<Hierarchical2D3>(warp, "Hierarchical2D3")
        .def(py::init([](FloatArray data, std::vector<std::vector<Float>> param_values,
                         bool normalize, bool build_hierarchy) {
            if (data.ndim() != 5)
                throw std::domain_error("data array has incorrect dimension");
            if (param_values.size() != 3 ||
                (uint32_t) param_values[0].size() != (uint32_t) data.shape(0) ||
                (uint32_t) param_values[1].size() != (uint32_t) data.shape(1) ||
                (uint32_t) param_values[2].size() != (uint32_t) data.shape(2))
                throw std::domain_error("param_values array has incorrect dimension");
            return Hierarchical2D3(
                Vector2u((uint32_t) data.shape(4), (uint32_t) data.shape(3)),
                data.data(),
                { { (uint32_t) param_values[0].size(),
                    (uint32_t) param_values[1].size(),
                    (uint32_t) param_values[2].size() } },
                { { param_values[0].data(), param_values[1].data(),
                    param_values[2].data() } },
                normalize, build_hierarchy);
        }), "data"_a, "param_values"_a, "normalize"_a = true, "build_hierarchy"_a = true, D(warp, Hierarchical2D))
        .def("invert", [](const Hierarchical2D3 *lw, const Vector2f &invert,
                          Float param1, Float param2, Float param3, bool active) {
                Float params[3] = { param1, param2, param3 };
                return lw->invert(invert, params, active);
            }, "sample"_a, "param1"_a, "param2"_a, "param3"_a,
               "active"_a = true, D(warp, Hierarchical2D, invert))
        .def("invert", enoki::vectorize_wrapper(
            [](const Hierarchical2D3 *lw, const Vector2fP &invert,
                FloatP param1, FloatP param2, FloatP param3, MaskP active) {
                FloatP params[3] = { param1, param2, param3 };
                return lw->invert(invert, params, active);
            }), "sample"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true)
        .def("sample", [](const Hierarchical2D3 *lw, const Vector2f &sample,
                          Float param1, Float param2, Float param3, bool active) {
                Float params[3] = { param1, param2, param3 };
                return lw->sample(sample, params, active);
            }, "sample"_a, "param1"_a, "param2"_a, "param3"_a,
               "active"_a = true, D(warp, Hierarchical2D, sample))
        .def("sample", enoki::vectorize_wrapper(
            [](const Hierarchical2D3 *lw, const Vector2fP &sample,
                FloatP param1, FloatP param2, FloatP param3, MaskP active) {
                FloatP params[3] = { param1, param2, param3 };
                return lw->sample(sample, params, active);
            }), "sample"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true)
        .def("eval", [](const Hierarchical2D3 *lw, const Vector2f &pos,
                       Float param1, Float param2, Float param3, bool active) {
                Float params[3] = { param1, param2, param3 };
                return lw->eval(pos, params, active);
            }, "pos"_a, "param1"_a, "param2"_a, "param3"_a,
               "active"_a = true, D(warp, Hierarchical2D, eval))
        .def("eval", enoki::vectorize_wrapper(
            [](const Hierarchical2D3 *lw, const Vector2fP &pos,
                FloatP param1, FloatP param2, FloatP param3, MaskP active) {
                FloatP params[3] = { param1, param2, param3 };
                return lw->eval(pos, params, active);
            }), "pos"_a, "param1"_a, "param2"_a,
                "param3"_a, "active"_a = true)
        .def("__repr__", &Hierarchical2D3::to_string);
}
