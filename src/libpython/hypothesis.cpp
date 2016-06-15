#include <hypothesis.h>
#include "python.h"

MTS_PY_EXPORT(hypothesis) {
    auto m2 = m.def_submodule("hypothesis", "A collection of quantile and quadrature routines"
                                            "for Z, Chi^2, and Student's T hypothesis tests.");

    // def(#Function, &Class::Function, DM(Class, Function), ##__VA_ARGS__)
    m2.def("stdnormal_cdf", &hypothesis::stdnormal_cdf, "Cumulative distribution function of "
                                                        "the standard normal distribution")
      .def("chi2_cdf", &hypothesis::chi2_cdf, "Cumulative distribution function of "
                                              "the Chi^2 distribution")
      .def("students_t_cdf", &hypothesis::students_t_cdf, "Cumulative distribution function of "
                                                          "Student's T distribution")
      .def("adaptiveSimpson", &hypothesis::adaptiveSimpson, "Adaptive Simpson integration over "
                                                            "a 1D interval")
      .def("adaptiveSimpson2D", &hypothesis::adaptiveSimpson2D,
           "Adaptive Simpson integration over a 2D rectangle",
           py::arg("f"), py::arg("x0"), py::arg("y0"), py::arg("x1"), py::arg("y1"),
           py::arg("eps") = 1e-6, py::arg("depth") = 6)

      .def("chi2_test", [](int nCells, const std::vector<double> obsFrequencies,
                           const std::vector<double> expFrequencies,
                           int sampleCount, double minExpFrequency,
                           double significanceLevel, int numTests = 1) {
        return hypothesis::chi2_test(nCells, obsFrequencies.data(), expFrequencies.data(),
                                     sampleCount, minExpFrequency, significanceLevel, numTests);
      }, "Peform a Chi^2 test based on the given frequency tables")

      .def("students_t_test", &hypothesis::students_t_test, "Peform a two-sided t-test "
                                                            "based on the given mean, "
                                                            "variance and reference value");
}
