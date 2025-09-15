#include <drjit/dynamic.h>
#include <drjit/tensor.h>
#include <mitsuba/core/distr_1d_cond.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>

MI_PY_EXPORT(ConditionalIrregular1D) {
    MI_PY_IMPORT_TYPES()

    using TYPE                   = UnpolarizedSpectrum;
    using ConditionalIrregular1D = mitsuba::ConditionalIrregular1D<TYPE>;
    using FloatStorage           = DynamicBuffer<Float>;

    MI_PY_CHECK_ALIAS(ConditionalIrregular1D, "ConditionalIrregular1D") {
        nb::class_<ConditionalIrregular1D>(m, "ConditionalIrregular1D")
            .def(nb::init<>(), D(ConditionalIrregular1D))
            .def(nb::init<const FloatStorage &, const FloatStorage &,
                          const std::vector<FloatStorage> &, bool, bool>(),
                 "nodes"_a, "pdf"_a, "nodes_cond"_a, "enable_sampling"_a = true,
                 "nearest"_a = false, D(ConditionalIrregular1D, ConditionalIrregular1D, 2))
            .def(
                "eval_pdf",
                [](const ConditionalIrregular1D *distr, TYPE x,
                   std::vector<TYPE> cond, Mask active = true) {
                    return distr->eval_pdf(x, cond, active);
                },
                "x"_a, "cond"_a, "active"_a = true, D(ConditionalIrregular1D, eval_pdf))
            .def(
                "eval_pdf_normalized",
                [](const ConditionalIrregular1D *distr, TYPE x,
                   std::vector<TYPE> cond, Mask active = true) {
                    return distr->eval_pdf_normalized(x, cond, active);
                },
                "x"_a, "cond"_a, "active"_a = true, D(ConditionalIrregular1D, eval_pdf_normalized))
            .def(
                "sample_pdf",
                [](const ConditionalIrregular1D *distr, TYPE u,
                   std::vector<TYPE> cond, Mask active = true) {
                    return distr->sample_pdf(u, cond, active);
                },
                "u"_a, "cond"_a, "active"_a = true, D(ConditionalIrregular1D, sample_pdf))
            .def(
                "integral",
                [](const ConditionalIrregular1D *distr,
                   std::vector<TYPE> cond) { return distr->integral(cond); },
                "cond"_a, D(ConditionalIrregular1D, integral))
            .def("tensor", &ConditionalIrregular1D::tensor,
                 nb::rv_policy::reference_internal, D(ConditionalIrregular1D, tensor))
            .def("update", &ConditionalIrregular1D::update, D(ConditionalIrregular1D, update))
            .def("empty", &ConditionalIrregular1D::empty, D(ConditionalIrregular1D, empty))
            .def("max", &ConditionalIrregular1D::max, D(ConditionalIrregular1D, max))
            .def_field(ConditionalIrregular1D, m_cdf, D(ConditionalIrregular1D, m_cdf))
            .def_field(ConditionalIrregular1D, m_integral, D(ConditionalIrregular1D, m_integral))
            .def_field(ConditionalIrregular1D, m_nodes, D(ConditionalIrregular1D, m_nodes))
            .def_repr(ConditionalIrregular1D);
    }
}

MI_PY_EXPORT(ConditionalRegular1D) {
    MI_PY_IMPORT_TYPES()

    using TYPE                 = UnpolarizedSpectrum;
    using ConditionalRegular1D = mitsuba::ConditionalRegular1D<TYPE>;
    using FloatStorage         = DynamicBuffer<Float>;

    MI_PY_CHECK_ALIAS(ConditionalRegular1D, "ConditionalRegular1D") {
        nb::class_<ConditionalRegular1D>(m, "ConditionalRegular1D")
            .def(nb::init<>(), D(ConditionalRegular1D))
            .def(nb::init<const FloatStorage &, const ScalarVector2f &,
                          const std::vector<ScalarVector2f> &,
                          const std::vector<ScalarUInt32> &, bool, bool>(),
                 "pdf"_a, "range"_a, "range_cond"_a, "size_cond"_a,
                 "enable_sampling"_a = true, "nearest"_a = false, D(ConditionalRegular1D, ConditionalRegular1D, 2))
            .def(
                "eval_pdf",
                [](const ConditionalRegular1D *distr, TYPE x,
                   std::vector<TYPE> cond, Mask active = true) {
                    return distr->eval_pdf(x, cond, active);
                },
                "x"_a, "cond"_a, "active"_a = true, D(ConditionalRegular1D, eval_pdf))
            .def(
                "eval_pdf_normalized",
                [](const ConditionalRegular1D *distr, TYPE x,
                   std::vector<TYPE> cond, Mask active = true) {
                    return distr->eval_pdf_normalized(x, cond, active);
                },
                "x"_a, "cond"_a, "active"_a = true, D(ConditionalRegular1D, eval_pdf_normalized))
            .def(
                "sample_pdf",
                [](const ConditionalRegular1D *distr, TYPE u,
                   std::vector<TYPE> cond, Mask active = true) {
                    return distr->sample_pdf(u, cond, active);
                },
                "u"_a, "cond"_a, "active"_a = true, D(ConditionalRegular1D, sample_pdf))
            .def(
                "integral",
                [](const ConditionalRegular1D *distr, std::vector<TYPE> cond) {
                    return distr->integral(cond);
                },
                "cond"_a, D(ConditionalRegular1D, integral))
            .def("tensor", &ConditionalRegular1D::tensor,
                 nb::rv_policy::reference_internal, D(ConditionalRegular1D, tensor))
            .def("update", &ConditionalRegular1D::update, D(ConditionalRegular1D, update))
            .def("empty", &ConditionalRegular1D::empty, D(ConditionalRegular1D, empty))
            .def("max", &ConditionalRegular1D::max, D(ConditionalRegular1D, max))
            .def_field(ConditionalRegular1D, m_integral, D(ConditionalRegular1D, m_integral))
            .def_field(ConditionalRegular1D, m_cdf, D(ConditionalRegular1D, m_cdf))
            .def_repr(ConditionalRegular1D);
    }
}
