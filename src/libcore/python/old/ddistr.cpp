#include <mitsuba/core/ddistr.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(DiscreteDistribution) {
    MTS_IMPORT_CORE_TYPES()
    MTS_PY_CHECK_ALIAS(DiscreteDistribution, m) {
        MTS_PY_STRUCT(DiscreteDistribution)
            .def(py::init<size_t>(), py::arg("n_entries") = 0,
                D(DiscreteDistribution, DiscreteDistribution))
            .def(py::init([](py::array_t<ScalarFloat> data) {
                if (data.ndim() != 1)
                    throw std::domain_error("array has incorrect dimension");
                return DiscreteDistribution(
                    (size_t) data.shape(0),
                    data.data()
                );
            }))
            .def_method(DiscreteDistribution, clear)
            .def_method(DiscreteDistribution, reserve)
            .def_method(DiscreteDistribution, append)
            .def_method(DiscreteDistribution, size)
            .def("eval",
                vectorize<Float>(&DiscreteDistribution::template eval<UInt32>),
                "index"_a , "active"_a = true, D(DiscreteDistribution, eval))
            .def_method(DiscreteDistribution, normalized)
            .def_method(DiscreteDistribution, sum)
            .def_method(DiscreteDistribution, normalization)
            .def_method(DiscreteDistribution, cdf)
            .def_method(DiscreteDistribution, normalize)
            .def("sample",
                vectorize<Float>(&DiscreteDistribution::template sample<Float>),
                "sample_value"_a, "active"_a = true, D(DiscreteDistribution, sample))
            .def("sample_pdf",
                vectorize<Float>(&DiscreteDistribution::template sample_pdf<Float>),
                "sample_value"_a, "active"_a = true, D(DiscreteDistribution, sample_pdf))
            .def("sample_reuse",
                vectorize<Float>(&DiscreteDistribution::template sample_reuse<Float>),
                "sample_value"_a, "active"_a = true, D(DiscreteDistribution, sample_reuse))
            .def("sample_reuse_pdf",
                vectorize<Float>(&DiscreteDistribution::template sample_reuse_pdf<Float>),
                "sample_value"_a, "active"_a = true, D(DiscreteDistribution, sample_reuse_pdf))
            .def_repr(DiscreteDistribution);
    }
}
