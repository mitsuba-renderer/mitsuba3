#include <mitsuba/core/spline.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(spline) {
    MTS_PY_IMPORT_MODULE(spline, "mitsuba.core.spline");

    spline.def("eval_spline",   spline::eval_spline<Float>,   "f0"_a, "f1"_a, "d0"_a, "d1"_a, "t"_a,
               D(spline, eval_spline));

    spline.def("eval_spline_d", spline::eval_spline_d<Float>, "f0"_a, "f1"_a, "d0"_a, "d1"_a, "t"_a,
               D(spline, eval_spline_d));

    spline.def("eval_spline_i", spline::eval_spline_i<Float>, "f0"_a, "f1"_a, "d0"_a, "d1"_a, "t"_a,
               D(spline, eval_spline_i));

    // ------------------------------------------------------------------------------------------

    spline.def("eval_1d", [](Float min, Float max, const py::array_t<Float> &values, Float x) {
            if (values.ndim() != 1)
                throw std::runtime_error("'values' must be a one-dimensional array!");
            return spline::eval_1d(min, max, values.data(), (uint32_t) values.shape(0), x);
        },
        "min"_a, "max"_a, "values"_a, "x"_a, D(spline, eval_1d)
    );

    spline.def("eval_1d",
        vectorize_wrapper(
            [](Float min, Float max, const py::array_t<Float> &values, FloatP x) {
                if (values.ndim() != 1)
                    throw std::runtime_error("'values' must be a one-dimensional array!");
                return spline::eval_1d(min, max, values.data(), (uint32_t) values.shape(0), x);
            }
        ), "min"_a, "max"_a, "values"_a, "x"_a
    );

    // ------------------------------------------------------------------------------------------

    spline.def("eval_1d", [](const py::array_t<Float> &nodes, const py::array_t<Float> &values, Float x) {
            if (nodes.ndim() != 1 || values.ndim() != 1)
                throw std::runtime_error("'nodes' and 'values' must be a one-dimensional array!");
            if (nodes.shape(0) != values.shape(0))
                throw std::runtime_error("'nodes' and 'values' must have a matching size!");
            return spline::eval_1d(nodes.data(), values.data(), (uint32_t) values.shape(0), x);
        },
        "nodes"_a, "values"_a, "x"_a, D(spline, eval_1d, 2)
    );

    spline.def("eval_1d",
        vectorize_wrapper(
            [](const py::array_t<Float> &nodes, const py::array_t<Float> &values, FloatP x) {
                if (nodes.ndim() != 1 || values.ndim() != 1)
                    throw std::runtime_error("'nodes' and 'values' must be a one-dimensional array!");
                if (nodes.shape(0) != values.shape(0))
                    throw std::runtime_error("'nodes' and 'values' must have a matching size!");
                return spline::eval_1d(nodes.data(), values.data(), (uint32_t) values.shape(0), x);
            }
        ), "nodes"_a, "values"_a, "x"_a
    );

    // ------------------------------------------------------------------------------------------

    spline.def("integrate_1d", [](Float min, Float max, const py::array_t<Float> &values) {
            if (values.ndim() != 1)
                throw std::runtime_error("'values' must be a one-dimensional array!");
            py::array_t<Float> result(values.size());
            spline::integrate_1d(min, max, values.data(), (uint32_t) values.size(), result.mutable_data());
            return result;
        },
        "min"_a, "max"_a, "values"_a, D(spline, integrate_1d)
    );

    // ------------------------------------------------------------------------------------------

    spline.def("integrate_1d",
        [](const py::array_t<Float> &nodes, const py::array_t<Float> &values) {
            if (nodes.ndim() != 1 || values.ndim() != 1)
                throw std::runtime_error("'nodes' and 'values' must be a one-dimensional array!");
            if (nodes.shape(0) != values.shape(0))
                throw std::runtime_error("'nodes' and 'values' must have a matching size!");
            py::array_t<Float> result(values.size());
            spline::integrate_1d(nodes.data(), values.data(), (uint32_t) values.size(), result.mutable_data());
            return result;
        },
        "nodes"_a, "values"_a, D(spline, integrate_1d, 2)
    );

    // ------------------------------------------------------------------------------------------

    spline.def("invert_1d", [](Float min, Float max, const py::array_t<Float> &values, Float y, Float eps) {
            if (values.ndim() != 1)
                throw std::runtime_error("'values' must be a one-dimensional array!");
            return spline::invert_1d(min, max, values.data(), (uint32_t) values.shape(0), y, eps);
        },
        "min"_a, "max_"_a, "values"_a, "y"_a, "eps"_a = 1e-6f, D(spline, invert_1d)
    );

    spline.def("invert_1d",
        vectorize_wrapper(
            [](Float min, Float max, const py::array_t<Float> &values, FloatP y, Float eps) {
                if (values.ndim() != 1)
                    throw std::runtime_error("'values' must be a one-dimensional array!");
                return spline::invert_1d(min, max, values.data(), (uint32_t) values.shape(0), y, eps);
            }
        ), "min"_a, "max"_a, "values"_a, "y"_a, "eps"_a = 1e-6f
    );

    // ------------------------------------------------------------------------------------------

    spline.def("invert_1d", [](const py::array_t<Float> &nodes, const py::array_t<Float> &values, Float y, Float eps) {
            if (nodes.ndim() != 1 || values.ndim() != 1)
                throw std::runtime_error("'nodes' and 'values' must be a one-dimensional array!");
            if (nodes.shape(0) != values.shape(0))
                throw std::runtime_error("'nodes' and 'values' must have a matching size!");
            return spline::invert_1d(nodes.data(), values.data(), (uint32_t) values.shape(0), y, eps);
        },
        "nodes"_a, "values"_a, "y"_a, "eps"_a = 1e-6f, D(spline, invert_1d, 2)
    );

    spline.def("invert_1d",
        vectorize_wrapper(
            [](const py::array_t<Float> &nodes, const py::array_t<Float> &values, FloatP y, Float eps) {
                if (nodes.ndim() != 1 || values.ndim() != 1)
                    throw std::runtime_error("'nodes' and 'values' must be a one-dimensional array!");
                if (nodes.shape(0) != values.shape(0))
                    throw std::runtime_error("'nodes' and 'values' must have a matching size!");
                return spline::invert_1d(nodes.data(), values.data(), (uint32_t) values.shape(0), y, eps);
            }
        ), "nodes"_a, "values"_a, "y"_a, "eps"_a = 1e-6f
    );

    // ------------------------------------------------------------------------------------------

    spline.def("sample_1d", [](Float min, Float max, const py::array_t<Float> &values,
                               const py::array_t<Float> &cdf, Float sample, Float eps) {
            if (values.ndim() != 1)
                throw std::runtime_error("'values' must be a one-dimensional array!");
            if (cdf.ndim() != 1)
                throw std::runtime_error("'cdf' must be a one-dimensional array!");
            if (values.size() != cdf.size())
                throw std::runtime_error("'values' and 'cdf' must have a matching size!");
            return spline::sample_1d(min, max, values.data(), cdf.data(), (uint32_t) values.shape(0), sample, eps);
        },
        "min"_a, "max"_a, "values"_a, "cdf"_a, "sample"_a, "eps"_a = 1e-6f, D(spline, sample_1d)
    );

    spline.def("sample_1d",
        vectorize_wrapper(
            [](Float min, Float max, const py::array_t<Float> &values,
               const py::array_t<Float> &cdf, FloatP sample, Float eps) {
                if (values.ndim() != 1)
                    throw std::runtime_error("'values' must be a one-dimensional array!");
                if (cdf.ndim() != 1)
                    throw std::runtime_error("'cdf' must be a one-dimensional array!");
                if (values.size() != cdf.size())
                    throw std::runtime_error("'values' and 'cdf' must have a matching size!");
                return spline::sample_1d(min, max, values.data(), cdf.data(), (uint32_t) values.shape(0), sample, eps);
            }
        ), "min"_a, "max"_a, "values"_a, "cdf"_a, "sample"_a, "eps"_a = 1e-6f
    );

    // ------------------------------------------------------------------------------------------

    spline.def("sample_1d", [](const py::array_t<Float> &nodes, const py::array_t<Float> &values,
            const py::array_t<Float> &cdf, Float sample, Float eps) {
            if (values.ndim() != 1)
                throw std::runtime_error("'values' must be a one-dimensional array!");
            if (cdf.ndim() != 1)
                throw std::runtime_error("'cdf' must be a one-dimensional array!");
            if (values.size() != cdf.size())
                throw std::runtime_error("'values' and 'cdf' must have a matching size!");
            return spline::sample_1d(nodes.data(), values.data(), cdf.data(),
                                     (uint32_t) values.shape(0), sample, eps);
        },
        "nodes"_a, "values"_a, "cdf"_a, "sample"_a, "eps"_a = 1e-6f, D(spline, sample_1d, 2)
    );

    spline.def("sample_1d",
        vectorize_wrapper(
            [](const py::array_t<Float> &nodes, const py::array_t<Float> &values,
               const py::array_t<Float> &cdf, FloatP sample, Float eps) {
                if (values.ndim() != 1)
                    throw std::runtime_error("'values' must be a one-dimensional array!");
                if (cdf.ndim() != 1)
                    throw std::runtime_error("'cdf' must be a one-dimensional array!");
                if (values.size() != cdf.size())
                    throw std::runtime_error("'values' and 'cdf' must have a matching size!");
                return spline::sample_1d(nodes.data(), values.data(), cdf.data(), (uint32_t) values.shape(0), sample, eps);
            }
        ), "nodes"_a, "values"_a, "cdf"_a, "sample"_a, "eps"_a = 1e-6f
    );

    // ------------------------------------------------------------------------------------------

    spline.def("eval_spline_weights", [](Float min, Float max, uint32_t size, Float x) {
            py::array_t<Float, 4> weight;
            int32_t offset;
            bool result;
            std::tie(result, offset) = spline::eval_spline_weights(
                min, max, size, x, weight.mutable_data());
            return std::make_tuple(result, offset, weight);
        },
        "min"_a, "max"_a, "size"_a, "x"_a, D(spline, eval_spline_weights)
    );

    // ------------------------------------------------------------------------------------------

    spline.def("eval_spline_weights", [](const py::array_t<Float> &nodes, Float x) {
            py::array_t<Float, 4> weight;
            int32_t offset;
            bool result;
            std::tie(result, offset) = spline::eval_spline_weights(
                nodes.data(), (uint32_t) nodes.shape(0), x, weight.mutable_data());
            return std::make_tuple(result, offset, weight);
        },
        "nodes"_a, "x"_a, D(spline, eval_spline_weights, 2)
    );

    // ------------------------------------------------------------------------------------------

    spline.def(
        "eval_2d",
        [](const py::array_t<Float> &nodes1, const py::array_t<Float> &nodes2,
           const py::array_t<Float> &values, Float x, Float y) {
            return spline::eval_2d(nodes1.data(), (uint32_t) nodes1.shape(0),
                                   nodes2.data(), (uint32_t) nodes2.shape(0),
                                   values.data(), x, y);
        },
        "nodes1"_a, "nodes2"_a, "values"_a, "x"_a, "y"_a, D(spline, eval_2d));

    spline.def(
        "eval_2d",
        vectorize_wrapper([](
            const py::array_t<Float> &nodes1, const py::array_t<Float> &nodes2,
            const py::array_t<Float> &values, FloatP x, FloatP y) {
            return spline::eval_2d(nodes1.data(), (uint32_t) nodes1.shape(0),
                                   nodes2.data(), (uint32_t) nodes2.shape(0),
                                   values.data(), x, y);
        }),
        "nodes1"_a, "nodes2"_a, "values"_a, "x"_a, "y"_a);
}
