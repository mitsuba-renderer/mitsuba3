#include <mitsuba/python/python.h>
#include <mitsuba/core/warp.h>
#include <enoki/stl.h>
#include <pybind11/iostream.h>

MTS_PY_EXPORT_VARIANTS(warp) {
    MTS_IMPORT_CORE_TYPES()

    m.def(
        "square_to_uniform_disk",
        vectorize<Float>(warp::square_to_uniform_disk<FloatP>),
        "sample"_a, D(warp, square_to_uniform_disk));

    m.def(
        "uniform_disk_to_square",
        vectorize<Float>(warp::uniform_disk_to_square<FloatP>),
        "p"_a, D(warp, uniform_disk_to_square));

    m.def(
        "square_to_uniform_disk_pdf",
        vectorize<Float>(warp::square_to_uniform_disk_pdf<true, FloatP>),
        "p"_a, D(warp, square_to_uniform_disk_pdf));

    m.def(
        "uniform_disk_to_square_concentric",
        vectorize<Float>(warp::uniform_disk_to_square_concentric<FloatP>),
        "p"_a, D(warp, uniform_disk_to_square_concentric));
    m.def(
        "square_to_uniform_disk_concentric",
        vectorize<Float>(warp::square_to_uniform_disk_concentric<FloatP>),
        "sample"_a, D(warp, square_to_uniform_disk_concentric));

    m.def(
        "square_to_uniform_square_concentric",
        vectorize<Float>(warp::square_to_uniform_square_concentric<FloatP>),
        "sample"_a, D(warp, square_to_uniform_square_concentric));

    m.def(
        "square_to_uniform_disk_concentric_pdf",
        vectorize<Float>(warp::square_to_uniform_disk_concentric_pdf<true, FloatP>),
        "p"_a, D(warp, square_to_uniform_disk_concentric_pdf));

    m.def(
        "square_to_uniform_triangle",
        vectorize<Float>(warp::square_to_uniform_triangle<FloatP>),
        "sample"_a, D(warp, square_to_uniform_triangle));

    m.def(
        "uniform_triangle_to_square",
        vectorize<Float>(warp::uniform_triangle_to_square<FloatP>),
        "p"_a, D(warp, uniform_triangle_to_square));

    m.def(
        "interval_to_tent",
        warp::interval_to_tent<Float>,
        "sample"_a, D(warp, interval_to_tent));

    m.def(
        "tent_to_interval",
        warp::tent_to_interval<Float>,
        "value"_a, D(warp, tent_to_interval));

    m.def(
        "interval_to_nonuniform_tent",
        warp::interval_to_nonuniform_tent<Float>,
        "a"_a, "b"_a, "c"_a, "d"_a, D(warp, interval_to_nonuniform_tent));

    m.def(
        "square_to_tent",
        vectorize<Float>(warp::square_to_tent<FloatP>),
        "sample"_a, D(warp, square_to_tent));

    m.def(
        "tent_to_square",
        vectorize<Float>(warp::tent_to_square<FloatP>),
        "value"_a, D(warp, tent_to_square));

    m.def(
        "square_to_tent_pdf",
        vectorize<Float>(warp::square_to_tent_pdf<FloatP>),
        "v"_a, D(warp, square_to_tent_pdf));

    m.def(
        "square_to_uniform_triangle_pdf",
        vectorize<Float>(warp::square_to_uniform_triangle_pdf<true, FloatP>),
        "p"_a, D(warp, square_to_uniform_triangle_pdf));

    m.def(
        "square_to_uniform_sphere",
        vectorize<Float>(warp::square_to_uniform_sphere<FloatP>),
        "sample"_a, D(warp, square_to_uniform_sphere));

    m.def(
        "uniform_sphere_to_square",
        vectorize<Float>(warp::uniform_sphere_to_square<FloatP>),
        "sample"_a, D(warp, uniform_sphere_to_square));

    m.def(
        "square_to_uniform_sphere_pdf",
        vectorize<Float>(warp::square_to_uniform_sphere_pdf<true, FloatP>),
        "v"_a, D(warp, square_to_uniform_sphere_pdf));

    m.def(
        "square_to_uniform_hemisphere",
        vectorize<Float>(warp::square_to_uniform_hemisphere<FloatP>),
        "sample"_a, D(warp, square_to_uniform_hemisphere));

    m.def(
        "uniform_hemisphere_to_square",
        vectorize<Float>(warp::uniform_hemisphere_to_square<FloatP>),
        "v"_a, D(warp, uniform_hemisphere_to_square));

    m.def(
        "square_to_uniform_hemisphere_pdf",
        vectorize<Float>(warp::square_to_uniform_hemisphere_pdf<true, FloatP>),
        "v"_a, D(warp, square_to_uniform_hemisphere_pdf));

    m.def(
        "square_to_cosine_hemisphere",
        vectorize<Float>(warp::square_to_cosine_hemisphere<FloatP>),
        "sample"_a, D(warp, square_to_cosine_hemisphere));

    m.def(
        "cosine_hemisphere_to_square",
        vectorize<Float>(warp::cosine_hemisphere_to_square<FloatP>),
        "v"_a, D(warp, cosine_hemisphere_to_square));

    m.def(
        "square_to_cosine_hemisphere_pdf",
        vectorize<Float>(warp::square_to_cosine_hemisphere_pdf<true, FloatP>),
        "v"_a, D(warp, square_to_cosine_hemisphere_pdf));

    m.def(
        "square_to_uniform_cone",
        vectorize<Float>(warp::square_to_uniform_cone<FloatP>),
        "v"_a, "cos_cutoff"_a, D(warp, square_to_uniform_cone));

    m.def(
        "uniform_cone_to_square",
        vectorize<Float>(warp::uniform_cone_to_square<FloatP>),
        "v"_a, "cos_cutoff"_a, D(warp, uniform_cone_to_square));

    m.def(
        "square_to_uniform_cone_pdf",
        vectorize<Float>(warp::square_to_uniform_cone_pdf<true, FloatP>),
        "v"_a, "cos_cutoff"_a, D(warp, square_to_uniform_cone_pdf));

    m.def(
        "square_to_beckmann",
        vectorize<Float>(warp::square_to_beckmann<FloatP>),
        "sample"_a, "alpha"_a, D(warp, square_to_beckmann));

    m.def(
        "beckmann_to_square",
        vectorize<Float>(warp::beckmann_to_square<FloatP>),
        "v"_a, "alpha"_a, D(warp, beckmann_to_square));

    m.def(
        "square_to_beckmann_pdf",
        vectorize<Float>(warp::square_to_beckmann_pdf<FloatP>),
        "v"_a, "alpha"_a, D(warp, square_to_beckmann_pdf));

    m.def(
        "square_to_von_mises_fisher",
        vectorize<Float>(warp::square_to_von_mises_fisher<FloatP>),
        "sample"_a, "kappa"_a, D(warp, square_to_von_mises_fisher));

    m.def(
        "von_mises_fisher_to_square",
        vectorize<Float>(warp::von_mises_fisher_to_square<FloatP>),
        "v"_a, "kappa"_a, D(warp, von_mises_fisher_to_square));

    m.def(
        "square_to_von_mises_fisher_pdf",
        vectorize<Float>(warp::square_to_von_mises_fisher_pdf<FloatP>),
        "v"_a, "kappa"_a, D(warp, square_to_von_mises_fisher_pdf));

    m.def(
        "square_to_rough_fiber",
        vectorize<Float>(warp::square_to_rough_fiber<FloatP>),
        "sample"_a, "wi"_a, "tangent"_a, "kappa"_a,
        D(warp, square_to_rough_fiber));

    m.def(
        "square_to_rough_fiber_pdf",
        vectorize<Float>(warp::square_to_rough_fiber_pdf<FloatP>),
        "v"_a, "wi"_a, "tangent"_a, "kappa"_a,
        D(warp, square_to_rough_fiber_pdf));

    m.def(
        "square_to_std_normal",
        vectorize<Float>(warp::square_to_std_normal<FloatP>),
        "v"_a, D(warp, square_to_std_normal));

    m.def(
        "square_to_std_normal_pdf",
        vectorize<Float>(warp::square_to_std_normal_pdf<FloatP>),
        "v"_a, D(warp, square_to_std_normal_pdf));

    // --------------------------------------------------------------------

    using FloatArray = py::array_t<ScalarFloat, py::array::c_style | py::array::forcecast>;

    using Marginal2D0 = warp::Marginal2D<Float, 0>;
    using Marginal2D1 = warp::Marginal2D<Float, 1>;
    using Marginal2D2 = warp::Marginal2D<Float, 2>;
    using Marginal2D3 = warp::Marginal2D<Float, 3>;

    using Marginal2D0P = warp::Marginal2D<FloatP, 0>;
    using Marginal2D1P = warp::Marginal2D<FloatP, 1>;
    using Marginal2D2P = warp::Marginal2D<FloatP, 2>;
    using Marginal2D3P = warp::Marginal2D<FloatP, 3>;
    using Vector2fP = Vector<FloatP, 2>;

    MTS_PY_CHECK_ALIAS(Marginal2D0)
    MTS_PY_CHECK_ALIAS(Marginal2D1)
    MTS_PY_CHECK_ALIAS(Marginal2D2)
    MTS_PY_CHECK_ALIAS(Marginal2D3)

    py::class_<Marginal2D0>(m, "Marginal2D0", D(warp, Marginal2D))
        .def(py::init([](FloatArray data, bool normalize, bool build_cdf) {
                 if (data.ndim() != 2)
                     throw std::domain_error("data array has incorrect dimension");
                 return Marginal2D0(
                     ScalarVector2u((uint32_t) data.shape(1), (uint32_t) data.shape(0)),
                     data.data(), {}, {}, normalize, build_cdf);
             }),
             "data"_a, "normalize"_a = true, "build_cdf"_a = true)

        .def("sample",
             vectorize<Float>([](const Marginal2D0P *lw, const Vector2fP &sample, bool active) {
                 return lw->sample(sample, (const FloatP *) nullptr, active);
             }),
             "sample"_a, "active"_a = true, D(warp, Marginal2D, sample))
        .def("invert",
            vectorize<Float>([](const Marginal2D0P *lw, const Vector2fP &invert, bool active) {
                 return lw->invert(invert, (const FloatP *) nullptr, active);
            }),
            "sample"_a, "active"_a = true, D(warp, Marginal2D, invert))
        .def("eval",
             vectorize<Float>([](const Marginal2D0P *lw, const Vector2fP &pos, bool active) {
                 return lw->eval(pos, (const FloatP *) nullptr, active);
            }),
            "pos"_a, "active"_a = true, D(warp, Marginal2D, eval))
        .def("__repr__", &Marginal2D0::to_string);
    ;

    py::class_<Marginal2D1>(m, "Marginal2D1")
        .def(py::init([](FloatArray data, std::vector<std::vector<ScalarFloat>> param_values,
                         bool normalize, bool build_cdf) {
                 if (data.ndim() != 3)
                     throw std::domain_error("data array has incorrect dimension");
                 if (param_values.size() == 1 ||
                     (uint32_t) param_values[0].size() != (uint32_t) data.shape(0))
                     throw std::domain_error("param_values array has incorrect dimension");
                 return Marginal2D1(
                     ScalarVector2u((uint32_t) data.shape(2), (uint32_t) data.shape(1)),
                     data.data(), { { (uint32_t) param_values[0].size() } },
                     { { param_values[0].data() } }, normalize, build_cdf);
             }),
             "data"_a, "param_values"_a, "normalize"_a = true, "build_cdf"_a = true,
             D(warp, Marginal2D))
        .def("sample",
             vectorize<Float>(
                 [](const Marginal2D1P *lw, const Vector2fP &sample, FloatP param1, bool active) {
                     FloatP params[1] = { param1 };
                     return lw->sample(sample, params, active);
                 }),
             "sample"_a, "param1"_a, "active"_a = true, D(warp, Marginal2D, sample))
        .def("invert",
             vectorize<Float>(
                 [](const Marginal2D1P *lw, const Vector2fP &invert, FloatP param1, bool active) {
                     FloatP params[1] = { param1 };
                     return lw->invert(invert, params, active);
                 }),
             "sample"_a, "param1"_a, "active"_a = true, D(warp, Marginal2D, invert))
        .def("eval",
             vectorize<Float>(
                 [](const Marginal2D1P *lw, const Vector2fP &pos, FloatP param1, bool active) {
                     FloatP params[1] = { param1 };
                     return lw->eval(pos, params, active);
                 }),
             "pos"_a, "param1"_a, "active"_a = true, D(warp, Marginal2D, eval))
        .def("__repr__", &Marginal2D1::to_string);

    py::class_<Marginal2D2>(m, "Marginal2D2")
        .def(py::init([](FloatArray data, std::vector<std::vector<ScalarFloat>> param_values,
                         bool normalize, bool build_cdf) {
                 if (data.ndim() != 4)
                     throw std::domain_error("data array has incorrect dimension");
                 if (param_values.size() != 2 ||
                     (uint32_t) param_values[0].size() != (uint32_t) data.shape(0) ||
                     (uint32_t) param_values[1].size() != (uint32_t) data.shape(1))
                     throw std::domain_error("param_values array has incorrect dimension");
                 return Marginal2D2(
                     ScalarVector2u((uint32_t) data.shape(3), (uint32_t) data.shape(2)), data.data(),
                     { { (uint32_t) param_values[0].size(), (uint32_t) param_values[1].size() } },
                     { { param_values[0].data(), param_values[1].data() } }, normalize, build_cdf);
             }),
             "data"_a, "param_values"_a, "normalize"_a = true, "build_cdf"_a = true,
             D(warp, Marginal2D))
        .def("sample",
             vectorize<Float>([](const Marginal2D2P *lw, const Vector2fP &sample, FloatP param1,
                                 FloatP param2, bool active) {
                 FloatP params[2] = { param1, param2 };
                 return lw->sample(sample, params, active);
             }),
             "sample"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Marginal2D, sample))

        .def("invert",
             vectorize<Float>([](const Marginal2D2P *lw, const Vector2fP &invert, FloatP param1,
                                 FloatP param2, bool active) {
                 FloatP params[2] = { param1, param2 };
                 return lw->invert(invert, params, active);
             }),
             "sample"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Marginal2D, invert))
        .def("eval",
             vectorize<Float>([](const Marginal2D2P *lw, const Vector2fP &pos, FloatP param1,
                                 FloatP param2, bool active) {
                 FloatP params[2] = { param1, param2 };
                 return lw->eval(pos, params, active);
             }),
             "pos"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Marginal2D, eval))
        .def("__repr__", &Marginal2D2::to_string);

    py::class_<Marginal2D3>(m, "Marginal2D3")
        .def(py::init([](FloatArray data, std::vector<std::vector<ScalarFloat>> param_values,
                         bool normalize, bool build_cdf) {
                 if (data.ndim() != 5)
                     throw std::domain_error("data array has incorrect dimension");
                 if (param_values.size() != 3 ||
                     (uint32_t) param_values[0].size() != (uint32_t) data.shape(0) ||
                     (uint32_t) param_values[1].size() != (uint32_t) data.shape(1) ||
                     (uint32_t) param_values[2].size() != (uint32_t) data.shape(2))
                     throw std::domain_error("param_values array has incorrect dimension");
                 return Marginal2D3(
                     ScalarVector2u((uint32_t) data.shape(4), (uint32_t) data.shape(3)), data.data(),
                     { { (uint32_t) param_values[0].size(), (uint32_t) param_values[1].size(),
                         (uint32_t) param_values[2].size() } },
                     { { param_values[0].data(), param_values[1].data(), param_values[2].data() } },
                     normalize, build_cdf);
             }),
             "data"_a, "param_values"_a, "normalize"_a = true, "build_cdf"_a = true,
             D(warp, Marginal2D))
        .def("invert",
             vectorize<Float>([](const Marginal2D3P *lw, const Vector2fP &invert, FloatP param1,
                                 FloatP param2, FloatP param3, bool active) {
                 FloatP params[3] = { param1, param2, param3 };
                 return lw->invert(invert, params, active);
             }),
             "sample"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true,
             D(warp, Marginal2D, invert))
        .def("sample",
             vectorize<Float>([](const Marginal2D3P *lw, const Vector2fP &sample, FloatP param1,
                                 FloatP param2, FloatP param3, bool active) {
                 FloatP params[3] = { param1, param2, param3 };
                 return lw->sample(sample, params, active);
             }),
             "sample"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true,
             D(warp, Marginal2D, sample))
        .def("eval",
             vectorize<Float>([](const Marginal2D3P *lw, const Vector2fP &pos, FloatP param1,
                                 FloatP param2, FloatP param3, bool active) {
                 FloatP params[3] = { param1, param2, param3 };
                 return lw->eval(pos, params, active);
             }),
             "pos"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true,
             D(warp, Marginal2D, eval))
        .def("__repr__", &Marginal2D3::to_string);

    // --------------------------------------------------------------------

    using Hierarchical2D0 = warp::Hierarchical2D<Float, 0>;
    using Hierarchical2D1 = warp::Hierarchical2D<Float, 1>;
    using Hierarchical2D2 = warp::Hierarchical2D<Float, 2>;
    using Hierarchical2D3 = warp::Hierarchical2D<Float, 3>;
    using Hierarchical2D0P = warp::Hierarchical2D<FloatP, 0>;
    using Hierarchical2D1P = warp::Hierarchical2D<FloatP, 1>;
    using Hierarchical2D2P = warp::Hierarchical2D<FloatP, 2>;
    using Hierarchical2D3P = warp::Hierarchical2D<FloatP, 3>;

    py::class_<Hierarchical2D0>(m, "Hierarchical2D0", D(warp, Hierarchical2D))
        .def(py::init([](FloatArray data, bool normalize, bool build_hierarchy) {
                 if (data.ndim() != 2)
                     throw std::domain_error("data array has incorrect dimension");
                 return Hierarchical2D0(
                     ScalarVector2u((uint32_t) data.shape(1), (uint32_t) data.shape(0)),
                     data.data(), {}, {}, normalize, build_hierarchy);
             }),
             "data"_a, "normalize"_a = true, "build_hierarchy"_a = true)
        .def("sample",
             vectorize<Float>([](const Hierarchical2D0P *lw, const Vector2fP &sample, bool active) {
                 return lw->sample(sample, (const FloatP *) nullptr, active);
             }),
             "sample"_a, "active"_a = true, D(warp, Hierarchical2D, sample))
        .def("invert",
             vectorize<Float>([](const Hierarchical2D0P *lw, const Vector2fP &invert, bool active) {
                 return lw->invert(invert, (const FloatP *) nullptr, active);
             }),
             "sample"_a, "active"_a = true, D(warp, Hierarchical2D, invert))
        .def("eval",
             vectorize<Float>([](const Hierarchical2D0P *lw, const Vector2fP &pos, bool active) {
                 return lw->eval(pos, (const FloatP *) nullptr, active);
             }),
             "pos"_a, "active"_a = true, D(warp, Hierarchical2D, eval))
        .def("__repr__", &Hierarchical2D0::to_string);
    ;

    py::class_<Hierarchical2D1>(m, "Hierarchical2D1")
        .def(py::init([](FloatArray data, std::vector<std::vector<ScalarFloat>> param_values,
                         bool normalize, bool build_hierarchy) {
                 if (data.ndim() != 3)
                     throw std::domain_error("data array has incorrect dimension");
                 if (param_values.size() == 1 ||
                     (uint32_t) param_values[0].size() != (uint32_t) data.shape(0))
                     throw std::domain_error("param_values array has incorrect dimension");
                 return Hierarchical2D1(
                     ScalarVector2u((uint32_t) data.shape(2), (uint32_t) data.shape(1)), data.data(),
                     { { (uint32_t) param_values[0].size() } }, { { param_values[0].data() } },
                     normalize, build_hierarchy);
             }),
             "data"_a, "param_values"_a, "normalize"_a = true, "build_hierarchy"_a = true,
             D(warp, Hierarchical2D))
        .def("sample",
             vectorize<Float>(
                 [](const Hierarchical2D1P *lw, const Vector2fP &sample, FloatP param1, bool active) {
                     FloatP params[1] = { param1 };
                     return lw->sample(sample, params, active);
                 }),
             "sample"_a, "param1"_a, "active"_a = true, D(warp, Hierarchical2D, sample))
        .def("invert",
             vectorize<Float>(
                 [](const Hierarchical2D1P *lw, const Vector2fP &invert, FloatP param1, bool active) {
                     FloatP params[1] = { param1 };
                     return lw->invert(invert, params, active);
                 }),
             "sample"_a, "param1"_a, "active"_a = true, D(warp, Hierarchical2D, invert))
        .def("eval",
             vectorize<Float>(
                 [](const Hierarchical2D1P *lw, const Vector2fP &pos, FloatP param1, bool active) {
                     FloatP params[1] = { param1 };
                     return lw->eval(pos, params, active);
                 }),
             "pos"_a, "param1"_a, "active"_a = true, D(warp, Hierarchical2D, eval))
        .def("__repr__", &Hierarchical2D1::to_string);

    py::class_<Hierarchical2D2>(m, "Hierarchical2D2")
        .def(py::init([](FloatArray data, std::vector<std::vector<ScalarFloat>> param_values,
                         bool normalize, bool build_hierarchy) {
                 if (data.ndim() != 4)
                     throw std::domain_error("data array has incorrect dimension");
                 if (param_values.size() != 2 ||
                     (uint32_t) param_values[0].size() != (uint32_t) data.shape(0) ||
                     (uint32_t) param_values[1].size() != (uint32_t) data.shape(1))
                     throw std::domain_error("param_values array has incorrect dimension");
                 return Hierarchical2D2(
                     ScalarVector2u((uint32_t) data.shape(3), (uint32_t) data.shape(2)), data.data(),
                     { { (uint32_t) param_values[0].size(), (uint32_t) param_values[1].size() } },
                     { { param_values[0].data(), param_values[1].data() } }, normalize,
                     build_hierarchy);
             }),
             "data"_a, "param_values"_a, "normalize"_a = true, "build_hierarchy"_a = true,
             D(warp, Hierarchical2D))
        .def("sample",
             vectorize<Float>([](const Hierarchical2D2P *lw, const Vector2fP &sample, FloatP param1,
                                 FloatP param2, bool active) {
                 FloatP params[2] = { param1, param2 };
                 return lw->sample(sample, params, active);
             }),
             "sample"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Hierarchical2D, sample))
        .def("invert",
             vectorize<Float>([](const Hierarchical2D2P *lw, const Vector2fP &invert, FloatP param1,
                                 FloatP param2, bool active) {
                 FloatP params[2] = { param1, param2 };
                 return lw->invert(invert, params, active);
             }),
             "sample"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Hierarchical2D, invert))
        .def("eval",
             vectorize<Float>([](const Hierarchical2D2P *lw, const Vector2fP &pos, FloatP param1,
                                 FloatP param2, bool active) {
                 FloatP params[2] = { param1, param2 };
                 return lw->eval(pos, params, active);
             }),
             "pos"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Hierarchical2D, eval))
        .def("__repr__", &Hierarchical2D2::to_string);

    py::class_<Hierarchical2D3>(m, "Hierarchical2D3")
        .def(py::init([](FloatArray data, std::vector<std::vector<ScalarFloat>> param_values,
                         bool normalize, bool build_hierarchy) {
                 if (data.ndim() != 5)
                     throw std::domain_error("data array has incorrect dimension");
                 if (param_values.size() != 3 ||
                     (uint32_t) param_values[0].size() != (uint32_t) data.shape(0) ||
                     (uint32_t) param_values[1].size() != (uint32_t) data.shape(1) ||
                     (uint32_t) param_values[2].size() != (uint32_t) data.shape(2))
                     throw std::domain_error("param_values array has incorrect dimension");
                 return Hierarchical2D3(
                     ScalarVector2u((uint32_t) data.shape(4), (uint32_t) data.shape(3)), data.data(),
                     { { (uint32_t) param_values[0].size(), (uint32_t) param_values[1].size(),
                         (uint32_t) param_values[2].size() } },
                     { { param_values[0].data(), param_values[1].data(), param_values[2].data() } },
                     normalize, build_hierarchy);
             }),
             "data"_a, "param_values"_a, "normalize"_a = true, "build_hierarchy"_a = true,
             D(warp, Hierarchical2D))
        .def("invert",
             vectorize<Float>([](const Hierarchical2D3P *lw, const Vector2fP &invert, FloatP param1,
                                 FloatP param2, FloatP param3, bool active) {
                 FloatP params[3] = { param1, param2, param3 };
                 return lw->invert(invert, params, active);
             }),
             "sample"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true,
             D(warp, Hierarchical2D, invert))
        .def("sample",
             vectorize<Float>([](const Hierarchical2D3P *lw, const Vector2fP &sample, FloatP param1,
                                 FloatP param2, FloatP param3, bool active) {
                 FloatP params[3] = { param1, param2, param3 };
                 return lw->sample(sample, params, active);
             }),
             "sample"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true,
             D(warp, Hierarchical2D, sample))
        .def("eval",
             vectorize<Float>([](const Hierarchical2D3P *lw, const Vector2fP &pos, FloatP param1,
                                 FloatP param2, FloatP param3, bool active) {
                 FloatP params[3] = { param1, param2, param3 };
                 return lw->eval(pos, params, active);
             }),
             "pos"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true,
             D(warp, Hierarchical2D, eval))
        .def("__repr__", &Hierarchical2D3::to_string);
}
