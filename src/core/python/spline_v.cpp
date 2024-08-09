#include <mitsuba/core/spline.h>
#include <drjit/dynamic.h>
#include <mitsuba/python/python.h>

#include <nanobind/ndarray.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>

template<typename Float_>
void bind_spline(nb::module_ &m) {
    MI_PY_IMPORT_TYPES()
    if constexpr (!dr::is_cuda_v<Float_>) {
        using FloatX = DynamicBuffer<Float_>;
        m.def("eval_spline", spline::eval_spline<ScalarFloat>, "f0"_a, "f1"_a,
              "d0"_a, "d1"_a, "t"_a, D(spline, eval_spline))
            .def("eval_spline_d", spline::eval_spline_d<ScalarFloat>, "f0"_a,
                 "f1"_a, "d0"_a, "d1"_a, "t"_a, D(spline, eval_spline_d))
            .def("eval_spline_i", spline::eval_spline_i<ScalarFloat>, "f0"_a,
                 "f1"_a, "d0"_a, "d1"_a, "t"_a, D(spline, eval_spline_i))
            .def("eval_1d",
                 [](ScalarFloat min, ScalarFloat max,
                    const FloatX &values, Float x) {
                     return spline::eval_1d(min, max, values.data(),
                                            (uint32_t) values.size(), x);
                 },
                 "min"_a, "max"_a, "values"_a, "x"_a, D(spline, eval_1d))
            .def("eval_1d",
                 [](const FloatX &nodes,
                    const FloatX &values, Float x) {
                     if (nodes.size() != values.size())
                         throw std::runtime_error(
                             "'nodes' and 'values' must have a matching size!");
                     return spline::eval_1d(nodes.data(), values.data(),
                                            (uint32_t) values.size(), x);
                 },
                 "nodes"_a, "values"_a, "x"_a, D(spline, eval_1d, 2))
            .def("integrate_1d",
                 [](ScalarFloat min, ScalarFloat max,
                    const FloatX &values) {
                     using Result  = DynamicBuffer<ScalarFloat>;
                     Result result = dr::empty<Result>(values.size());
                     spline::integrate_1d(min, max, values.data(),
                                          (uint32_t) values.size(),
                                          result.data());
                     return result;
                 },
                 "min"_a, "max"_a, "values"_a, D(spline, integrate_1d))
            .def("integrate_1d",
                 [](const FloatX &nodes,
                    const FloatX &values) {
                     if (nodes.size() != values.size())
                         throw std::runtime_error(
                             "'nodes' and 'values' must have a matching size!");
                     using Result  = DynamicBuffer<ScalarFloat>;
                     Result result = dr::empty<Result>(values.size());
                     spline::integrate_1d(nodes.data(), values.data(),
                                          (uint32_t) values.size(),
                                          result.data());
                     return result;
                 },
                 "nodes"_a, "values"_a, D(spline, integrate_1d, 2))
            .def("invert_1d",
                 [](ScalarFloat min, ScalarFloat max,
                    const FloatX &values, Float y,
                    ScalarFloat eps) {
                     return spline::invert_1d(min, max, values.data(),
                                              (uint32_t) values.size(), y,
                                              eps);
                 },
                 "min"_a, "max_"_a, "values"_a, "y"_a, "eps"_a = 1e-6f,
                 D(spline, invert_1d))
            .def("invert_1d",
                 [](const FloatX &nodes,
                    const FloatX &values,
                    Float y, ScalarFloat eps) {
                     if (nodes.size() != values.size())
                         throw std::runtime_error(
                             "'nodes' and 'values' must have a matching size!");
                     return spline::invert_1d(nodes.data(), values.data(),
                                              (uint32_t) values.size(), y,
                                              eps);
                 },
                 "nodes"_a, "values"_a, "y"_a, "eps"_a = 1e-6f,
                 D(spline, invert_1d, 2))
            .def("sample_1d",
                 [](ScalarFloat min, ScalarFloat max,
                    const FloatX &values,
                    const FloatX &cdf,
                    Float sample,ScalarFloat eps) {
                     if (values.size() != cdf.size())
                         throw std::runtime_error(
                             "'values' and 'cdf' must have a matching size!");
                     return spline::sample_1d(
                         min, max, values.data(), cdf.data(),
                         (uint32_t) values.size(), sample, eps);
                 },
                 "min"_a, "max"_a, "values"_a, "cdf"_a, "sample"_a,
                 "eps"_a = 1e-6f, D(spline, sample_1d))
            .def("sample_1d",
                 [](const FloatX &nodes,
                    const FloatX &values,
                    const FloatX &cdf, 
                    Float sample,ScalarFloat eps) {
                     if (values.size() != cdf.size())
                         throw std::runtime_error(
                             "'values' and 'cdf' must have a matching size!");
                     return spline::sample_1d(
                         nodes.data(), values.data(), cdf.data(),
                         (uint32_t) values.size(), sample, eps);
                 },
                 "nodes"_a, "values"_a, "cdf"_a, "sample"_a, "eps"_a = 1e-6f,
                 D(spline, sample_1d, 2))
            .def("eval_spline_weights",
                 [](ScalarFloat min, ScalarFloat max, uint32_t size, Float x) {
                     std::vector<Float> weight(4);
                     auto [result, offset] = spline::eval_spline_weights(
                         min, max, size, x, weight.data());
                     return std::make_tuple(result, offset, weight);
                 },
                 "min"_a, "max"_a, "size"_a, "x"_a,
                 D(spline, eval_spline_weights))
            .def("eval_spline_weights",
                 [](const FloatX &nodes, Float x) {
                     std::vector<Float> weight(4);
                     auto [result, offset] = spline::eval_spline_weights(
                         nodes.data(), (uint32_t) nodes.size(), x,
                         weight.data());
                     return std::make_tuple(result, offset, weight);
                 },
                 "nodes"_a, "x"_a, D(spline, eval_spline_weights, 2))
            .def("eval_2d",
                 [](const FloatX &nodes1,
                    const FloatX &nodes2,
                    const FloatX &values, Float x, Float y) {
                     return spline::eval_2d(
                         nodes1.data(), (uint32_t) nodes1.size(),
                         nodes2.data(), (uint32_t) nodes2.size(),
                         values.data(), x, y);
                 },
                 "nodes1"_a, "nodes2"_a, "values"_a, "x"_a, "y"_a,
                 D(spline, eval_2d));
    }
}

MI_PY_EXPORT(spline) {
    MI_PY_IMPORT_TYPES()
    bind_spline<Float>(m);
}
