#include <mitsuba/core/ddistr.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT_VARIANTS(DiscreteDistribution) {
    MTS_IMPORT_CORE_TYPES()
    using DiscreteDistribution  = DiscreteDistribution<Float>;
    using DiscreteDistributionP = mitsuba::DiscreteDistribution<FloatP>;
    using UInt32P = replace_scalar_t<FloatP, ScalarUInt32>;

    MTS_PY_CHECK_ALIAS(DiscreteDistribution, m) {
        MTS_PY_STRUCT(DiscreteDistribution)
            .def(py::init<size_t>(), py::arg("n_entries") = 0,
                D(DiscreteDistribution, DiscreteDistribution))
            .def(py::init([](py::array_t<float> data) {
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
                vectorize<Float>(&DiscreteDistributionP::template eval<UInt32P>),
                "index"_a , "active"_a = true, D(DiscreteDistribution, eval))
            .def_method(DiscreteDistribution, normalized)
            .def_method(DiscreteDistribution, sum)
            .def_method(DiscreteDistribution, normalization)
            .def_method(DiscreteDistribution, cdf)
            .def_method(DiscreteDistribution, normalize)
            .def("sample",
                vectorize<Float>(&DiscreteDistributionP::template sample<FloatP>),
                "sample_value"_a, "active"_a = true, D(DiscreteDistribution, sample))
            .def("sample_pdf",
                vectorize<Float>(&DiscreteDistributionP::template sample_pdf<FloatP>),
                "sample_value"_a, "active"_a = true, D(DiscreteDistribution, sample_pdf))
            .def("sample_reuse",
                vectorize<Float>(&DiscreteDistributionP::template sample_reuse<FloatP>),
                "sample_value"_a, "active"_a = true, D(DiscreteDistribution, sample_reuse))
            .def("sample_reuse_pdf",
                vectorize<Float>(&DiscreteDistributionP::template sample_reuse_pdf<FloatP>),
                "sample_value"_a, "active"_a = true, D(DiscreteDistribution, sample_reuse_pdf))
            .def_repr(DiscreteDistribution)
            ;
    }
}
