#include <mitsuba/core/ddistr.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(DiscreteDistribution) {
    MTS_PY_STRUCT(DiscreteDistribution)
        .def(py::init<size_t>(), py::arg("n_entries") = 0,
             D(DiscreteDistribution, DiscreteDistribution))
        .def(py::init([](py::array_t<Float> data) {
            if (data.ndim() != 1)
                throw std::domain_error("array has incorrect dimension");
            return DiscreteDistribution(
                (size_t) data.shape(0),
                data.data()
            );
        }))
        .mdef(DiscreteDistribution, clear)
        .mdef(DiscreteDistribution, reserve)
        .mdef(DiscreteDistribution, append)
        .mdef(DiscreteDistribution, size)

        .def("__getitem__", [](const DiscreteDistribution &d, const size_t &i) {
            return d[i];
        }, D(DiscreteDistribution, operator_array))
        .def("__getitem__", vectorize_wrapper(
            [](const DiscreteDistribution &d, const SizeP &indices) {
                return d[indices];
            }
        ), D(DiscreteDistribution, operator_array))

        .mdef(DiscreteDistribution, normalized)
        .mdef(DiscreteDistribution, sum)
        .mdef(DiscreteDistribution, normalization)
        .mdef(DiscreteDistribution, cdf)
        .mdef(DiscreteDistribution, normalize)

        // ---------------------------------------------------------------------

        .def("sample", [](const DiscreteDistribution &d, Float arg) { return d.sample(arg); },
             "sample_value"_a, D(DiscreteDistribution, sample))
        .def("sample", vectorize_wrapper(&DiscreteDistribution::sample<FloatP>),
             "sample_value"_a, "active"_a = true)

        // ---------------------------------------------------------------------

        .def("sample_pdf", [](const DiscreteDistribution &d, Float arg) { return d.sample_pdf(arg); },
             "sample_value"_a, D(DiscreteDistribution, sample_pdf))
        .def("sample_pdf", vectorize_wrapper(&DiscreteDistribution::sample_pdf<FloatP>),
             "sample_value"_a, "active"_a = true)

        // ---------------------------------------------------------------------

        .def("sample_reuse", [](const DiscreteDistribution &d, Float arg) { return d.sample_reuse(arg); },
             "sample_value"_a, D(DiscreteDistribution, sample_reuse))
        .def("sample_reuse", vectorize_wrapper(&DiscreteDistribution::sample_reuse<FloatP>),
             "sample_value"_a, "active"_a = true)

        // ---------------------------------------------------------------------

        .def("sample_reuse_pdf", [](const DiscreteDistribution &d, Float arg) { return d.sample_reuse_pdf(arg); },
             "sample_value"_a, D(DiscreteDistribution, sample_reuse_pdf))
        .def("sample_reuse_pdf", vectorize_wrapper(&DiscreteDistribution::sample_reuse_pdf<FloatP>),
             "sample_value"_a, "active"_a = true)

        // ---------------------------------------------------------------------

        .def("__repr__", [](const DiscreteDistribution &d) {
            std::ostringstream oss;
            oss << d;
            return oss.str();
        });
}
