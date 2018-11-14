#include <mitsuba/core/math.h>
#include <enoki/morton.h>
#include <enoki/special.h>
#include <enoki/color.h>
#include <mitsuba/python/python.h>
#include <bitset>

MTS_PY_EXPORT(math) {
    MTS_PY_IMPORT_MODULE(math, "mitsuba.core.math");

    math.def("comp_ellint_1", &enoki::comp_ellint_1<double>, "k"_a);
    math.def("comp_ellint_2", &enoki::comp_ellint_2<double>, "k"_a);
    math.def("comp_ellint_3", &enoki::comp_ellint_3<double, double>, "k"_a, "nu"_a);
    math.def("ellint_1",      &enoki::ellint_1<double, double>, "phi"_a, "k"_a);
    math.def("ellint_2",      &enoki::ellint_2<double, double>, "phi"_a, "k"_a);
    math.def("ellint_3",      &enoki::ellint_3<double, double, double>, "phi"_a, "k"_a, "nu"_a);

    math.def("i0e", enoki::i0e<Float>, "x"_a);

    math.def("legendre_p",
             py::overload_cast<int, Float>(math::legendre_p<Float>), "l"_a,
             "x"_a, D(math, legendre_p));

    math.def("legendre_p",
             vectorize_wrapper(py::overload_cast<int, FloatP>(math::legendre_p<FloatP>)),
             "l"_a, "x"_a);

    math.def("legendre_p",
             py::overload_cast<int, int, Float>(math::legendre_p<Float>), "l"_a,
             "m"_a, "x"_a, D(math, legendre_p));

    math.def("legendre_p",
             vectorize_wrapper(py::overload_cast<int, int, FloatP>(math::legendre_p<FloatP>)),
             "l"_a, "m"_a, "x"_a);

    math.def("legendre_pd", math::legendre_pd<Float>,
             "l"_a, "x"_a, D(math, legendre_pd));

    math.def("legendre_pd", vectorize_wrapper(math::legendre_pd<FloatP>),
            "l"_a, "x"_a);

    math.def("legendre_pd_diff",
             math::legendre_pd_diff<Float>,
             "l"_a, "x"_a, D(math, legendre_pd_diff));

    math.def("legendre_pd_diff",
             vectorize_wrapper(math::legendre_pd_diff<FloatP>),
             "l"_a, "x"_a);

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
    math.def("log2i", [](uint64_t v) { return enoki::log2i<uint64_t>(v); } );
    math.def("is_power_of_two", &math::is_power_of_two<uint64_t>, D(math, is_power_of_two));
    math.def("round_to_power_of_two", &math::round_to_power_of_two<uint64_t>, D(math, round_to_power_of_two));

    math.def("linear_to_srgb",
             vectorize_wrapper([](double &c) { return linear_to_srgb(c); }),
             "Applies the sRGB gamma curve to the given argument.");
    math.def("linear_to_srgb", vectorize_wrapper([](const Float64P &c) {
                 return linear_to_srgb(c);
             }));
    math.def("srgb_to_linear",
             vectorize_wrapper([](double &c) { return srgb_to_linear(c); }),
             "Applies the inverse sRGB gamma curve to the given argument.");
    math.def("srgb_to_linear", vectorize_wrapper([](const Float64P &c) {
                 return srgb_to_linear(c);
             }));

    math.def("chi2", [](py::array_t<double> obs, py::array_t<double> exp, double thresh) {
        if (obs.ndim() != 1 || exp.ndim() != 1 || exp.shape(0) != obs.shape(0))
            throw std::runtime_error("Unsupported input dimensions");
        return math::chi2(obs.data(), exp.data(), thresh, obs.shape(0));
    }, D(math, chi2));

    math.def("solve_quadratic", &math::solve_quadratic<float>,
             D(math, solve_quadratic), "a"_a, "b"_a, "c"_a);

    math.def("solve_quadratic", vectorize_wrapper(&math::solve_quadratic<FloatP>),
             D(math, solve_quadratic), "a"_a, "b"_a, "c"_a);

    math.def("find_interval", [](const py::array_t<Float> &arr, Float x) {
        if (arr.ndim() != 1)
            throw std::runtime_error("'arr' must be a one-dimensional array!");
        return math::find_interval(arr.shape(0),
            [&](size_t idx) {
                return arr.at(idx) <= x;
            }
        );
    });

    math.def("morton_decode2", &enoki::morton_decode<Array<uint32_t, 2>>, "m"_a);
    math.def("morton_decode2", vectorize_wrapper(enoki::morton_decode<Array<UInt32P, 2>>), "m"_a);
    math.def("morton_decode3", &enoki::morton_decode<Array<uint32_t, 3>>, "m"_a);
    math.def("morton_decode3", vectorize_wrapper(enoki::morton_decode<Array<UInt32P, 3>>), "m"_a);

    math.def("morton_encode2", &enoki::morton_encode<Array<uint32_t, 2>>, "v"_a);
    math.def("morton_encode2", vectorize_wrapper(enoki::morton_encode<Array<UInt32P, 2>>), "v"_a);
    math.def("morton_encode3", &enoki::morton_encode<Array<uint32_t, 3>>, "v"_a);
    math.def("morton_encode3", vectorize_wrapper(enoki::morton_encode<Array<UInt32P, 3>>), "v"_a);
}
