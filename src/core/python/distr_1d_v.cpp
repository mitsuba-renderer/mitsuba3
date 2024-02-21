#include <mitsuba/core/distr_1d.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(DiscreteDistribution) {
    MI_PY_IMPORT_TYPES()

    using DiscreteDistribution = mitsuba::DiscreteDistribution<Float>;
    using FloatStorage = DynamicBuffer<Float>;

    MI_PY_STRUCT(DiscreteDistribution)
        .def(nb::init<>(), D(DiscreteDistribution))
        .def(nb::init<const DiscreteDistribution &>(), "Copy constructor")
        .def(nb::init<const FloatStorage &>(), "pmf"_a,
             D(DiscreteDistribution, DiscreteDistribution, 2))
        .def("__len__", &DiscreteDistribution::size)
        .def("size", &DiscreteDistribution::size, D(DiscreteDistribution, size))
        .def("empty", &DiscreteDistribution::empty, D(DiscreteDistribution, empty))
        .def("pmf", nb::overload_cast<>(&DiscreteDistribution::pmf),
             D(DiscreteDistribution, pmf))
        .def("cdf", nb::overload_cast<>(&DiscreteDistribution::cdf),
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

MI_PY_EXPORT(ContinuousDistribution) {
    MI_PY_IMPORT_TYPES()

    using ContinuousDistribution = mitsuba::ContinuousDistribution<Float>;
    using FloatStorage = DynamicBuffer<Float>;

    MI_PY_STRUCT(ContinuousDistribution)
        .def(nb::init<>(), D(ContinuousDistribution))
        .def(nb::init<const ContinuousDistribution &>(), "Copy constructor")
        .def(nb::init<const ScalarVector2f &, const FloatStorage &>(),
             "range"_a, "pdf"_a, D(ContinuousDistribution, ContinuousDistribution, 2))
        .def("__len__", &ContinuousDistribution::size)
        .def("size", &ContinuousDistribution::size, D(ContinuousDistribution, size))
        .def("empty", &ContinuousDistribution::empty, D(ContinuousDistribution, empty))
        .def("range", nb::overload_cast<>(&ContinuousDistribution::range),
             D(ContinuousDistribution, range))
        .def("pdf", nb::overload_cast<>(&ContinuousDistribution::pdf),
             D(ContinuousDistribution, pdf))
        .def("cdf", nb::overload_cast<>(&ContinuousDistribution::cdf),
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

MI_PY_EXPORT(IrregularContinuousDistribution) {
    MI_PY_IMPORT_TYPES()

    using IrregularContinuousDistribution = mitsuba::IrregularContinuousDistribution<Float>;
    using FloatStorage = DynamicBuffer<Float>;

    MI_PY_STRUCT(IrregularContinuousDistribution)
        .def(nb::init<>(), D(IrregularContinuousDistribution))
        .def(nb::init<const IrregularContinuousDistribution &>(), "Copy constructor")
        .def(nb::init<const FloatStorage &, const FloatStorage &>(),
             "nodes"_a, "pdf"_a, D(IrregularContinuousDistribution, IrregularContinuousDistribution, 2))
        .def("__len__", &IrregularContinuousDistribution::size)
        .def("size", &IrregularContinuousDistribution::size, D(IrregularContinuousDistribution, size))
        .def("empty", &IrregularContinuousDistribution::empty, D(IrregularContinuousDistribution, empty))
        .def("range", nb::overload_cast<>(&IrregularContinuousDistribution::range),
             D(IrregularContinuousDistribution, range))
        .def("nodes", nb::overload_cast<>(&IrregularContinuousDistribution::nodes),
             D(IrregularContinuousDistribution, nodes))
        .def("pdf", nb::overload_cast<>(&IrregularContinuousDistribution::pdf),
             D(IrregularContinuousDistribution, pdf))
        .def("cdf", nb::overload_cast<>(&IrregularContinuousDistribution::cdf),
             D(IrregularContinuousDistribution, cdf))
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
