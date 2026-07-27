#include <drjit/dynamic.h>
#include <drjit/tensor.h>
#include <mitsuba/core/distr_1d_cond.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>

template <typename Type>
void bind_conditional_irregular_1d(nb::module_ &m, const char *name) {
    MI_PY_IMPORT_TYPES()
    using FloatStorage           = DynamicBuffer<Float>;
    using ConditionalIrregular1D = mitsuba::ConditionalIrregular1D<Type>;

    MI_PY_CHECK_ALIAS(ConditionalIrregular1D, name) {
        nb::class_<ConditionalIrregular1D>(m, name)
            .def(nb::init<>(), D(ConditionalIrregular1D))
            .def(nb::init<const FloatStorage &, const FloatStorage &,
                          const std::vector<FloatStorage> &>(),
                 "nodes"_a, "pdf"_a, "nodes_cond"_a,
                 D(ConditionalIrregular1D, ConditionalIrregular1D, 2))
            .def(nb::init<const FloatStorage &, const TensorXf &,
                          const std::vector<FloatStorage> &>(),
                 "nodes"_a, "pdf"_a, "nodes_cond"_a,
                 D(ConditionalIrregular1D, ConditionalIrregular1D, 3))
            .def(
                "eval_pdf",
                [](const ConditionalIrregular1D *distr, Type x,
                   std::vector<Type> cond, Mask active = true) {
                    return distr->eval_pdf(x, cond, active);
                },
                "x"_a, "cond"_a, "active"_a = true, D(ConditionalIrregular1D, eval_pdf))
            .def(
                "eval_pdf_normalized",
                [](const ConditionalIrregular1D *distr, Type x,
                   std::vector<Type> cond, Mask active = true) {
                    return distr->eval_pdf_normalized(x, cond, active);
                },
                "x"_a, "cond"_a, "active"_a = true, D(ConditionalIrregular1D, eval_pdf_normalized))
            .def(
                "sample_pdf",
                [](const ConditionalIrregular1D *distr, Type u,
                   std::vector<Type> cond, Mask active = true) {
                    return distr->sample_pdf(u, cond, active);
                },
                "u"_a, "cond"_a, "active"_a = true, D(ConditionalIrregular1D, sample_pdf))
            .def(
                "integral",
                [](const ConditionalIrregular1D *distr,
                   std::vector<Type> cond) { return distr->integral(cond); },
                "cond"_a, D(ConditionalIrregular1D, integral))
            .def("update", &ConditionalIrregular1D::update, D(ConditionalIrregular1D, update))
            .def("empty", &ConditionalIrregular1D::empty, D(ConditionalIrregular1D, empty))
            .def("max", &ConditionalIrregular1D::max, D(ConditionalIrregular1D, max))
            .def_prop_rw("pdf",
              [](ConditionalIrregular1D &t) { return t.pdf(); },
              [](ConditionalIrregular1D &t, TensorXf &value) { t.pdf() = value; },
              D(ConditionalIrregular1D, pdf))
            .def_prop_rw("nodes",
              [](ConditionalIrregular1D &t) { return t.nodes(); },
              [](ConditionalIrregular1D &t, FloatStorage &value) { t.nodes() = value; },
              D(ConditionalIrregular1D, nodes))
            .def_prop_rw("nodes_cond",
              [](ConditionalIrregular1D &t) { return t.nodes_cond(); },
              [](ConditionalIrregular1D &t, std::vector<FloatStorage> &value) { t.nodes_cond() = value; },
              D(ConditionalIrregular1D, nodes_cond))
            .def_prop_rw("cdf_array",
              [](ConditionalIrregular1D &t) { return t.cdf_array(); },
              [](ConditionalIrregular1D &t, FloatStorage &value) { t.cdf_array() = value; },
              D(ConditionalIrregular1D, cdf_array))
            .def_prop_rw("integral_array",
              [](ConditionalIrregular1D &t) { return t.integral_array(); },
              [](ConditionalIrregular1D &t, FloatStorage &value) { t.integral_array() = value; },
              D(ConditionalIrregular1D, integral_array))
            .def_repr(ConditionalIrregular1D);
    }
}

template <typename Type>
void bind_conditional_regular_1d(nb::module_ &m, const char *name) {
    MI_PY_IMPORT_TYPES()
    using ConditionalRegular1D = mitsuba::ConditionalRegular1D<Type>;
    using FloatStorage         = DynamicBuffer<Float>;

    MI_PY_CHECK_ALIAS(ConditionalRegular1D, name) {
        nb::class_<ConditionalRegular1D>(m, name)
            .def(nb::init<>(), D(ConditionalRegular1D))
            .def(nb::init<const FloatStorage &, const ScalarVector2f &,
                          const std::vector<ScalarVector2f> &,
                          const std::vector<ScalarUInt32> &>(),
                 "pdf"_a, "range"_a, "range_cond"_a, "size_cond"_a,
                 D(ConditionalRegular1D, ConditionalRegular1D, 2))
            .def(nb::init<const TensorXf &, const ScalarVector2f &,
                          const std::vector<ScalarVector2f> &>(),
                 "pdf"_a, "range"_a, "range_cond"_a,
                 D(ConditionalRegular1D, ConditionalRegular1D, 3))
            .def(
                "eval_pdf",
                [](const ConditionalRegular1D *distr, Type x,
                   std::vector<Type> cond, Mask active = true) {
                    return distr->eval_pdf(x, cond, active);
                },
                "x"_a, "cond"_a, "active"_a = true, D(ConditionalRegular1D, eval_pdf))
            .def(
                "eval_pdf_normalized",
                [](const ConditionalRegular1D *distr, Type x,
                   std::vector<Type> cond, Mask active = true) {
                    return distr->eval_pdf_normalized(x, cond, active);
                },
                "x"_a, "cond"_a, "active"_a = true, D(ConditionalRegular1D, eval_pdf_normalized))
            .def(
                "sample_pdf",
                [](const ConditionalRegular1D *distr, Type u,
                   std::vector<Type> cond, Mask active = true) {
                    return distr->sample_pdf(u, cond, active);
                },
                "u"_a, "cond"_a, "active"_a = true, D(ConditionalRegular1D, sample_pdf))
            .def(
                "integral",
                [](const ConditionalRegular1D *distr, std::vector<Type> cond) {
                    return distr->integral(cond);
                },
                "cond"_a, D(ConditionalRegular1D, integral))
            .def("update", &ConditionalRegular1D::update, D(ConditionalRegular1D, update))
            .def("empty", &ConditionalRegular1D::empty, D(ConditionalRegular1D, empty))
            .def("max", &ConditionalRegular1D::max, D(ConditionalRegular1D, max))
            .def_prop_rw("pdf",
              [](ConditionalRegular1D &t) { return t.pdf(); },
              [](ConditionalRegular1D &t, TensorXf &value) { t.pdf() = value; },
              D(ConditionalRegular1D, pdf))
            .def_prop_rw("range",
              [](ConditionalRegular1D &t) { return t.range(); },
              [](ConditionalRegular1D &t, ScalarVector2f value) { t.range() = value; },
              D(ConditionalRegular1D, range))
            .def_prop_rw("range_cond",
              [](ConditionalRegular1D &t) { return t.range_cond(); },
              [](ConditionalRegular1D &t, std::vector<ScalarVector2f> &value) { t.range_cond() = value; },
              D(ConditionalRegular1D, range_cond))
            .def_prop_rw("cdf_array",
              [](ConditionalRegular1D &t) { return t.cdf_array(); },
              [](ConditionalRegular1D &t, FloatStorage &value) { t.cdf_array() = value; },
              D(ConditionalRegular1D, cdf_array))
            .def_prop_rw("integral_array",
              [](ConditionalRegular1D &t) { return t.integral_array(); },
              [](ConditionalRegular1D &t, FloatStorage &value) { t.integral_array() = value; },
              D(ConditionalRegular1D, integral_array))
            .def_repr(ConditionalRegular1D);
    }
}

MI_PY_EXPORT(ConditionalIrregular1D) {
    MI_PY_IMPORT_TYPES()

    bind_conditional_irregular_1d<Float>(m, "ConditionalIrregular1D");
    bind_conditional_irregular_1d<UnpolarizedSpectrum>(m, "ConditionalIrregular1DSpectrum");
}

MI_PY_EXPORT(ConditionalRegular1D) {
    MI_PY_IMPORT_TYPES()

    bind_conditional_regular_1d<Float>(m, "ConditionalRegular1D");
    bind_conditional_regular_1d<UnpolarizedSpectrum>(m, "ConditionalRegular1DSpectrum");
}
