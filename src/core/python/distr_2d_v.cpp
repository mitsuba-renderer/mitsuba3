#include <mitsuba/core/distr_2d.h>
#include <mitsuba/python/python.h>

#include <drjit-core/python.h>

#include <nanobind/ndarray.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/tuple.h>

template <typename Warp> auto bind_warp(nb::module_ &m,
        const char *name,
        const char *doc,
        const char *doc_constructor,
        const char *doc_sample,
        const char *doc_invert,
        const char *doc_eval) {
    using Float                = typename Warp::Float;
    using ScalarFloat          = dr::scalar_t<Float>;
    using Vector2f             = dr::Array<Float, 2>;
    using ScalarVector2u       = dr::Array<uint32_t, 2>;
    using Mask                 = dr::mask_t<Float>;
    using PyArray              = nb::ndarray<ScalarFloat, nb::c_contig,
                                 nb::ndim<Warp::Dimension + 2>>;

    nb::object zero = nb::cast(dr::zeros<dr::Array<ScalarFloat, Warp::Dimension>>());

    auto constructor = [](Warp* t, const PyArray &data,
                    const std::array<std::vector<ScalarFloat>, Warp::Dimension>
                        &param_values_in,
                    bool normalize, bool build_hierarchy) {

            std::array<uint32_t, Warp::Dimension> param_res;
            std::array<const ScalarFloat *, Warp::Dimension> param_values;

            for (size_t i = 0; i < Warp::Dimension; ++i) {
                if (param_values_in[i].size() != (size_t) data.shape(i))
                    throw std::domain_error(
                        "'param_values' array has incorrect dimension");
                param_values[i] = param_values_in[i].data();
                param_res[i]    = (uint32_t) param_values_in[i].size();
            }

            new (t) Warp(data.data(),
                ScalarVector2u((uint32_t) data.shape(data.ndim() - 1),
                           (uint32_t) data.shape(data.ndim() - 2)),
                param_res, param_values, normalize, build_hierarchy);
        };

    MI_PY_CHECK_ALIAS(Warp, name) {
        auto warp = nb::class_<Warp>(m, name, doc);

        if constexpr (Warp::Dimension == 0)
            warp.def("__init__", constructor, "data"_a,
                     "param_values"_a = nb::list(), "normalize"_a = true,
                     "enable_sampling"_a = true, doc_constructor);
        else
            warp.def("__init__", constructor, "data"_a, "param_values"_a,
                     "normalize"_a = true, "build_hierarchy"_a = true,
                     doc_constructor);

        warp.def("sample",
                 [](const Warp *w, const Vector2f &sample,
                              const dr::Array<Float, Warp::Dimension> &param,
                              Mask active) {
                     return w->sample(sample, param.data(), active);
                 },
                 "sample"_a, "param"_a = zero, "active"_a = true, doc_sample)
            .def("invert",
                 [](const Warp *w, const Vector2f &sample,
                              const dr::Array<Float, Warp::Dimension> &param,
                              Mask active) {
                     return w->invert(sample, param.data(), active);
                 },
                 "sample"_a, "param"_a = zero, "active"_a = true, doc_invert)
            .def("eval",
                 [](const Warp *w, const Vector2f &pos,
                              const dr::Array<Float, Warp::Dimension> &param,
                              Mask active) {
                     return w->eval(pos, param.data(), active);
                 },
                 "pos"_a, "param"_a = zero, "active"_a = true, doc_eval)
            .def("__repr__", &Warp::to_string);
    }
}

template <typename Warp> void bind_warp_hierarchical(nb::module_ &m, const char *name) {
    bind_warp<Warp>(m, name,
        D(Hierarchical2D),
        D(Hierarchical2D, Hierarchical2D, 2),
        D(Hierarchical2D, sample),
        D(Hierarchical2D, invert),
        D(Hierarchical2D, eval)
    );
}

template <typename Warp> void bind_warp_marginal(nb::module_ &m, const char *name) {
    bind_warp<Warp>(m, name,
        D(Marginal2D),
        D(Marginal2D, Marginal2D, 2),
        D(Marginal2D, sample),
        D(Marginal2D, invert),
        D(Marginal2D, eval)
    );
}

MI_PY_EXPORT(Hierarchical2D) {
    MI_PY_IMPORT_TYPES()

    bind_warp_hierarchical<Hierarchical2D<Float, 0>>(m, "Hierarchical2D0");
    bind_warp_hierarchical<Hierarchical2D<Float, 1>>(m, "Hierarchical2D1");
    bind_warp_hierarchical<Hierarchical2D<Float, 2>>(m, "Hierarchical2D2");
    bind_warp_hierarchical<Hierarchical2D<Float, 3>>(m, "Hierarchical2D3");
}

MI_PY_EXPORT(Marginal2D) {
    MI_PY_IMPORT_TYPES()

    bind_warp_marginal<Marginal2D<Float, 0, false>>(m, "MarginalDiscrete2D0");
    bind_warp_marginal<Marginal2D<Float, 1, false>>(m, "MarginalDiscrete2D1");
    bind_warp_marginal<Marginal2D<Float, 2, false>>(m, "MarginalDiscrete2D2");
    bind_warp_marginal<Marginal2D<Float, 3, false>>(m, "MarginalDiscrete2D3");

    bind_warp_marginal<Marginal2D<Float, 0, true>>(m, "MarginalContinuous2D0");
    bind_warp_marginal<Marginal2D<Float, 1, true>>(m, "MarginalContinuous2D1");
    bind_warp_marginal<Marginal2D<Float, 2, true>>(m, "MarginalContinuous2D2");
    bind_warp_marginal<Marginal2D<Float, 3, true>>(m, "MarginalContinuous2D3");
}

MI_PY_EXPORT(DiscreteDistribution2D) {
    MI_PY_IMPORT_TYPES()
    using Warp = DiscreteDistribution2D<Float>;

    MI_PY_CHECK_ALIAS(Warp, "DiscreteDistribution2D") {
        nb::class_<Warp>(m, "DiscreteDistribution2D")
            .def("__init__",[](Warp* t, const nb::ndarray<ScalarFloat, nb::ndim<2>> &data) {
                 new (t) Warp(
                     data.data(),
                     ScalarVector2u((uint32_t) data.shape(data.ndim() - 1),
                                    (uint32_t) data.shape(data.ndim() - 2)));
                 }, "data"_a)
            .def("eval", &Warp::eval, "pos"_a, "active"_a = true)
            .def("pdf", &Warp::pdf, "pos"_a, "active"_a = true)
            .def("sample", &Warp::sample, "sample"_a, "active"_a = true)
            .def("__repr__", &Warp::to_string);
    }
}
