#include <mitsuba/core/distr.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(DiscreteDistribution) {
    MTS_PY_IMPORT_TYPES()

    using DiscreteDistribution = mitsuba::DiscreteDistribution<Float>;
    using FloatStorage = DynamicBuffer<Float>;

    MTS_PY_STRUCT(DiscreteDistribution)
        .def(py::init<FloatStorage>(), "pmf"_a,
             D(DiscreteDistribution, DiscreteDistribution))
        .def("__len__", &DiscreteDistribution::size)
        .def("empty", &DiscreteDistribution::empty, D(DiscreteDistribution, empty))
        .def_property("pmf",
             [](DiscreteDistribution &d) { return d.pmf(); },
             [](DiscreteDistribution &d, const FloatStorage &v) { d.pmf() = v; },
             D(DiscreteDistribution, pmf),
             py::return_value_policy::reference_internal)
        .def_property("cdf",
             [](DiscreteDistribution &d) { return d.cdf(); },
             [](DiscreteDistribution &d, const FloatStorage &v) { d.cdf() = v; },
             D(DiscreteDistribution, cdf),
             py::return_value_policy::reference_internal)
        .def("eval_pmf", vectorize(&DiscreteDistribution::eval_pmf),
             "index"_a, "active"_a = true, D(DiscreteDistribution, eval_pmf))
        .def("eval_pmf_normalized", vectorize(&DiscreteDistribution::eval_pmf_normalized),
             "index"_a, "active"_a = true, D(DiscreteDistribution, eval_pmf_normalized))
        .def("eval_cdf", vectorize(&DiscreteDistribution::eval_cdf),
             "index"_a, "active"_a = true, D(DiscreteDistribution, eval_cdf))
        .def("eval_cdf_normalized", vectorize(&DiscreteDistribution::eval_cdf_normalized),
             "index"_a, "active"_a = true, D(DiscreteDistribution, eval_cdf_normalized))
        .def_method(DiscreteDistribution, update)
        .def_method(DiscreteDistribution, sum)
        .def_method(DiscreteDistribution, normalization)
        .def("sample",
            vectorize(&DiscreteDistribution::sample),
            "value"_a, "active"_a = true, D(DiscreteDistribution, sample))
        .def("sample_pmf",
            vectorize(&DiscreteDistribution::sample_pmf),
            "value"_a, "active"_a = true, D(DiscreteDistribution, sample_pmf))
        .def("sample_reuse",
            vectorize(&DiscreteDistribution::sample_reuse),
            "value"_a, "active"_a = true, D(DiscreteDistribution, sample_reuse))
        .def("sample_reuse_pmf",
            vectorize(&DiscreteDistribution::sample_reuse_pmf),
            "value"_a, "active"_a = true, D(DiscreteDistribution, sample_reuse_pmf))
        .def_repr(DiscreteDistribution);
}
