#include <mitsuba/core/random.h>
#include <mitsuba/python/python.h>

#define DE(...) DOC(enoki, ##__VA_ARGS__)

MTS_PY_EXPORT(random) {
    using PCG32 = mitsuba::PCG32;
    py::class_<PCG32> PCG32_(m, "PCG32", DE(PCG32));

    PCG32_
        .def(py::init<uint64_t, uint64_t>(),
             "initstate"_a = PCG32_DEFAULT_STATE,
             "initseq"_a = PCG32_DEFAULT_STREAM, DE(PCG32, PCG32))
        .def(py::init<const PCG32 &>(), "Copy constructor")
        .def("seed", &PCG32::seed, "initstate"_a, "initseq"_a = 1u, DE(PCG32, seed))
        .def("next_uint32", (uint32_t (PCG32::*)()) &PCG32::next_uint32, DE(PCG32, next_uint32))
        .def("next_uint32", [](PCG32 &rng, const std::vector<size_t> &shape) {
            py::array_t<uint32_t> result(shape);
            for (py::ssize_t i = 0; i < result.size(); ++i)
                result.mutable_data()[i] = rng.next_uint32();
            return result;
        }, "shape"_a)
        .def("next_uint32_bounded", [](PCG32 &rng, uint32_t bound) {
                return rng.next_uint32_bounded(bound);
            }, "bound"_a, DE(PCG32, next_uint32_bounded))
        .def("next_uint32_bounded", [](PCG32 &rng, uint32_t bound, const std::vector<size_t> &shape) {
            py::array_t<uint32_t> result(shape);
            for (py::ssize_t i = 0; i < result.size(); ++i)
                result.mutable_data()[i] = rng.next_uint32_bounded(bound);
            return result;
        }, "bound"_a, "shape"_a)
        .def("next_uint64", (uint64_t (PCG32::*)()) &PCG32::next_uint64, DE(PCG32, next_uint64))
        .def("next_uint64", [](PCG32 &rng, const std::vector<size_t> &shape) {
            py::array_t<uint64_t> result(shape);
            for (py::ssize_t i = 0; i < result.size(); ++i)
                result.mutable_data()[i] = rng.next_uint64();
            return result;
        }, "shape"_a)
        .def("next_uint64_bounded", [](PCG32 &rng, uint64_t bound) {
                return rng.next_uint64_bounded(bound);
            }, "bound"_a, DE(PCG32, next_uint64_bounded))
        .def("next_uint64_bounded", [](PCG32 &rng, uint64_t bound, const std::vector<size_t> &shape) {
            py::array_t<uint64_t> result(shape);
            for (py::ssize_t i = 0; i < result.size(); ++i)
                result.mutable_data()[i] = rng.next_uint64_bounded(bound);
            return result;
        }, "bound"_a, "shape"_a)
        .def("next_float32", (float (PCG32::*)()) &PCG32::next_float32, DE(PCG32, next_float32))
        .def("next_float32", [](PCG32 &rng, const std::vector<size_t> &shape) {
            py::array_t<float> result(shape);
            for (py::ssize_t i = 0; i < result.size(); ++i)
                result.mutable_data()[i] = rng.next_float32();
            return result;
        }, "shape"_a)
        .def("next_float64", (double (PCG32::*)()) &PCG32::next_float64, DE(PCG32, next_float64))
        .def("next_float64", [](PCG32 &rng, const std::vector<size_t> &shape) {
            py::array_t<double> result(shape);
            for (py::ssize_t i = 0; i < result.size(); ++i)
                result.mutable_data()[i] = rng.next_float64();
            return result;
        }, "shape"_a)
        .def("next_float", [p = py::handle(PCG32_)](py::args args, py::kwargs kwargs) -> py::object {
            #if defined(SINGLE_PRECISION)
                return p.attr("next_float32")(*args, **kwargs);
            #else
                return p.attr("next_float64")(*args, **kwargs);
            #endif
        })
        .def("advance", &PCG32::advance, "delta"_a, DE(PCG32, advance))
        .def("shuffle", [](PCG32 &p, py::list l) {
            auto vec = l.cast<std::vector<py::object>>();
            p.shuffle(vec.begin(), vec.end());
            for (size_t i = 0; i < vec.size(); ++i)
                l[i] = vec[i];
        }, DE(PCG32, shuffle))
        .def(py::self == py::self, DE(PCG32, operator, eq))
        .def(py::self != py::self, DE(PCG32, operator, ne))
        .def(py::self - py::self, DE(PCG32, operator, sub))
        .def_repr(PCG32);

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
