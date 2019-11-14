#include <mitsuba/python/python.h>
#include <mitsuba/core/warp.h>
#include <enoki/stl.h>
#include <pybind11/iostream.h>

MTS_PY_EXPORT(warp) {
    MTS_IMPORT_CORE_TYPES()

    // Create dedicated submodule
    auto warp = m.def_submodule("warp", "Common warping techniques that map from the unit square "
                                        "to other domains, such as spheres, hemispheres, etc.");

    // Import ditr.py in this submodule
    warp.attr("distr") = py::module::import("mitsuba.core.warp.distr");

    warp.def(
        "square_to_uniform_disk",
        vectorize<Float>(warp::square_to_uniform_disk<Float>),
        "sample"_a, D(warp, square_to_uniform_disk));

    warp.def(
        "uniform_disk_to_square",
        vectorize<Float>(warp::uniform_disk_to_square<Float>),
        "p"_a, D(warp, uniform_disk_to_square));

    warp.def(
        "square_to_uniform_disk_pdf",
        vectorize<Float>(warp::square_to_uniform_disk_pdf<true, Float>),
        "p"_a, D(warp, square_to_uniform_disk_pdf));

    warp.def(
        "uniform_disk_to_square_concentric",
        vectorize<Float>(warp::uniform_disk_to_square_concentric<Float>),
        "p"_a, D(warp, uniform_disk_to_square_concentric));
    warp.def(
        "square_to_uniform_disk_concentric",
        vectorize<Float>(warp::square_to_uniform_disk_concentric<Float>),
        "sample"_a, D(warp, square_to_uniform_disk_concentric));

    warp.def(
        "square_to_uniform_square_concentric",
        vectorize<Float>(warp::square_to_uniform_square_concentric<Float>),
        "sample"_a, D(warp, square_to_uniform_square_concentric));

    warp.def(
        "square_to_uniform_disk_concentric_pdf",
        vectorize<Float>(warp::square_to_uniform_disk_concentric_pdf<true, Float>),
        "p"_a, D(warp, square_to_uniform_disk_concentric_pdf));

    warp.def(
        "square_to_uniform_triangle",
        vectorize<Float>(warp::square_to_uniform_triangle<Float>),
        "sample"_a, D(warp, square_to_uniform_triangle));

    warp.def(
        "uniform_triangle_to_square",
        vectorize<Float>(warp::uniform_triangle_to_square<Float>),
        "p"_a, D(warp, uniform_triangle_to_square));

    warp.def(
        "interval_to_tent",
        warp::interval_to_tent<Float>,
        "sample"_a, D(warp, interval_to_tent));

    warp.def(
        "tent_to_interval",
        warp::tent_to_interval<Float>,
        "value"_a, D(warp, tent_to_interval));

    warp.def(
        "interval_to_nonuniform_tent",
        warp::interval_to_nonuniform_tent<Float>,
        "a"_a, "b"_a, "c"_a, "d"_a, D(warp, interval_to_nonuniform_tent));

    warp.def(
        "square_to_tent",
        vectorize<Float>(warp::square_to_tent<Float>),
        "sample"_a, D(warp, square_to_tent));

    warp.def(
        "tent_to_square",
        vectorize<Float>(warp::tent_to_square<Float>),
        "value"_a, D(warp, tent_to_square));

    warp.def(
        "square_to_tent_pdf",
        vectorize<Float>(warp::square_to_tent_pdf<Float>),
        "v"_a, D(warp, square_to_tent_pdf));

    warp.def(
        "square_to_uniform_triangle_pdf",
        vectorize<Float>(warp::square_to_uniform_triangle_pdf<true, Float>),
        "p"_a, D(warp, square_to_uniform_triangle_pdf));

    warp.def(
        "square_to_uniform_sphere",
        vectorize<Float>(warp::square_to_uniform_sphere<Float>),
        "sample"_a, D(warp, square_to_uniform_sphere));

    warp.def(
        "uniform_sphere_to_square",
        vectorize<Float>(warp::uniform_sphere_to_square<Float>),
        "sample"_a, D(warp, uniform_sphere_to_square));

    warp.def(
        "square_to_uniform_sphere_pdf",
        vectorize<Float>(warp::square_to_uniform_sphere_pdf<true, Float>),
        "v"_a, D(warp, square_to_uniform_sphere_pdf));

    warp.def(
        "square_to_uniform_hemisphere",
        vectorize<Float>(warp::square_to_uniform_hemisphere<Float>),
        "sample"_a, D(warp, square_to_uniform_hemisphere));

    warp.def(
        "uniform_hemisphere_to_square",
        vectorize<Float>(warp::uniform_hemisphere_to_square<Float>),
        "v"_a, D(warp, uniform_hemisphere_to_square));

    warp.def(
        "square_to_uniform_hemisphere_pdf",
        vectorize<Float>(warp::square_to_uniform_hemisphere_pdf<true, Float>),
        "v"_a, D(warp, square_to_uniform_hemisphere_pdf));

    warp.def(
        "square_to_cosine_hemisphere",
        vectorize<Float>(warp::square_to_cosine_hemisphere<Float>),
        "sample"_a, D(warp, square_to_cosine_hemisphere));

    warp.def(
        "cosine_hemisphere_to_square",
        vectorize<Float>(warp::cosine_hemisphere_to_square<Float>),
        "v"_a, D(warp, cosine_hemisphere_to_square));

    warp.def(
        "square_to_cosine_hemisphere_pdf",
        vectorize<Float>(warp::square_to_cosine_hemisphere_pdf<true, Float>),
        "v"_a, D(warp, square_to_cosine_hemisphere_pdf));

    warp.def(
        "square_to_uniform_cone",
        vectorize<Float>(warp::square_to_uniform_cone<Float>),
        "v"_a, "cos_cutoff"_a, D(warp, square_to_uniform_cone));

    warp.def(
        "uniform_cone_to_square",
        vectorize<Float>(warp::uniform_cone_to_square<Float>),
        "v"_a, "cos_cutoff"_a, D(warp, uniform_cone_to_square));

    warp.def(
        "square_to_uniform_cone_pdf",
        vectorize<Float>(warp::square_to_uniform_cone_pdf<true, Float>),
        "v"_a, "cos_cutoff"_a, D(warp, square_to_uniform_cone_pdf));

    warp.def(
        "square_to_beckmann",
        vectorize<Float>(warp::square_to_beckmann<Float>),
        "sample"_a, "alpha"_a, D(warp, square_to_beckmann));

    warp.def(
        "beckmann_to_square",
        vectorize<Float>(warp::beckmann_to_square<Float>),
        "v"_a, "alpha"_a, D(warp, beckmann_to_square));

    warp.def(
        "square_to_beckmann_pdf",
        vectorize<Float>(warp::square_to_beckmann_pdf<Float>),
        "v"_a, "alpha"_a, D(warp, square_to_beckmann_pdf));

    warp.def(
        "square_to_von_mises_fisher",
        vectorize<Float>(warp::square_to_von_mises_fisher<Float>),
        "sample"_a, "kappa"_a, D(warp, square_to_von_mises_fisher));

    warp.def(
        "von_mises_fisher_to_square",
        vectorize<Float>(warp::von_mises_fisher_to_square<Float>),
        "v"_a, "kappa"_a, D(warp, von_mises_fisher_to_square));

    warp.def(
        "square_to_von_mises_fisher_pdf",
        vectorize<Float>(warp::square_to_von_mises_fisher_pdf<Float>),
        "v"_a, "kappa"_a, D(warp, square_to_von_mises_fisher_pdf));

    warp.def(
        "square_to_rough_fiber",
        vectorize<Float>(warp::square_to_rough_fiber<Float>),
        "sample"_a, "wi"_a, "tangent"_a, "kappa"_a,
        D(warp, square_to_rough_fiber));

    warp.def(
        "square_to_rough_fiber_pdf",
        vectorize<Float>(warp::square_to_rough_fiber_pdf<Float>),
        "v"_a, "wi"_a, "tangent"_a, "kappa"_a,
        D(warp, square_to_rough_fiber_pdf));

    warp.def(
        "square_to_std_normal",
        vectorize<Float>(warp::square_to_std_normal<Float>),
        "v"_a, D(warp, square_to_std_normal));

    warp.def(
        "square_to_std_normal_pdf",
        vectorize<Float>(warp::square_to_std_normal_pdf<Float>),
        "v"_a, D(warp, square_to_std_normal_pdf));

    // --------------------------------------------------------------------

    using FloatArray = py::array_t<ScalarFloat, py::array::c_style | py::array::forcecast>;

    using Marginal2D0 = warp::Marginal2D<Float, 0>;
    using Marginal2D1 = warp::Marginal2D<Float, 1>;
    using Marginal2D2 = warp::Marginal2D<Float, 2>;
    using Marginal2D3 = warp::Marginal2D<Float, 3>;

    MTS_PY_CHECK_ALIAS(Marginal2D0, m) {
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
                vectorize<Float>([](const Marginal2D0 *lw, const Vector2f &sample, Mask active) {
                    return lw->sample(sample, (const Float *) nullptr, active);
                }),
                "sample"_a, "active"_a = true, D(warp, Marginal2D, sample))
            .def("invert",
                vectorize<Float>([](const Marginal2D0 *lw, const Vector2f &invert, Mask active) {
                    return lw->invert(invert, (const Float *) nullptr, active);
                }),
                "sample"_a, "active"_a = true, D(warp, Marginal2D, invert))
            .def("eval",
                vectorize<Float>([](const Marginal2D0 *lw, const Vector2f &pos, Mask active) {
                    return lw->eval(pos, (const Float *) nullptr, active);
                }),
                "pos"_a, "active"_a = true, D(warp, Marginal2D, eval))
            .def("__repr__", &Marginal2D0::to_string);
    }

    MTS_PY_CHECK_ALIAS(Marginal2D1, m) {
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
                    [](const Marginal2D1 *lw, const Vector2f &sample, Float param1, Mask active) {
                        Float params[1] = { param1 };
                        return lw->sample(sample, params, active);
                    }),
                "sample"_a, "param1"_a, "active"_a = true, D(warp, Marginal2D, sample))
            .def("invert",
                vectorize<Float>(
                    [](const Marginal2D1 *lw, const Vector2f &invert, Float param1, Mask active) {
                        Float params[1] = { param1 };
                        return lw->invert(invert, params, active);
                    }),
                "sample"_a, "param1"_a, "active"_a = true, D(warp, Marginal2D, invert))
            .def("eval",
                vectorize<Float>(
                    [](const Marginal2D1 *lw, const Vector2f &pos, Float param1, Mask active) {
                        Float params[1] = { param1 };
                        return lw->eval(pos, params, active);
                    }),
                "pos"_a, "param1"_a, "active"_a = true, D(warp, Marginal2D, eval))
            .def("__repr__", &Marginal2D1::to_string)
            ;
    }

    MTS_PY_CHECK_ALIAS(Marginal2D2, m) {
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
                vectorize<Float>([](const Marginal2D2 *lw, const Vector2f &sample, Float param1,
                                    Float param2, Mask active) {
                    Float params[2] = { param1, param2 };
                    return lw->sample(sample, params, active);
                }),
                "sample"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Marginal2D, sample))

            .def("invert",
                vectorize<Float>([](const Marginal2D2 *lw, const Vector2f &invert, Float param1,
                                    Float param2, Mask active) {
                    Float params[2] = { param1, param2 };
                    return lw->invert(invert, params, active);
                }),
                "sample"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Marginal2D, invert))
            .def("eval",
                vectorize<Float>([](const Marginal2D2 *lw, const Vector2f &pos, Float param1,
                                    Float param2, Mask active) {
                    Float params[2] = { param1, param2 };
                    return lw->eval(pos, params, active);
                }),
                "pos"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Marginal2D, eval))
            .def("__repr__", &Marginal2D2::to_string)
            ;
    }

    MTS_PY_CHECK_ALIAS(Marginal2D3, m) {
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
                vectorize<Float>([](const Marginal2D3 *lw, const Vector2f &invert, Float param1,
                                    Float param2, Float param3, Mask active) {
                    Float params[3] = { param1, param2, param3 };
                    return lw->invert(invert, params, active);
                }),
                "sample"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true,
                D(warp, Marginal2D, invert))
            .def("sample",
                vectorize<Float>([](const Marginal2D3 *lw, const Vector2f &sample, Float param1,
                                    Float param2, Float param3, Mask active) {
                    Float params[3] = { param1, param2, param3 };
                    return lw->sample(sample, params, active);
                }),
                "sample"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true,
                D(warp, Marginal2D, sample))
            .def("eval",
                vectorize<Float>([](const Marginal2D3 *lw, const Vector2f &pos, Float param1,
                                    Float param2, Float param3, Mask active) {
                    Float params[3] = { param1, param2, param3 };
                    return lw->eval(pos, params, active);
                }),
                "pos"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true,
                D(warp, Marginal2D, eval))
            .def("__repr__", &Marginal2D3::to_string)
            ;
    }

    // --------------------------------------------------------------------

    using Hierarchical2D0 = warp::Hierarchical2D<Float, 0>;
    using Hierarchical2D1 = warp::Hierarchical2D<Float, 1>;
    using Hierarchical2D2 = warp::Hierarchical2D<Float, 2>;
    using Hierarchical2D3 = warp::Hierarchical2D<Float, 3>;

    MTS_PY_CHECK_ALIAS(Hierarchical2D0, m) {
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
                vectorize<Float>([](const Hierarchical2D0 *lw, const Vector2f &sample, Mask active) {
                    return lw->sample(sample, (const Float *) nullptr, active);
                }),
                "sample"_a, "active"_a = true, D(warp, Hierarchical2D, sample))
            .def("invert",
                vectorize<Float>([](const Hierarchical2D0 *lw, const Vector2f &invert, Mask active) {
                    return lw->invert(invert, (const Float *) nullptr, active);
                }),
                "sample"_a, "active"_a = true, D(warp, Hierarchical2D, invert))
            .def("eval",
                vectorize<Float>([](const Hierarchical2D0 *lw, const Vector2f &pos, Mask active) {
                    return lw->eval(pos, (const Float *) nullptr, active);
                }),
                "pos"_a, "active"_a = true, D(warp, Hierarchical2D, eval))
            .def("__repr__", &Hierarchical2D0::to_string)
            ;
    }

    MTS_PY_CHECK_ALIAS(Hierarchical2D1, m) {
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
                    [](const Hierarchical2D1 *lw, const Vector2f &sample, Float param1, Mask active) {
                        Float params[1] = { param1 };
                        return lw->sample(sample, params, active);
                    }),
                "sample"_a, "param1"_a, "active"_a = true, D(warp, Hierarchical2D, sample))
            .def("invert",
                vectorize<Float>(
                    [](const Hierarchical2D1 *lw, const Vector2f &invert, Float param1, Mask active) {
                        Float params[1] = { param1 };
                        return lw->invert(invert, params, active);
                    }),
                "sample"_a, "param1"_a, "active"_a = true, D(warp, Hierarchical2D, invert))
            .def("eval",
                vectorize<Float>(
                    [](const Hierarchical2D1 *lw, const Vector2f &pos, Float param1, Mask active) {
                        Float params[1] = { param1 };
                        return lw->eval(pos, params, active);
                    }),
                "pos"_a, "param1"_a, "active"_a = true, D(warp, Hierarchical2D, eval))
            .def("__repr__", &Hierarchical2D1::to_string)
            ;
    }

    MTS_PY_CHECK_ALIAS(Hierarchical2D2, m) {
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
                vectorize<Float>([](const Hierarchical2D2 *lw, const Vector2f &sample, Float param1,
                                    Float param2, Mask active) {
                    Float params[2] = { param1, param2 };
                    return lw->sample(sample, params, active);
                }),
                "sample"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Hierarchical2D, sample))
            .def("invert",
                vectorize<Float>([](const Hierarchical2D2 *lw, const Vector2f &invert, Float param1,
                                    Float param2, Mask active) {
                    Float params[2] = { param1, param2 };
                    return lw->invert(invert, params, active);
                }),
                "sample"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Hierarchical2D, invert))
            .def("eval",
                vectorize<Float>([](const Hierarchical2D2 *lw, const Vector2f &pos, Float param1,
                                    Float param2, Mask active) {
                    Float params[2] = { param1, param2 };
                    return lw->eval(pos, params, active);
                }),
                "pos"_a, "param1"_a, "param2"_a, "active"_a = true, D(warp, Hierarchical2D, eval))
            .def("__repr__", &Hierarchical2D2::to_string)
            ;
    }

    MTS_PY_CHECK_ALIAS(Hierarchical2D3, m) {
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
                vectorize<Float>([](const Hierarchical2D3 *lw, const Vector2f &invert, Float param1,
                                    Float param2, Float param3, Mask active) {
                    Float params[3] = { param1, param2, param3 };
                    return lw->invert(invert, params, active);
                }),
                "sample"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true,
                D(warp, Hierarchical2D, invert))
            .def("sample",
                vectorize<Float>([](const Hierarchical2D3 *lw, const Vector2f &sample, Float param1,
                                    Float param2, Float param3, Mask active) {
                    Float params[3] = { param1, param2, param3 };
                    return lw->sample(sample, params, active);
                }),
                "sample"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true,
                D(warp, Hierarchical2D, sample))
            .def("eval",
                vectorize<Float>([](const Hierarchical2D3 *lw, const Vector2f &pos, Float param1,
                                    Float param2, Float param3, Mask active) {
                    Float params[3] = { param1, param2, param3 };
                    return lw->eval(pos, params, active);
                }),
                "pos"_a, "param1"_a, "param2"_a, "param3"_a, "active"_a = true,
                D(warp, Hierarchical2D, eval))
            .def("__repr__", &Hierarchical2D3::to_string)
            ;
    }
}
