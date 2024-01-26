#include <mitsuba/core/math.h>
#include <drjit/color.h>
#include <drjit/dynamic.h>
#include <drjit/morton.h>
#include <bitset>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(math) {
    MI_PY_IMPORT_TYPES()

    m.attr("RayEpsilon")      = py::cast(math::RayEpsilon<Float>);
    m.attr("ShadowEpsilon")   = py::cast(math::ShadowEpsilon<Float>);
    m.attr("ShapeEpsilon")    = py::cast(math::ShapeEpsilon<Float>);

    m.def("legendre_p",
          py::overload_cast<int, Float>(math::legendre_p<Float>),
          "l"_a, "x"_a, D(math, legendre_p));

    m.def("legendre_p",
          py::overload_cast<int, int, Float>(math::legendre_p<Float>),
          "l"_a, "m"_a, "x"_a, D(math, legendre_p));

    m.def("legendre_pd",
          math::legendre_pd<Float>,
          "l"_a, "x"_a, D(math, legendre_pd));

    m.def("legendre_pd_diff",
          math::legendre_pd_diff<Float>,
          "l"_a, "x"_a, D(math, legendre_pd_diff));

    m.def("ulpdiff", &math::ulpdiff<ScalarFloat>, D(math, ulpdiff));

    m.def("is_power_of_two", &math::is_power_of_two<ScalarUInt64>, D(math, is_power_of_two));

    m.def("round_to_power_of_two", &math::round_to_power_of_two<ScalarUInt64>,
          D(math, round_to_power_of_two));

    m.def("linear_to_srgb",
          [](Float &c) { return dr::linear_to_srgb(c); },
          "Applies the sRGB gamma curve to the given argument.");

    m.def("srgb_to_linear",
          [](Float &c) { return dr::srgb_to_linear(c); },
          "Applies the inverse sRGB gamma curve to the given argument.");

    m.def("chi2",
          [](const DynamicBuffer<double> &obs, const DynamicBuffer<double> &exp, double thresh) {
              if (exp.size() != obs.size())
                  throw std::runtime_error("Unsupported input dimensions");
              return math::chi2(obs.data(), exp.data(), thresh, obs.size());
          }, D(math, chi2));

    m.def("solve_quadratic",
          &math::solve_quadratic<Float>,
          "a"_a, "b"_a, "c"_a, D(math, solve_quadratic));

    m.def("morton_decode2", &dr::morton_decode<dr::Array<UInt32, 2>>, "m"_a);
    m.def("morton_decode3", &dr::morton_decode<dr::Array<UInt32, 3>>, "m"_a);
    m.def("morton_encode2", &dr::morton_encode<dr::Array<UInt32, 2>>, "v"_a);
    m.def("morton_encode3", &dr::morton_encode<dr::Array<UInt32, 3>>, "v"_a);

    m.def("find_interval",
          [](uint32_t size, const std::function<Mask(const UInt32 &)> &pred) {
              return math::find_interval<UInt32>(size, pred);
          },
          "size"_a, "pred"_a, D(math, find_interval));
}
