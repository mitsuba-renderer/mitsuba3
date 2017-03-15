#include <mitsuba/core/spline.h>
#include <Eigen/Core>
#include "python.h"
/* Dynamic vectors */
typedef Eigen::Matrix<Float, Eigen::Dynamic, 1> VectorX;

MTS_PY_EXPORT(spline) {
    MTS_PY_IMPORT_MODULE(spline, "mitsuba.core.spline");

    spline.def("eval_spline",   spline::eval_spline<Float>,   "f0"_a, "f1"_a, "d0"_a, "d1"_a, "t"_a);

    spline.def("eval_spline_d", spline::eval_spline_d<Float>, "f0"_a, "f1"_a, "d0"_a, "d1"_a, "t"_a);

    spline.def("eval_spline_i", spline::eval_spline_i<Float>, "f0"_a, "f1"_a, "d0"_a, "d1"_a, "t"_a);

// ------------------------------------------------------------------------------------------

    spline.def("eval_1d", [](Float min, Float max, const py::array_t<Float> &values, Float x){
        if(values.ndim() != 1)
            throw std::runtime_error("'values' must be a one-dimentional array!");
        return spline::eval_1d(min, max, values.data(), values.shape(0), x);
    });

    spline.def("eval_1d", 
        vectorize_wrapper(
            [](Float min, Float max, const py::array_t<Float> &values, FloatP x){
                if(values.ndim() != 1)
                    throw std::runtime_error("'values' must be a one-dimentional array!");
                return spline::eval_1d(min, max, values.data(), values.shape(0), x);
            }
        ), "min"_a, "max"_a, "values"_a, "x"_a
    );

// ------------------------------------------------------------------------------------------

    spline.def("eval_1d", [](const py::array_t<Float> &nodes, const py::array_t<Float> &values, Float x){
        if(nodes.ndim() != 1 || values.ndim() != 1)
            throw std::runtime_error("'nodes' and 'values' must be a one-dimentional array!");
        if (nodes.shape(0) != values.shape(0))
            throw std::runtime_error("'nodes' and 'values' must have a matching size!");
        return spline::eval_1d(nodes.data(), values.data(), values.shape(0), x);
    });

    spline.def("eval_1d",
        vectorize_wrapper(
            [](const py::array_t<Float> &nodes, const py::array_t<Float> &values, FloatP x){
                if(nodes.ndim() != 1 || values.ndim() != 1)
                    throw std::runtime_error("'nodes' and 'values' must be a one-dimentional array!");
                if (nodes.shape(0) != values.shape(0))
                    throw std::runtime_error("'nodes' and 'values' must have a matching size!");
                return spline::eval_1d(nodes.data(), values.data(), values.shape(0), x);
            }
        ), "nodes"_a, "values"_a, "x"_a
    );

// ------------------------------------------------------------------------------------------

    spline.def("integrate_1d", [](Float min, Float max, const py::array_t<Float> &values) {
        if (values.ndim() != 1)
            throw std::runtime_error("'values' must be a one-dimentional array!");
        py::array_t<Float> result(values.size());
        spline::integrate_1d(min, max, values.data(), values.size(), result.mutable_data());
        return result;
    });

    spline.def("integrate_1d", 
        [](const py::array_t<Float> &nodes, const py::array_t<Float> &values) {
            if (nodes.ndim() != 1 || values.ndim() != 1)
                throw std::runtime_error("'nodes' and 'values' must be a one-dimentional array!");        
            if (nodes.shape(0) != values.shape(0))
                throw std::runtime_error("'nodes' and 'values' must have a matching size!");
            py::array_t<Float> result(values.size());
            spline::integrate_1d(nodes.data(), values.data(), values.size(), result.mutable_data());
            return result;
        }
    );

    // ------------------------------------------------------------------------------------------
    
    spline.def("invert_1d", [](Float min, Float max, const py::array_t<Float> &values, Float y) {
        if (values.ndim() != 1)
            throw std::runtime_error("'values' must be a one-dimentional array!");
        return spline::invert_1d(min, max, values.data(), values.shape(0), y);
    });

    spline.def("invert_1d", 
        vectorize_wrapper(
            [](Float min, Float max, const py::array_t<Float> &values, FloatP y) {
                if (values.ndim() != 1)
                    throw std::runtime_error("'values' must be a one-dimentional array!");
                return spline::invert_1d(min, max, values.data(), values.shape(0), y);
            }
        ), "min"_a, "max"_a, "values"_a, "y"_a
    );

    // ------------------------------------------------------------------------------------------

    spline.def("invert_1d", [](const py::array_t<Float> &nodes, const py::array_t<Float> &values, Float y) {
        if (nodes.ndim() != 1 || values.ndim() != 1)
            throw std::runtime_error("'nodes' and 'values' must be a one-dimentional array!");
        if (nodes.shape(0) != values.shape(0))
            throw std::runtime_error("'nodes' and 'values' must have a matching size!");
        return spline::invert_1d(nodes.data(), values.data(), values.shape(0), y);
    });

    spline.def("invert_1d", 
        vectorize_wrapper(
            [](const py::array_t<Float> &nodes, const py::array_t<Float> &values, FloatP y) {
                if (nodes.ndim() != 1 || values.ndim() != 1)
                    throw std::runtime_error("'nodes' and 'values' must be a one-dimentional array!");
                if (nodes.shape(0) != values.shape(0))
                    throw std::runtime_error("'nodes' and 'values' must have a matching size!");
                return spline::invert_1d(nodes.data(), values.data(), values.shape(0), y);
            }
        ), "nodes"_a, "values"_a, "y"_a
    );

    // ------------------------------------------------------------------------------------------
    
    spline.def("sample_1d", [](Float min, Float max, const py::array_t<Float> &values, 
                               const py::array_t<Float> &cdf, Float sample) {
        if (values.ndim() != 1)
            throw std::runtime_error("'values' must be a one-dimentional array!");
        if (cdf.ndim() != 1)
            throw std::runtime_error("'cdf' must be a one-dimentional array!");
        if (values.size() != cdf.size())
            throw std::runtime_error("'values' and 'cdf' must have a matching size!");
        Float pos, fval, pdf;
        pos = spline::sample_1d(min, max, values.data(), cdf.data(), values.shape(0),
                                 sample, &fval, &pdf);
        return std::make_tuple(pos, fval, pdf);
    }); 

    spline.def("sample_1d", 
        vectorize_wrapper(
            [](Float min, Float max, const py::array_t<Float> &values,
                const py::array_t<Float> &cdf, FloatP sample) {
                if (values.ndim() != 1)
                    throw std::runtime_error("'values' must be a one-dimentional array!");
                if (cdf.ndim() != 1)
                    throw std::runtime_error("'cdf' must be a one-dimentional array!");
                if (values.size() != cdf.size())
                    throw std::runtime_error("'values' and 'cdf' must have a matching size!");
                FloatP pos, fval, pdf;
                pos = spline::sample_1d(min, max, values.data(), cdf.data(), values.shape(0),
                    sample, &fval, &pdf);
                return std::make_tuple(pos, fval, pdf);
            }
        ), "min"_a, "max"_a, "values"_a, "cdf"_a, "sample"_a
    );

    // ------------------------------------------------------------------------------------------

    spline.def("sample_1d", [](const py::array_t<Float> &nodes, const py::array_t<Float> &values,
        const py::array_t<Float> &cdf, Float sample) {
        if (values.ndim() != 1)
            throw std::runtime_error("'values' must be a one-dimentional array!");
        if (cdf.ndim() != 1)
            throw std::runtime_error("'cdf' must be a one-dimentional array!");
        if (values.size() != cdf.size())
            throw std::runtime_error("'values' and 'cdf' must have a matching size!");
        Float pos, fval, pdf;
        pos = spline::sample_1d(nodes.data(), values.data(), cdf.data(), values.shape(0),
            sample, &fval, &pdf);
        return std::make_tuple(pos, fval, pdf);
    });

    spline.def("sample_1d", 
        vectorize_wrapper(
            [](const py::array_t<Float> &nodes, const py::array_t<Float> &values,
                const py::array_t<Float> &cdf, FloatP sample) {
                if (values.ndim() != 1)
                    throw std::runtime_error("'values' must be a one-dimentional array!");
                if (cdf.ndim() != 1)
                    throw std::runtime_error("'cdf' must be a one-dimentional array!");
                if (values.size() != cdf.size())
                    throw std::runtime_error("'values' and 'cdf' must have a matching size!");
                FloatP pos, fval, pdf;
                pos = spline::sample_1d(nodes.data(), values.data(), cdf.data(), values.shape(0),
                    sample, &fval, &pdf);
                return std::make_tuple(pos, fval, pdf);
            }
        ), "nodes"_a, "values"_a, "cdf"_a, "sample"_a
    );

    // ------------------------------------------------------------------------------------------

    spline.def("eval_spline_weights", [](Float min, Float max, size_t size, Float x) {
        py::array_t<Float, 4> weight;
        ssize_t offset;
        bool result;
        std::tie<bool, ssize_t>(result, offset) = spline::eval_spline_weights(min, max, size, x, weight.mutable_data());
        return std::make_tuple(result, offset, weight);
    });

#if 0
    spline.def("eval_spline_weights", [](Float min, Float max, size_t size, FloatP x) {
        py::array_t<FloatP, 4> weight;
        spline::SizeP offset;
        spline::BoolP result;
        result = spline::eval_spline_weights(min, max, size, x, offset, weight.mutable_data());
        return std::make_tuple(result, offset, weight);
    });
#endif
    // ------------------------------------------------------------------------------------------

    spline.def("eval_spline_weights", [](const py::array_t<Float> &nodes, Float x) {
        py::array_t<Float, 4> weight;
        ssize_t offset;
        bool result;
        std::tie<bool, ssize_t>(result, offset) = spline::eval_spline_weights(nodes.data(), nodes.shape(0), x, weight.mutable_data());
        return std::make_tuple(result, offset, weight);
    });

#if 0
    spline.def("eval_spline_weights", [](const py::array_t<Float> &nodes, FloatP x) {
        py::array_t<FloatP, 4> weight;
        spline::SizeP offset;
        spline::BoolP result;
        result = spline::eval_spline_weights(nodes.data(), nodes.shape(0), x, offset, weight.mutable_data());
        return std::make_tuple(result, offset, weight);
    });
#endif
    // ------------------------------------------------------------------------------------------

    spline.def("eval_2d", [](const py::array_t<Float> &nodes1, 
        const py::array_t<Float> &nodes2, const py::array_t<Float> &values, Float x, Float y) {
            return spline::eval_2d(nodes1.data(), nodes1.shape(0), 
                                   nodes2.data(), nodes2.shape(0), values.data(), x, y);
    });

    spline.def("eval_2d",
        vectorize_wrapper(
            [](const py::array_t<Float> &nodes1,
               const py::array_t<Float> &nodes2, 
               const py::array_t<Float> &values, 
               FloatP x, FloatP y) {
                return spline::eval_2d(nodes1.data(), nodes1.shape(0), 
                                       nodes2.data(), nodes2.shape(0), values.data(), x, y);
            }
        ), "nodes1"_a, "nodes2"_a, "values"_a, "x"_a, "y"_a
    );
}