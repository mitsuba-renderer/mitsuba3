#include <mitsuba/core/math.h>
#include <enoki/morton.h>
#include <enoki/special.h>
#include <enoki/color.h>
#include <mitsuba/python/python.h>
#include <bitset>

MTS_PY_EXPORT_FLOAT_VARIANTS(math) {
    MTS_IMPORT_CORE_TYPES()

    py::module math = m.def_submodule("math");

    math.def("comp_ellint_1", &enoki::comp_ellint_1<Float>, "k"_a);
    math.def("comp_ellint_2", &enoki::comp_ellint_2<Float>, "k"_a);
    math.def("comp_ellint_3", &enoki::comp_ellint_3<Float, Float>, "k"_a, "nu"_a);
    math.def("ellint_1",      &enoki::ellint_1<Float, Float>, "phi"_a, "k"_a);
    math.def("ellint_2",      &enoki::ellint_2<Float, Float>, "phi"_a, "k"_a);
    math.def("ellint_3",      &enoki::ellint_3<Float, Float, Float>, "phi"_a, "k"_a, "nu"_a);

    math.attr("E")               = py::cast(math::E<Float>);
    math.attr("Pi")              = py::cast(math::Pi<Float>);
    math.attr("InvPi")           = py::cast(math::InvPi<Float>);
    math.attr("InvTwoPi")        = py::cast(math::InvTwoPi<Float>);
    math.attr("InvFourPi")       = py::cast(math::InvFourPi<Float>);
    math.attr("SqrtPi")          = py::cast(math::SqrtPi<Float>);
    math.attr("InvSqrtPi")       = py::cast(math::InvSqrtPi<Float>);
    math.attr("SqrtTwo")         = py::cast(math::SqrtTwo<Float>);
    math.attr("InvSqrtTwo")      = py::cast(math::InvSqrtTwo<Float>);
    math.attr("SqrtTwoPi")       = py::cast(math::SqrtTwoPi<Float>);
    math.attr("InvSqrtTwoPi")    = py::cast(math::InvSqrtTwoPi<Float>);
    math.attr("OneMinusEpsilon") = py::cast(math::OneMinusEpsilon<Float>);
    math.attr("RecipOverflow")   = py::cast(math::RecipOverflow<Float>);
    math.attr("Epsilon")         = py::cast(math::Epsilon<Float>);
    math.attr("Infinity")        = py::cast(math::Infinity<Float>);
    math.attr("Min")             = py::cast(math::Min<Float>);
    math.attr("Max")             = py::cast(math::Max<Float>);

    math.def("i0e", enoki::i0e<Float>, "x"_a);

    math.def("legendre_p",
             vectorize<Float>(py::overload_cast<int, Float>(math::legendre_p<Float>)),
             "l"_a, "x"_a, D(math, legendre_p));
    math.def("legendre_p",
             vectorize<Float>(py::overload_cast<int, int, Float>(math::legendre_p<Float>)),
             "l"_a, "m"_a, "x"_a, D(math, legendre_p));
    math.def("legendre_pd",
             vectorize<Float>(math::legendre_pd<Float>),
             "l"_a, "x"_a, D(math, legendre_pd));
    math.def("legendre_pd_diff",
             vectorize<Float>(math::legendre_pd_diff<Float>),
             "l"_a, "x"_a, D(math, legendre_pd_diff));

    math.def("ulpdiff", &math::ulpdiff<Float>, D(math, ulpdiff));
    math.def("log2i", [](UInt64 v) { return enoki::log2i<UInt64>(v); } );
    math.def("is_power_of_two", &math::is_power_of_two<UInt64>, D(math, is_power_of_two));
    math.def("round_to_power_of_two", &math::round_to_power_of_two<UInt64>,
             D(math, round_to_power_of_two));
    math.def("linear_to_srgb",
             vectorize<Float>([](Float &c) { return linear_to_srgb(c); }),
             "Applies the sRGB gamma curve to the given argument.");
    math.def("srgb_to_linear",
             vectorize<Float>([](Float &c) { return srgb_to_linear(c); }),
             "Applies the inverse sRGB gamma curve to the given argument.");

    math.def("find_interval", [](size_t start, size_t end, const std::function<bool(size_t)> &pred) {
        return math::find_interval(start, end, pred);
    }, D(math, find_interval));

    math.def("find_interval", [](const py::array_t<Float> &arr, Float x) {
        if (arr.ndim() != 1)
            throw std::runtime_error("'arr' must be a one-dimensional array!");
        return math::find_interval(arr.shape(0),
            [&](size_t idx) {
                return arr.at(idx) <= x;
            }
        );
    });

    math.def("chi2", [](py::array_t<double> obs, py::array_t<double> exp, double thresh) {
        if (obs.ndim() != 1 || exp.ndim() != 1 || exp.shape(0) != obs.shape(0))
            throw std::runtime_error("Unsupported input dimensions");
        return math::chi2(obs.data(), exp.data(), thresh, obs.shape(0));
    }, D(math, chi2));

    math.def("solve_quadratic",
             vectorize<Float>(&math::solve_quadratic<Float>),
             "a"_a, "b"_a, "c"_a, D(math, solve_quadratic));

    math.def("morton_decode2", vectorize<Float>(&enoki::morton_decode<Array<UInt32, 2>>), "m"_a);
    math.def("morton_decode3", vectorize<Float>(&enoki::morton_decode<Array<UInt32, 3>>), "m"_a);
    math.def("morton_encode2", vectorize<Float>(&enoki::morton_encode<Array<UInt32, 2>>), "v"_a);
    math.def("morton_encode3", vectorize<Float>(&enoki::morton_encode<Array<UInt32, 3>>), "v"_a);
}
