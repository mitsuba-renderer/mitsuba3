#include <mitsuba/core/random.h>
#include <mitsuba/python/python.h>

#define DP(...) DOC(enoki, PCG32, ##__VA_ARGS__)

MTS_PY_EXPORT(random) {
    using PCG32 = mitsuba::PCG32;
    py::class_<PCG32> PCG32_(m, "PCG32", DP());

    PCG32_
        .def(py::init<uint64_t, uint64_t>(),
             "initstate"_a = PCG32_DEFAULT_STATE,
             "initseq"_a = index_sequence<uint64_t>() + PCG32_DEFAULT_STREAM,
             DP(PCG32))
        .def(py::init<const PCG32 &>(), "Copy constructor")
        .def("seed", &PCG32::seed, "initstate"_a, "initseq"_a = 1u, DP(seed))
        .def("next_uint32", py::overload_cast<>(&PCG32::next_uint32), DP(next_uint32))
        .def("next_uint32_bounded", py::overload_cast<uint32_t>(&PCG32::next_uint32_bounded),
             "bound"_a, DP(next_uint32_bounded))
        .def("next_uint64", py::overload_cast<>(&PCG32::next_uint64), DP(next_uint64))
        .def("next_uint64_bounded", py::overload_cast<uint64_t>(&PCG32::next_uint64_bounded),
             "bound"_a, DP(next_uint64_bounded))
        .def("next_float32", py::overload_cast<>(&PCG32::next_float32), DP(next_float32))
        .def("next_float32", [](PCG32 &rng, size_t n) {
            py::array_t<float> result(n);
            for (size_t i = 0; i < n; ++i)
                result.mutable_data()[i] = rng.next_float32();
            return result;
        }, "n"_a)
        .def("next_float32", [](PCG32 &rng, size_t m, size_t n) {
            py::array_t<float> result({m, n});
            for (size_t i = 0; i < n*m; ++i)
                result.mutable_data()[i] = rng.next_float32();
            return result;
        }, "m"_a, "n"_a)
        .def("next_float64", py::overload_cast<>(&PCG32::next_float64), DP(next_float64))
        .def("next_float64", [](PCG32 &rng, size_t n) {
            py::array_t<float> result(n);
            for (size_t i = 0; i < n; ++i)
                result.mutable_data()[i] = rng.next_float64();
            return result;
        }, "n"_a)
        .def("next_float64", [](PCG32 &rng, size_t m, size_t n) {
            py::array_t<float> result({m, n});
            for (size_t i = 0; i < n*m; ++i)
                result.mutable_data()[i] = rng.next_float64();
            return result;
        }, "m"_a, "n"_a)
        .def("next_float", [p = py::handle(PCG32_)](py::args args, py::kwargs kwargs) -> py::object {
            #if defined(SINGLE_PRECISION)
                return p.attr("next_float32")(*args, **kwargs);
            #else
                return p.attr("next_float64")(*args, **kwargs);
            #endif
        })
        .def("advance", &PCG32::advance, "delta"_a, DP(advance))
        .def("shuffle", [](PCG32 &p, py::list l) {
            auto vec = l.cast<std::vector<py::object>>();
            p.shuffle(vec.begin(), vec.end());
            for (size_t i = 0; i < vec.size(); ++i)
                l[i] = vec[i];
        }, DP(shuffle))
        .def(py::self == py::self, DP(operator, eq))
        .def(py::self != py::self, DP(operator, ne))
        .def(py::self - py::self, DP(operator, sub))
        .def("__repr__", [](const PCG32 &p) {
            std::ostringstream oss;
            oss << "PCG32[state=0x" << std::hex << p.state << ", inc=0x" << std::hex << p.inc << "]";
            return oss.str();
        });

    m.def("sample_tea_float32", sample_tea_float32<uint32_t>,
          "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_float32));

    m.def("sample_tea_float32",
          [](const UInt32X &v0, const UInt32X &v1, int rounds) {
              return vectorize_safe(sample_tea_float32<UInt32P>, v0, v1, rounds);
          }, "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_float32));

    m.def("sample_tea_float64", sample_tea_float64<uint32_t>,
          "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_float64));

    m.def("sample_tea_float64",
          [](const UInt32X &v0, const UInt32X &v1, int rounds) {
              return vectorize_safe(sample_tea_float64<UInt32P>, v0, v1, rounds);
          }, "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_float64));

    m.attr("sample_tea_float") = m.attr(
        sizeof(Float) == sizeof(float) ? "sample_tea_float32" : "sample_tea_float64");
}
