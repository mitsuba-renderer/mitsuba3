#include <mitsuba/core/ddistribution.h>
#include "python.h"

MTS_PY_EXPORT(DiscreteDistribution) {
    MTS_PY_STRUCT(DiscreteDistribution)
        .def(py::init<size_t>(), py::arg("n_entries") = 0,
             D(DiscreteDistribution, DiscreteDistribution))
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

        .def("sample", &DiscreteDistribution::sample<Float>,
             "sample_value"_a, D(DiscreteDistribution, sample))
        .def("sample", vectorize_wrapper(&DiscreteDistribution::sample<FloatP>),
             "sample_value"_a)

        .def("sample_pdf", &DiscreteDistribution::sample_pdf<Float>,
             "sample_value"_a, D(DiscreteDistribution, sample_pdf))
        .def("sample_pdf", vectorize_wrapper(&DiscreteDistribution::sample_pdf<FloatP>),
             "sample_value"_a)

        // ---------------------------------------------------------------------

        .def("sample_reuse", [](const DiscreteDistribution &d, Float s) {
            const auto res = d.sample_reuse(&s);
            return std::make_pair(res, s);
        }, "sample_value"_a, D(DiscreteDistribution, sample_reuse))

        .def("sample_reuse", vectorize_wrapper(
            [](const DiscreteDistribution &d, FloatP s) {
                const auto res = d.sample_reuse(&s);
                return std::make_pair(res, s);
            }
        ), "sample_value"_a)

        .def("sample_reuse_pdf", [](const DiscreteDistribution &d, Float s) {
            const auto res = d.sample_reuse_pdf(&s);
            return std::make_tuple(res.first, s, res.second);
        }, "sample_value"_a, D(DiscreteDistribution, sample_reuse_pdf))

        .def("sample_reuse_pdf", vectorize_wrapper(
            [](const DiscreteDistribution &d, FloatP s) {
                const auto res = d.sample_reuse_pdf(&s);
                return std::make_tuple(res.first, s, res.second);
            }
        ), "sample_value"_a)

        // ---------------------------------------------------------------------

        .def("__repr__", [](const DiscreteDistribution &d) {
            std::ostringstream oss;
            oss << d;
            return oss.str();
        });
}
