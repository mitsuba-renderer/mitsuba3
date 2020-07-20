#include <mitsuba/core/math.h>
#include <enoki/morton.h>
#include <enoki/special.h>
#include <enoki/color.h>
#include <bitset>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(math) {
    MTS_PY_IMPORT_TYPES()

    m.attr("E")               = py::cast(math::E<Float>);
    m.attr("Pi")              = py::cast(math::Pi<Float>);
    m.attr("InvPi")           = py::cast(math::InvPi<Float>);
    m.attr("InvTwoPi")        = py::cast(math::InvTwoPi<Float>);
    m.attr("InvFourPi")       = py::cast(math::InvFourPi<Float>);
    m.attr("SqrtPi")          = py::cast(math::SqrtPi<Float>);
    m.attr("InvSqrtPi")       = py::cast(math::InvSqrtPi<Float>);
    m.attr("SqrtTwo")         = py::cast(math::SqrtTwo<Float>);
    m.attr("InvSqrtTwo")      = py::cast(math::InvSqrtTwo<Float>);
    m.attr("SqrtTwoPi")       = py::cast(math::SqrtTwoPi<Float>);
    m.attr("InvSqrtTwoPi")    = py::cast(math::InvSqrtTwoPi<Float>);
    m.attr("OneMinusEpsilon") = py::cast(math::OneMinusEpsilon<Float>);
    m.attr("RecipOverflow")   = py::cast(math::RecipOverflow<Float>);
    m.attr("Epsilon")         = py::cast(math::Epsilon<Float>);
    m.attr("Infinity")        = py::cast(math::Infinity<Float>);
    m.attr("Min")             = py::cast(math::Min<Float>);
    m.attr("Max")             = py::cast(math::Max<Float>);
    m.attr("RayEpsilon")      = py::cast(math::RayEpsilon<Float>);
    m.attr("ShadowEpsilon")   = py::cast(math::ShadowEpsilon<Float>);

    m.def("legendre_p",
          vectorize(py::overload_cast<int, Float>(math::legendre_p<Float>)),
          "l"_a, "x"_a, D(math, legendre_p));

    m.def("legendre_p",
          vectorize(py::overload_cast<int, int, Float>(math::legendre_p<Float>)),
          "l"_a, "m"_a, "x"_a, D(math, legendre_p));

    m.def("legendre_pd",
          vectorize(math::legendre_pd<Float>),
          "l"_a, "x"_a, D(math, legendre_pd));

    m.def("legendre_pd_diff",
          vectorize(math::legendre_pd_diff<Float>),
          "l"_a, "x"_a, D(math, legendre_pd_diff));

    m.def("ulpdiff", &math::ulpdiff<ScalarFloat>, D(math, ulpdiff));

    m.def("is_power_of_two", &math::is_power_of_two<ScalarUInt64>, D(math, is_power_of_two));

    m.def("round_to_power_of_two", &math::round_to_power_of_two<ScalarUInt64>,
          D(math, round_to_power_of_two));

    m.def("linear_to_srgb",
          vectorize([](Float &c) { return linear_to_srgb(c); }),
          "Applies the sRGB gamma curve to the given argument.");

    m.def("srgb_to_linear",
          vectorize([](Float &c) { return srgb_to_linear(c); }),
          "Applies the inverse sRGB gamma curve to the given argument.");

    m.def("chi2",
          [](const DynamicBuffer<double> &obs, const DynamicBuffer<double> &exp, double thresh) {
              if (exp.size() != obs.size())
                  throw std::runtime_error("Unsupported input dimensions");
              return math::chi2(obs.data(), exp.data(), thresh, obs.size());
          }, D(math, chi2));

    m.def("solve_quadratic",
          vectorize(&math::solve_quadratic<Float>),
          "a"_a, "b"_a, "c"_a, D(math, solve_quadratic));

    m.def("morton_decode2", vectorize(&enoki::morton_decode<Array<UInt32, 2>>), "m"_a);
    m.def("morton_decode3", vectorize(&enoki::morton_decode<Array<UInt32, 3>>), "m"_a);
    m.def("morton_encode2", vectorize(&enoki::morton_encode<Array<UInt32, 2>>), "v"_a);
    m.def("morton_encode3", vectorize(&enoki::morton_encode<Array<UInt32, 3>>), "v"_a);

    m.def("find_interval",
          [](uint32_t size, const std::function<Mask(const UInt32 &)> &pred) {
              return math::find_interval(size, pred);
          },
          "size"_a, "pred"_a, D(math, find_interval));
}
