#include <mitsuba/python/python.h>
#include <mitsuba/core/distr_2d.h>
#include <pybind11/numpy.h>
#include <enoki/stl.h>

template <typename Warp> void bind_warp(py::module &m, const char *name) {
    using Float                = typename Warp::Float;
    using FloatStorage         = typename Warp::FloatStorage;
    using ScalarFloat          = scalar_t<Float>;
    using NumPyArray           = py::array_t<ScalarFloat, py::array::c_style | py::array::forcecast>;
    using Vector2f             = enoki::Array<Float, 2>;
    using ScalarVector2u       = enoki::Array<uint32_t, 2>;
    using Mask                 = mask_t<Float>;

    py::object zero = py::cast(enoki::zero<Array<ScalarFloat, Warp::Dimension>>());

    py::class_<Warp>(m, name, py::module_local())
        .def(py::init([](const NumPyArray &data,
                         const std::array<FloatStorage, Warp::Dimension> &param_values_in,
                         bool normalize, bool build_hierarchy) {
                 if (data.ndim() != Warp::Dimension + 2)
                     throw std::domain_error("'data' array has incorrect dimension");

                 std::array<uint32_t, Warp::Dimension> param_res;
                 std::array<const ScalarFloat *, Warp::Dimension> param_values;

                 for (size_t i = 0; i < Warp::Dimension; ++i) {
                     if (param_values_in[i].size() != (size_t) data.shape(i))
                         throw std::domain_error("'param_values' array has incorrect dimension");
                     param_values[i] = param_values_in[i].data();
                     param_res[i] = param_values_in[i].size();
                 }

                 return Warp(data.data(),
                             ScalarVector2u((uint32_t) data.shape(data.ndim() - 1),
                                            (uint32_t) data.shape(data.ndim() - 2)),
                             param_res, param_values, normalize, build_hierarchy);
             }),
             "data"_a, "param_values"_a, "normalize"_a = true, "build_hierarchy"_a = true,
             D(warp, Hierarchical2D))
        .def("sample",
            vectorize([](const Warp *w, const Vector2f &sample,
                         const Array<Float, Warp::Dimension> &param,
                         Mask active) {
                return w->sample(sample, param.data(), active);
            }),
            "sample"_a, "param"_a = zero, "active"_a = true, D(warp, Hierarchical2D, sample))
        .def("invert",
            vectorize([](const Warp *w, const Vector2f &sample,
                         const Array<Float, Warp::Dimension> &param,
                         Mask active) {
                return w->invert(sample, param.data(), active);
            }),
            "sample"_a, "param"_a = zero, "active"_a = true, D(warp, Hierarchical2D, invert))
        .def("eval",
            vectorize([](const Warp *w, const Vector2f &pos,
                         const Array<Float, Warp::Dimension> &param,
                         Mask active) {
                return w->eval(pos, param.data(), active);
            }),
            "pos"_a, "param"_a = zero, "active"_a = true, D(warp, Hierarchical2D, eval))
        .def("__repr__", &Warp::to_string);
}

MTS_PY_EXPORT(Hierarchical2D) {
    MTS_PY_IMPORT_TYPES()

    bind_warp<Hierarchical2D<Float, 0>>(m, "Hierarchical2D0");
    bind_warp<Hierarchical2D<Float, 1>>(m, "Hierarchical2D1");
    bind_warp<Hierarchical2D<Float, 2>>(m, "Hierarchical2D2");
    bind_warp<Hierarchical2D<Float, 3>>(m, "Hierarchical2D3");
}

MTS_PY_EXPORT(Marginal2D) {
    MTS_PY_IMPORT_TYPES()

    bind_warp<Marginal2D<Float, 0>>(m, "Marginal2D0");
    bind_warp<Marginal2D<Float, 1>>(m, "Marginal2D1");
    bind_warp<Marginal2D<Float, 2>>(m, "Marginal2D2");
    bind_warp<Marginal2D<Float, 3>>(m, "Marginal2D3");
}
