#include <mitsuba/core/math.h>
#include "python.h"

MTS_PY_EXPORT(math) {
    py::module math = m.def_submodule("math", "Mathematical routines, special functions, etc.");

    math.def("signum",          (double(*)(double))                           math::signum, DM(math, signum));
    math.def("safe_acos",       (double(*)(double))                           math::safe_acos, DM(math, safe_acos));
    math.def("safe_asin",       (double(*)(double))                           math::safe_asin, DM(math, safe_asin));
    math.def("safe_sqrt",       (double(*)(double))                           math::safe_sqrt, DM(math, safe_sqrt));
    math.def("erf",             (double(*)(double))                           math::erf, DM(math, erf));
    math.def("erfinv",          (double(*)(double))                           math::erfinv, DM(math, erfinv));
    math.def("normal_quantile", (double(*)(double))                           math::normal_quantile, DM(math, normal_quantile));
    math.def("normal_cdf",      (double(*)(double))                           math::normal_cdf, DM(math, normal_cdf));
    math.def("comp_ellint_1",   (double(*)(double))                           math::comp_ellint_1, DM(math, comp_ellint_1));
    math.def("comp_ellint_2",   (double(*)(double))                           math::comp_ellint_2, DM(math, comp_ellint_2));
    math.def("comp_ellint_3",   (double(*)(double, double))                   math::comp_ellint_3, DM(math, comp_ellint_3));
    math.def("ellint_1",        (double(*)(double, double))                   math::ellint_1, DM(math, ellint_1));
    math.def("ellint_2",        (double(*)(double, double))                   math::ellint_2, DM(math, ellint_2));
    math.def("ellint_3",        (double(*)(double, double, double))           math::ellint_3, DM(math, ellint_3));
    math.def("i0e",             (double(*)(double))                           math::i0e, DM(math, i0e));
    math.def("legendre_p",      (double(*)(int l, double))                    math::legendre_p, DM(math, legendre_p));
    math.def("legendre_p",      (double(*)(int, int, double))                 math::legendre_p, DM(math, legendre_p, 2));
    math.def("legendre_pd",     (std::pair<double, double>(*)(int l, double)) math::legendre_pd, DM(math, legendre_pd));

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
    math.attr("Infinity")        = py::cast(math::Infinity);
    math.attr("MaxFloat")        = py::cast(math::MaxFloat);
    math.attr("MachineEpsilon")  = py::cast(math::MachineEpsilon);

    math.def("findInterval", [](size_t size, const std::function<bool(size_t)> &pred) {
        return math::findInterval(size, pred);
    }, DM(math, findInterval));
}
