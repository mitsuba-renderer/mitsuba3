#include <mitsuba/core/distr_1d.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(DiscreteDistribution) {
    MTS_PY_IMPORT_TYPES()

    using DiscreteDistribution = mitsuba::DiscreteDistribution<Float>;
    using FloatStorage = DynamicBuffer<Float>;

    MTS_PY_STRUCT(DiscreteDistribution, py::module_local())
        .def(py::init<>(), D(DiscreteDistribution))
        .def(py::init<const DiscreteDistribution &>(), "Copy constructor")
        .def(py::init<const FloatStorage &>(), "pmf"_a,
             D(DiscreteDistribution, DiscreteDistribution, 2))
        .def("__len__", &DiscreteDistribution::size)
        .def("size", &DiscreteDistribution::size, D(DiscreteDistribution, size))
        .def("empty", &DiscreteDistribution::empty, D(DiscreteDistribution, empty))
        .def("pmf", py::overload_cast<>(&DiscreteDistribution::pmf),
             D(DiscreteDistribution, pmf), py::return_value_policy::reference_internal)
        .def("cdf", py::overload_cast<>(&DiscreteDistribution::cdf),
             D(DiscreteDistribution, cdf), py::return_value_policy::reference_internal)
        .def("eval_pmf", vectorize(&DiscreteDistribution::eval_pmf<Float>),
             "index"_a, "active"_a = true, D(DiscreteDistribution, eval_pmf))
        .def("eval_pmf", vectorize(&DiscreteDistribution::eval_pmf<Wavelength>),
             "index"_a, "active"_a = true, D(DiscreteDistribution, eval_pmf))
        .def("eval_pmf_normalized", vectorize(&DiscreteDistribution::eval_pmf_normalized<Float>),
             "index"_a, "active"_a = true, D(DiscreteDistribution, eval_pmf_normalized))
        .def("eval_pmf_normalized", vectorize(&DiscreteDistribution::eval_pmf_normalized<Wavelength>),
             "index"_a, "active"_a = true, D(DiscreteDistribution, eval_pmf_normalized))
        .def("eval_cdf", vectorize(&DiscreteDistribution::eval_cdf<Float>),
             "index"_a, "active"_a = true, D(DiscreteDistribution, eval_cdf))
        .def("eval_cdf", vectorize(&DiscreteDistribution::eval_cdf<Wavelength>),
             "index"_a, "active"_a = true, D(DiscreteDistribution, eval_cdf))
        .def("eval_cdf_normalized", vectorize(&DiscreteDistribution::eval_cdf_normalized<Float>),
             "index"_a, "active"_a = true, D(DiscreteDistribution, eval_cdf_normalized))
        .def("eval_cdf_normalized", vectorize(&DiscreteDistribution::eval_cdf_normalized<Wavelength>),
             "index"_a, "active"_a = true, D(DiscreteDistribution, eval_cdf_normalized))
        .def_method(DiscreteDistribution, update)
        .def_method(DiscreteDistribution, sum)
        .def_method(DiscreteDistribution, normalization)
        .def("sample", vectorize(&DiscreteDistribution::sample<Float>),
            "value"_a, "active"_a = true, D(DiscreteDistribution, sample))
        .def("sample", vectorize(&DiscreteDistribution::sample<Wavelength>),
            "value"_a, "active"_a = true, D(DiscreteDistribution, sample))
        .def("sample_pmf", vectorize(&DiscreteDistribution::sample_pmf<Float>),
            "value"_a, "active"_a = true, D(DiscreteDistribution, sample_pmf))
        .def("sample_pmf", vectorize(&DiscreteDistribution::sample_pmf<Wavelength>),
            "value"_a, "active"_a = true, D(DiscreteDistribution, sample_pmf))
        .def("sample_reuse", vectorize(&DiscreteDistribution::sample_reuse<Float>),
            "value"_a, "active"_a = true, D(DiscreteDistribution, sample_reuse))
        .def("sample_reuse", vectorize(&DiscreteDistribution::sample_reuse<Wavelength>),
            "value"_a, "active"_a = true, D(DiscreteDistribution, sample_reuse))
        .def("sample_reuse_pmf", vectorize(&DiscreteDistribution::sample_reuse_pmf<Float>),
            "value"_a, "active"_a = true, D(DiscreteDistribution, sample_reuse_pmf))
        .def("sample_reuse_pmf", vectorize(&DiscreteDistribution::sample_reuse_pmf<Wavelength>),
            "value"_a, "active"_a = true, D(DiscreteDistribution, sample_reuse_pmf))
        .def_repr(DiscreteDistribution);
}

MTS_PY_EXPORT(ContinuousDistribution) {
    MTS_PY_IMPORT_TYPES()

    using ContinuousDistribution = mitsuba::ContinuousDistribution<Float>;
    using FloatStorage = DynamicBuffer<Float>;

    MTS_PY_STRUCT(ContinuousDistribution, py::module_local())
        .def(py::init<>(), D(ContinuousDistribution))
        .def(py::init<const ContinuousDistribution &>(), "Copy constructor")
        .def(py::init<const ScalarVector2f &, const FloatStorage &>(),
             "range"_a, "pdf"_a, D(ContinuousDistribution, ContinuousDistribution, 2))
        .def("__len__", &ContinuousDistribution::size)
        .def("size", &ContinuousDistribution::size, D(ContinuousDistribution, size))
        .def("empty", &ContinuousDistribution::empty, D(ContinuousDistribution, empty))
        .def("range", py::overload_cast<>(&ContinuousDistribution::range),
             D(ContinuousDistribution, range), py::return_value_policy::reference_internal)
        .def("pdf", py::overload_cast<>(&ContinuousDistribution::pdf),
             D(ContinuousDistribution, pdf), py::return_value_policy::reference_internal)
        .def("cdf", py::overload_cast<>(&ContinuousDistribution::cdf),
             D(ContinuousDistribution, cdf), py::return_value_policy::reference_internal)
        .def("eval_pdf", vectorize(&ContinuousDistribution::eval_pdf<Float>),
             "x"_a, "active"_a = true, D(ContinuousDistribution, eval_pdf))
        .def("eval_pdf", vectorize(&ContinuousDistribution::eval_pdf<Wavelength>),
             "x"_a, "active"_a = true, D(ContinuousDistribution, eval_pdf))
        .def("eval_pdf_normalized", vectorize(&ContinuousDistribution::eval_pdf_normalized<Float>),
             "x"_a, "active"_a = true, D(ContinuousDistribution, eval_pdf_normalized))
        .def("eval_pdf_normalized", vectorize(&ContinuousDistribution::eval_pdf_normalized<Wavelength>),
             "x"_a, "active"_a = true, D(ContinuousDistribution, eval_pdf_normalized))
        .def("eval_cdf", vectorize(&ContinuousDistribution::eval_cdf<Float>),
             "x"_a, "active"_a = true, D(ContinuousDistribution, eval_cdf))
        .def("eval_cdf", vectorize(&ContinuousDistribution::eval_cdf<Wavelength>),
             "x"_a, "active"_a = true, D(ContinuousDistribution, eval_cdf))
        .def("eval_cdf_normalized", vectorize(&ContinuousDistribution::eval_cdf_normalized<Float>),
             "x"_a, "active"_a = true, D(ContinuousDistribution, eval_cdf_normalized))
        .def("eval_cdf_normalized", vectorize(&ContinuousDistribution::eval_cdf_normalized<Wavelength>),
             "x"_a, "active"_a = true, D(ContinuousDistribution, eval_cdf_normalized))
        .def_method(ContinuousDistribution, update)
        .def_method(ContinuousDistribution, integral)
        .def_method(ContinuousDistribution, normalization)
        .def("sample", vectorize(&ContinuousDistribution::sample<Float>),
            "value"_a, "active"_a = true, D(ContinuousDistribution, sample))
        .def("sample", vectorize(&ContinuousDistribution::sample<Wavelength>),
            "value"_a, "active"_a = true, D(ContinuousDistribution, sample))
        .def("sample_pdf", vectorize(&ContinuousDistribution::sample_pdf<Float>),
            "value"_a, "active"_a = true, D(ContinuousDistribution, sample_pdf))
        .def("sample_pdf", vectorize(&ContinuousDistribution::sample_pdf<Wavelength>),
            "value"_a, "active"_a = true, D(ContinuousDistribution, sample_pdf))
        .def_repr(ContinuousDistribution);
}

MTS_PY_EXPORT(IrregularContinuousDistribution) {
    MTS_PY_IMPORT_TYPES()

    using IrregularContinuousDistribution = mitsuba::IrregularContinuousDistribution<Float>;
    using FloatStorage = DynamicBuffer<Float>;

    MTS_PY_STRUCT(IrregularContinuousDistribution, py::module_local())
        .def(py::init<>(), D(IrregularContinuousDistribution))
        .def(py::init<const IrregularContinuousDistribution &>(), "Copy constructor")
        .def(py::init<const FloatStorage &, const FloatStorage &>(),
             "nodes"_a, "pdf"_a, D(IrregularContinuousDistribution, IrregularContinuousDistribution, 2))
        .def("__len__", &IrregularContinuousDistribution::size)
        .def("size", &IrregularContinuousDistribution::size, D(IrregularContinuousDistribution, size))
        .def("empty", &IrregularContinuousDistribution::empty, D(IrregularContinuousDistribution, empty))
        .def("nodes", py::overload_cast<>(&IrregularContinuousDistribution::nodes),
             D(IrregularContinuousDistribution, nodes), py::return_value_policy::reference_internal)
        .def("pdf", py::overload_cast<>(&IrregularContinuousDistribution::pdf),
             D(IrregularContinuousDistribution, pdf), py::return_value_policy::reference_internal)
        .def("cdf", py::overload_cast<>(&IrregularContinuousDistribution::cdf),
             D(IrregularContinuousDistribution, cdf), py::return_value_policy::reference_internal)
        .def("eval_pdf", vectorize(&IrregularContinuousDistribution::eval_pdf<Float>),
             "x"_a, "active"_a = true, D(IrregularContinuousDistribution, eval_pdf))
        .def("eval_pdf", vectorize(&IrregularContinuousDistribution::eval_pdf<Wavelength>),
             "x"_a, "active"_a = true, D(IrregularContinuousDistribution, eval_pdf))
        .def("eval_pdf_normalized", vectorize(&IrregularContinuousDistribution::eval_pdf_normalized<Float>),
             "x"_a, "active"_a = true, D(IrregularContinuousDistribution, eval_pdf_normalized))
        .def("eval_pdf_normalized", vectorize(&IrregularContinuousDistribution::eval_pdf_normalized<Wavelength>),
             "x"_a, "active"_a = true, D(IrregularContinuousDistribution, eval_pdf_normalized))
        .def("eval_cdf", vectorize(&IrregularContinuousDistribution::eval_cdf<Float>),
             "x"_a, "active"_a = true, D(IrregularContinuousDistribution, eval_cdf))
        .def("eval_cdf", vectorize(&IrregularContinuousDistribution::eval_cdf<Wavelength>),
             "x"_a, "active"_a = true, D(IrregularContinuousDistribution, eval_cdf))
        .def("eval_cdf_normalized", vectorize(&IrregularContinuousDistribution::eval_cdf_normalized<Float>),
             "x"_a, "active"_a = true, D(IrregularContinuousDistribution, eval_cdf_normalized))
        .def("eval_cdf_normalized", vectorize(&IrregularContinuousDistribution::eval_cdf_normalized<Wavelength>),
             "x"_a, "active"_a = true, D(IrregularContinuousDistribution, eval_cdf_normalized))
        .def_method(IrregularContinuousDistribution, update)
        .def_method(IrregularContinuousDistribution, integral)
        .def_method(IrregularContinuousDistribution, normalization)
        .def("sample", vectorize(&IrregularContinuousDistribution::sample<Float>),
            "value"_a, "active"_a = true, D(IrregularContinuousDistribution, sample))
        .def("sample", vectorize(&IrregularContinuousDistribution::sample<Wavelength>),
            "value"_a, "active"_a = true, D(IrregularContinuousDistribution, sample))
        .def("sample_pdf", vectorize(&IrregularContinuousDistribution::sample_pdf<Float>),
            "value"_a, "active"_a = true, D(IrregularContinuousDistribution, sample_pdf))
        .def("sample_pdf", vectorize(&IrregularContinuousDistribution::sample_pdf<Wavelength>),
            "value"_a, "active"_a = true, D(IrregularContinuousDistribution, sample_pdf))
        .def_repr(IrregularContinuousDistribution);
}
