#include <mitsuba/core/spline.h>
#include <pybind11/numpy.h>
#include <mitsuba/python/python.h>

template<typename Float_>
void bind_spline(py::module &m) {
    MTS_PY_IMPORT_TYPES()
    if constexpr (!is_cuda_array_v<Float_>) {
        m.def("eval_spline", spline::eval_spline<ScalarFloat>,
            "f0"_a, "f1"_a, "d0"_a, "d1"_a, "t"_a, D(spline, eval_spline))
        .def("eval_spline_d", spline::eval_spline_d<ScalarFloat>,
            "f0"_a, "f1"_a, "d0"_a, "d1"_a, "t"_a, D(spline, eval_spline_d))
        .def("eval_spline_i", spline::eval_spline_i<ScalarFloat>,
            "f0"_a, "f1"_a, "d0"_a, "d1"_a, "t"_a, D(spline, eval_spline_i))
        .def("eval_1d",
            vectorize([](ScalarFloat min, ScalarFloat max,
                         const py::array_t<ScalarFloat> &values, Float x) {
                if (values.ndim() != 1)
                    throw std::runtime_error("'values' must be a one-dimensional array!");
                return spline::eval_1d(min, max, values.data(), (uint32_t) values.shape(0), x);
            }),
            "min"_a, "max"_a, "values"_a, "x"_a, D(spline, eval_1d))
        .def("eval_1d",
            vectorize([](const py::array_t<ScalarFloat> &nodes,
                         const py::array_t<ScalarFloat> &values, Float x) {
                if (nodes.ndim() != 1 || values.ndim() != 1)
                    throw std::runtime_error("'nodes' and 'values' must be a one-dimensional array!");
                if (nodes.shape(0) != values.shape(0))
                    throw std::runtime_error("'nodes' and 'values' must have a matching size!");
                return spline::eval_1d(nodes.data(), values.data(), (uint32_t) values.shape(0), x);
            }),
            "nodes"_a, "values"_a, "x"_a, D(spline, eval_1d, 2))
        .def("integrate_1d",
            [](ScalarFloat min, ScalarFloat max, const py::array_t<ScalarFloat> &values) {
                if (values.ndim() != 1)
                    throw std::runtime_error("'values' must be a one-dimensional array!");
                DynamicArray<Packet<ScalarFloat>> result;
                set_slices(result, values.size());
                spline::integrate_1d(min, max, values.data(),
                                    (uint32_t) values.size(), result.data());
                return result;
            },
            "min"_a, "max"_a, "values"_a, D(spline, integrate_1d))
        .def("integrate_1d",
            [](const py::array_t<ScalarFloat> &nodes, const py::array_t<ScalarFloat> &values) {
                if (nodes.ndim() != 1 || values.ndim() != 1)
                    throw std::runtime_error("'nodes' and 'values' must be a one-dimensional array!");
                if (nodes.shape(0) != values.shape(0))
                    throw std::runtime_error("'nodes' and 'values' must have a matching size!");
                DynamicArray<Packet<ScalarFloat>> result;
                set_slices(result, values.size());
                spline::integrate_1d(nodes.data(), values.data(),
                                    (uint32_t) values.size(), result.data());
                return result;
            },
            "nodes"_a, "values"_a, D(spline, integrate_1d, 2))
        .def("invert_1d",
            vectorize([](ScalarFloat min, ScalarFloat max,
                         const py::array_t<ScalarFloat> &values, Float y, ScalarFloat eps) {
                if (values.ndim() != 1)
                    throw std::runtime_error("'values' must be a one-dimensional array!");
                return spline::invert_1d(min, max, values.data(), (uint32_t) values.shape(0), y, eps);
            }),
            "min"_a, "max_"_a, "values"_a, "y"_a, "eps"_a = 1e-6f, D(spline, invert_1d))
        .def("invert_1d",
            vectorize([](const py::array_t<ScalarFloat> &nodes,
                         const py::array_t<ScalarFloat> &values, Float y, ScalarFloat eps) {
                if (nodes.ndim() != 1 || values.ndim() != 1)
                    throw std::runtime_error("'nodes' and 'values' must be a one-dimensional array!");
                if (nodes.shape(0) != values.shape(0))
                    throw std::runtime_error("'nodes' and 'values' must have a matching size!");
                return spline::invert_1d(nodes.data(), values.data(),
                                        (uint32_t) values.shape(0), y, eps);
            }),
            "nodes"_a, "values"_a, "y"_a, "eps"_a = 1e-6f, D(spline, invert_1d, 2))
        .def("sample_1d",
            vectorize([](ScalarFloat min, ScalarFloat max,
                         const py::array_t<ScalarFloat> &values,
                         const py::array_t<ScalarFloat> &cdf, Float sample, ScalarFloat eps) {
                if (values.ndim() != 1)
                    throw std::runtime_error("'values' must be a one-dimensional array!");
                if (cdf.ndim() != 1)
                    throw std::runtime_error("'cdf' must be a one-dimensional array!");
                if (values.size() != cdf.size())
                    throw std::runtime_error("'values' and 'cdf' must have a matching size!");
                return spline::sample_1d(min, max, values.data(), cdf.data(),
                                        (uint32_t) values.shape(0), sample, eps);
            }),
            "min"_a, "max"_a, "values"_a, "cdf"_a, "sample"_a, "eps"_a = 1e-6f, D(spline, sample_1d))
        .def("sample_1d",
            vectorize([](const py::array_t<ScalarFloat> &nodes,
                         const py::array_t<ScalarFloat> &values,
                         const py::array_t<ScalarFloat> &cdf, Float sample, ScalarFloat eps) {
                if (values.ndim() != 1)
                    throw std::runtime_error("'values' must be a one-dimensional array!");
                if (cdf.ndim() != 1)
                    throw std::runtime_error("'cdf' must be a one-dimensional array!");
                if (values.size() != cdf.size())
                    throw std::runtime_error("'values' and 'cdf' must have a matching size!");
                return spline::sample_1d(nodes.data(), values.data(), cdf.data(),
                                        (uint32_t) values.shape(0), sample, eps);
            }),
            "nodes"_a, "values"_a, "cdf"_a, "sample"_a, "eps"_a = 1e-6f, D(spline, sample_1d, 2))
        .def("eval_spline_weights",
            vectorize([](ScalarFloat min, ScalarFloat max, uint32_t size, Float x) {
                py::array_t<Float, 4> weight;
                auto [result, offset] = spline::eval_spline_weights(
                    min, max, size, x, weight.mutable_data());
                return std::make_tuple(result, offset, weight);
            }),
            "min"_a, "max"_a, "size"_a, "x"_a, D(spline, eval_spline_weights))
        .def("eval_spline_weights",
            vectorize([](const py::array_t<ScalarFloat> &nodes, Float x) {
                py::array_t<Float, 4> weight;
                auto [result, offset] = spline::eval_spline_weights(
                    nodes.data(), (uint32_t) nodes.shape(0), x, weight.mutable_data());
                return std::make_tuple(result, offset, weight);
            }),
            "nodes"_a, "x"_a, D(spline, eval_spline_weights, 2))
        .def("eval_2d",
            vectorize([](const py::array_t<ScalarFloat> &nodes1,
                         const py::array_t<ScalarFloat> &nodes2,
                         const py::array_t<ScalarFloat> &values, Float x, Float y) {
                return spline::eval_2d(nodes1.data(), (uint32_t) nodes1.shape(0),
                                    nodes2.data(), (uint32_t) nodes2.shape(0),
                                    values.data(), x, y);
            }),
            "nodes1"_a, "nodes2"_a, "values"_a, "x"_a, "y"_a, D(spline, eval_2d));
    }
}

MTS_PY_EXPORT(spline) {
    MTS_PY_IMPORT_TYPES()
    bind_spline<Float>(m);
}
