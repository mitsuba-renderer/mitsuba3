#include <mitsuba/core/distr_1d.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/tuple.h>

#include <nanobind/stl/string.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/tuple.h>

MI_PY_EXPORT(DiscreteDistribution) {
    MI_PY_IMPORT_TYPES()

    using DiscreteDistribution = mitsuba::DiscreteDistribution<Float>;
    using FloatStorage = DynamicBuffer<Float>;

    MI_PY_CHECK_ALIAS(DiscreteDistribution, "DiscreteDistribution") {
        MI_PY_STRUCT(DiscreteDistribution)
            .def(nb::init<>(), D(DiscreteDistribution))
            .def(nb::init<const DiscreteDistribution &>(), "Copy constructor")
            .def(nb::init<const FloatStorage &>(), "pmf"_a,
                 D(DiscreteDistribution, DiscreteDistribution, 2))
            .def("__len__", &DiscreteDistribution::size)
            .def("size", &DiscreteDistribution::size, D(DiscreteDistribution, size))
            .def("empty", &DiscreteDistribution::empty, D(DiscreteDistribution, empty))
            .def_prop_rw("pmf",
              [](DiscreteDistribution &t) { return t.pmf() ; },
              [](DiscreteDistribution &t, FloatStorage& value) { t.pmf() = value; },
              D(DiscreteDistribution, pmf))
            .def_prop_rw("cdf",
              [](DiscreteDistribution &t) { return t.cdf() ; },
              [](DiscreteDistribution &t, FloatStorage& value) { t.cdf() = value; },
              D(DiscreteDistribution, cdf))
            .def("eval_pmf", &DiscreteDistribution::eval_pmf,
                 "index"_a, "active"_a = true, D(DiscreteDistribution, eval_pmf))
            .def("eval_pmf_normalized", &DiscreteDistribution::eval_pmf_normalized,
                 "index"_a, "active"_a = true, D(DiscreteDistribution, eval_pmf_normalized))
            .def("eval_cdf", &DiscreteDistribution::eval_cdf,
                 "index"_a, "active"_a = true, D(DiscreteDistribution, eval_cdf))
            .def("eval_cdf_normalized", &DiscreteDistribution::eval_cdf_normalized,
                 "index"_a, "active"_a = true, D(DiscreteDistribution, eval_cdf_normalized))
            .def_method(DiscreteDistribution, update)
            .def_method(DiscreteDistribution, normalization)
            .def_method(DiscreteDistribution, sum)
            .def("sample",
                &DiscreteDistribution::sample,
                "value"_a, "active"_a = true, D(DiscreteDistribution, sample))
            .def("sample_pmf",
                &DiscreteDistribution::sample_pmf,
                "value"_a, "active"_a = true, D(DiscreteDistribution, sample_pmf))
            .def("sample_reuse",
                &DiscreteDistribution::sample_reuse,
                "value"_a, "active"_a = true, D(DiscreteDistribution, sample_reuse))
            .def("sample_reuse_pmf",
                &DiscreteDistribution::sample_reuse_pmf,
                "value"_a, "active"_a = true, D(DiscreteDistribution, sample_reuse_pmf))
            .def_repr(DiscreteDistribution);
    }
}

MI_PY_EXPORT(ContinuousDistribution) {
    MI_PY_IMPORT_TYPES()

    using ContinuousDistribution = mitsuba::ContinuousDistribution<Float>;
    using FloatStorage = DynamicBuffer<Float>;

    MI_PY_CHECK_ALIAS(ContinuousDistribution, "ContinuousDistribution") {
        MI_PY_STRUCT(ContinuousDistribution)
            .def(nb::init<>(), D(ContinuousDistribution))
            .def(nb::init<const ContinuousDistribution &>(), "Copy constructor")
            .def(nb::init<const ScalarVector2f &, const FloatStorage &>(),
                 "range"_a, "pdf"_a, D(ContinuousDistribution, ContinuousDistribution, 2))
            .def("__len__", &ContinuousDistribution::size)
            .def("size", &ContinuousDistribution::size, D(ContinuousDistribution, size))
            .def("empty", &ContinuousDistribution::empty, D(ContinuousDistribution, empty))
            .def_prop_rw("range",
              [](ContinuousDistribution &t) { return t.range() ; },
              [](ContinuousDistribution &t, ScalarVector2f value) { t.range() = value; },
              D(ContinuousDistribution, range))
            .def_prop_rw("pdf",
              [](ContinuousDistribution &t) { return t.pdf() ; },
              [](ContinuousDistribution &t, FloatStorage& value) { t.pdf() = value; },
              D(ContinuousDistribution, pdf))
            .def_prop_rw("cdf",
              [](ContinuousDistribution &t) { return t.cdf() ; },
              [](ContinuousDistribution &t, FloatStorage& value) { t.cdf() = value; },
              D(ContinuousDistribution, cdf))
            .def("eval_pdf", &ContinuousDistribution::eval_pdf,
                 "x"_a, "active"_a = true, D(ContinuousDistribution, eval_pdf))
            .def("eval_pdf_normalized", &ContinuousDistribution::eval_pdf_normalized,
                 "x"_a, "active"_a = true, D(ContinuousDistribution, eval_pdf_normalized))
            .def("eval_cdf", &ContinuousDistribution::eval_cdf,
                 "x"_a, "active"_a = true, D(ContinuousDistribution, eval_cdf))
            .def("eval_cdf_normalized", &ContinuousDistribution::eval_cdf_normalized,
                 "x"_a, "active"_a = true, D(ContinuousDistribution, eval_cdf_normalized))
            .def_method(ContinuousDistribution, update)
            .def_method(ContinuousDistribution, integral)
            .def_method(ContinuousDistribution, normalization)
            .def_method(ContinuousDistribution, interval_resolution)
            .def_method(ContinuousDistribution, max)
            .def("sample",
                &ContinuousDistribution::sample,
                "value"_a, "active"_a = true, D(ContinuousDistribution, sample))
            .def("sample_pdf",
                &ContinuousDistribution::sample_pdf,
                "value"_a, "active"_a = true, D(ContinuousDistribution, sample_pdf))
            .def_repr(ContinuousDistribution);
    }
}

MI_PY_EXPORT(IrregularContinuousDistribution) {
    MI_PY_IMPORT_TYPES()

    using IrregularContinuousDistribution = mitsuba::IrregularContinuousDistribution<Float>;
    using FloatStorage = DynamicBuffer<Float>;

    MI_PY_CHECK_ALIAS(IrregularContinuousDistribution, "IrregularContinuousDistribution") {
        MI_PY_STRUCT(IrregularContinuousDistribution)
            .def(nb::init<>(), D(IrregularContinuousDistribution))
            .def(nb::init<const IrregularContinuousDistribution &>(), "Copy constructor")
            .def(nb::init<const FloatStorage &, const FloatStorage &>(),
                 "nodes"_a, "pdf"_a, D(IrregularContinuousDistribution, IrregularContinuousDistribution, 2))
            .def("__len__", &IrregularContinuousDistribution::size)
            .def("size", &IrregularContinuousDistribution::size, D(IrregularContinuousDistribution, size))
            .def("empty", &IrregularContinuousDistribution::empty, D(IrregularContinuousDistribution, empty))
            .def_prop_rw("range",
              [](IrregularContinuousDistribution &t) { return t.range() ; },
              [](IrregularContinuousDistribution &t, ScalarVector2f& value) { t.range() = value; },
              D(IrregularContinuousDistribution, range))
            .def_prop_rw("nodes",
              [](IrregularContinuousDistribution &t) { return t.nodes() ; },
              [](IrregularContinuousDistribution &t, FloatStorage& value) { t.nodes() = value; },
              D(IrregularContinuousDistribution, nodes))
            .def_prop_rw("pdf",
              [](IrregularContinuousDistribution &t) { return t.pdf() ; },
              [](IrregularContinuousDistribution &t, FloatStorage& value) { t.pdf() = value; },
              D(IrregularContinuousDistribution, nodes))
            .def_prop_rw("cdf",
              [](IrregularContinuousDistribution &t) { return t.cdf() ; },
              [](IrregularContinuousDistribution &t, FloatStorage& value) { t.cdf() = value; },
              D(IrregularContinuousDistribution, nodes))
            .def("eval_pdf", &IrregularContinuousDistribution::eval_pdf,
                 "x"_a, "active"_a = true, D(IrregularContinuousDistribution, eval_pdf))
            .def("eval_pdf_normalized", &IrregularContinuousDistribution::eval_pdf_normalized,
                 "x"_a, "active"_a = true, D(IrregularContinuousDistribution, eval_pdf_normalized))
            .def("eval_cdf", &IrregularContinuousDistribution::eval_cdf,
                 "x"_a, "active"_a = true, D(IrregularContinuousDistribution, eval_cdf))
            .def("eval_cdf_normalized", &IrregularContinuousDistribution::eval_cdf_normalized,
                 "x"_a, "active"_a = true, D(IrregularContinuousDistribution, eval_cdf_normalized))
            .def_method(IrregularContinuousDistribution, update)
            .def_method(IrregularContinuousDistribution, integral)
            .def_method(IrregularContinuousDistribution, normalization)
            .def_method(IrregularContinuousDistribution, interval_resolution)
            .def_method(IrregularContinuousDistribution, max)
            .def("sample",
                &IrregularContinuousDistribution::sample,
                "value"_a, "active"_a = true, D(IrregularContinuousDistribution, sample))
            .def("sample_pdf",
                &IrregularContinuousDistribution::sample_pdf,
                "value"_a, "active"_a = true, D(IrregularContinuousDistribution, sample_pdf))
            .def_repr(IrregularContinuousDistribution);
    }
}
