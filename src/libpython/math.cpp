#include <mitsuba/core/math.h>
#include "python.h"

MTS_PY_EXPORT(math) {
    MTS_PY_IMPORT_MODULE(math, "mitsuba.core.math");

    math.def("normal_quantile", (double(*)(double))                           math::normal_quantile, D(math, normal_quantile));
    math.def("normal_cdf",      (double(*)(double))                           math::normal_cdf, D(math, normal_cdf));
    math.def("comp_ellint_1",   (double(*)(double))                           math::comp_ellint_1, D(math, comp_ellint_1));
    math.def("comp_ellint_2",   (double(*)(double))                           math::comp_ellint_2, D(math, comp_ellint_2));
    math.def("comp_ellint_3",   (double(*)(double, double))                   math::comp_ellint_3, D(math, comp_ellint_3));
    math.def("ellint_1",        (double(*)(double, double))                   math::ellint_1, D(math, ellint_1));
    math.def("ellint_2",        (double(*)(double, double))                   math::ellint_2, D(math, ellint_2));
    math.def("ellint_3",        (double(*)(double, double, double))           math::ellint_3, D(math, ellint_3));
    math.def("i0e",             (double(*)(double))                           math::i0e, D(math, i0e));
    math.def("legendre_p",      (double(*)(int l, double))                    math::legendre_p, D(math, legendre_p));
    math.def("legendre_p",      (double(*)(int, int, double))                 math::legendre_p, D(math, legendre_p, 2));
    math.def("legendre_pd",     (std::pair<double, double>(*)(int l, double)) math::legendre_pd, D(math, legendre_pd));

    math.attr("E")               = py::cast(math::E_d);
    math.attr("Pi")              = py::cast(math::Pi_d);
    math.attr("InvPi")           = py::cast(math::InvPi_d);
    math.attr("InvTwoPi")        = py::cast(math::InvTwoPi_d);
    math.attr("InvFourPi")       = py::cast(math::InvFourPi_d);
    math.attr("SqrtPi")          = py::cast(math::SqrtPi_d);
    math.attr("InvSqrtPi")       = py::cast(math::InvSqrtPi_d);
    math.attr("SqrtTwo")         = py::cast(math::SqrtTwo_d);
    math.attr("InvSqrtTwo")      = py::cast(math::InvSqrtTwo_d);
    math.attr("SqrtTwoPi")       = py::cast(math::SqrtTwoPi_d);
    math.attr("InvSqrtTwoPi")    = py::cast(math::InvSqrtTwoPi_d);
    math.attr("OneMinusEpsilon") = py::cast(math::OneMinusEpsilon);
    math.attr("RecipOverflow")   = py::cast(math::RecipOverflow);
    math.attr("Epsilon")         = py::cast(math::Epsilon);
    math.attr("Infinity")        = py::cast(math::Infinity);
    math.attr("MaxFloat")        = py::cast(math::MaxFloat);
    math.attr("MachineEpsilon")  = py::cast(math::MachineEpsilon);

    math.def("find_interval", [](size_t start, size_t end, const std::function<bool(size_t)> &pred) {
        return math::find_interval(start, end, pred);
    }, D(math, find_interval));

    math.def("ulpdiff", &math::ulpdiff<Float>, D(math, ulpdiff));
    math.def("log2i", &enoki::log2i<uint64_t>);
    math.def("is_power_of_two", &math::is_power_of_two<uint64_t>, D(math, is_power_of_two));
    math.def("round_to_power_of_two", &math::round_to_power_of_two<uint64_t>, D(math, round_to_power_of_two));
    math.def("gamma", &math::gamma<double>, D(math, gamma));
    math.def("inv_gamma", &math::inv_gamma<double>, D(math, inv_gamma));

    math.def("chi2", [](py::array_t<double> obs, py::array_t<double> exp, double thresh) {
        if (obs.ndim() != 1 || exp.ndim() != 1 || exp.shape(0) != obs.shape(0))
            throw std::runtime_error("Unsupported input dimensions");
        return math::chi2(obs.data(), exp.data(), thresh, obs.shape(0));
    }, D(math, chi2));
}
