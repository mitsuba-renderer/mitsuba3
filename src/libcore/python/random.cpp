#include <mitsuba/core/random.h>
#include <mitsuba/python/python.h>

#define DE(...) DOC(enoki, ##__VA_ARGS__)

MTS_PY_EXPORT_VARIANTS(random) {
    MTS_IMPORT_CORE_TYPES()
    using PCG32 = mitsuba::PCG32<UInt32>;

    auto pcg32 = py::class_<PCG32>(m, "PCG32", D(PCG32))
        .def(py::init<UInt64, UInt64>(),
             "initstate"_a = PCG32_DEFAULT_STATE,
             "initseq"_a = PCG32_DEFAULT_STREAM, D(PCG32, PCG32))
        .def(py::init<const PCG32 &>(), "Copy constructor")
        .def("seed", &PCG32::seed, "initstate"_a, "initseq"_a = 1u, D(PCG32, seed))
        .def("next_uint32", (UInt32 (PCG32::*)()) &PCG32::next_uint32, D(PCG32, next_uint32))
        .def("next_uint32", [](PCG32 &rng, const std::vector<size_t> &shape) {
            py::array_t<UInt32> result(shape);
            for (py::ssize_t i = 0; i < result.size(); ++i)
                result.mutable_data()[i] = rng.next_uint32();
            return result;
        }, "shape"_a)
        .def("next_uint32_bounded", [](PCG32 &rng, UInt32 bound) {
                return rng.next_uint32_bounded(bound);
            }, "bound"_a, D(PCG32, next_uint32_bounded))
        .def("next_uint32_bounded", [](PCG32 &rng, UInt32 bound, const std::vector<size_t> &shape) {
            py::array_t<UInt32> result(shape);
            for (py::ssize_t i = 0; i < result.size(); ++i)
                result.mutable_data()[i] = rng.next_uint32_bounded(bound);
            return result;
        }, "bound"_a, "shape"_a)
        .def("next_uint64", (uint64_t (PCG32::*)()) &PCG32::next_uint64, D(PCG32, next_uint64))
        .def("next_uint64", [](PCG32 &rng, const std::vector<size_t> &shape) {
            py::array_t<uint64_t> result(shape);
            for (py::ssize_t i = 0; i < result.size(); ++i)
                result.mutable_data()[i] = rng.next_uint64();
            return result;
        }, "shape"_a)
        .def("next_uint64_bounded", [](PCG32 &rng, uint64_t bound) {
                return rng.next_uint64_bounded(bound);
            }, "bound"_a, D(PCG32, next_uint64_bounded))
        .def("next_uint64_bounded",
            [](PCG32 &rng, uint64_t bound, const std::vector<size_t> &shape) {
                py::array_t<uint64_t> result(shape);
                for (py::ssize_t i = 0; i < result.size(); ++i)
                    result.mutable_data()[i] = rng.next_uint64_bounded(bound);
                return result;
            }, "bound"_a, "shape"_a)
        .def("next_float32", (Float (PCG32::*)()) &PCG32::next_float32, D(PCG32, next_float32))
        .def("next_float32", [](PCG32 &rng, const std::vector<size_t> &shape) {
            py::array_t<Float> result(shape);
            for (py::ssize_t i = 0; i < result.size(); ++i)
                result.mutable_data()[i] = rng.next_float32();
            return result;
        }, "shape"_a)
        .def("next_float64", (Float64 (PCG32::*)()) &PCG32::next_float64, D(PCG32, next_float64))
        .def("next_float64", [](PCG32 &rng, const std::vector<size_t> &shape) {
            py::array_t<Float64> result(shape);
            for (py::ssize_t i = 0; i < result.size(); ++i)
                result.mutable_data()[i] = rng.next_float64();
            return result;
        }, "shape"_a)

        .def("advance", &PCG32::advance, "delta"_a, D(PCG32, advance))
        .def("shuffle", [](PCG32 &p, py::list l) {
            auto vec = l.cast<std::vector<py::object>>();
            p.shuffle(vec.begin(), vec.end());
            for (size_t i = 0; i < vec.size(); ++i)
                l[i] = vec[i];
        }, D(PCG32, shuffle))
        .def(py::self == py::self, D(PCG32, operator, eq))
        .def(py::self != py::self, D(PCG32, operator, ne))
        .def(py::self - py::self, D(PCG32, operator, sub))
        .def_repr(PCG32)
        ;

        pcg32.def("next_float",
            [p = py::handle(pcg32)](py::args args, py::kwargs kwargs) -> py::object {
                if constexpr (is_float_v<Float>)
                    return p.attr("next_float32")(*args, **kwargs);
                else
                    return p.attr("next_float64")(*args, **kwargs);
            });

    m.def("sample_tea_float32",
          vectorize<Float>(sample_tea_float32<UInt32>),
          "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_float32));

    m.def("sample_tea_float64",
          vectorize<Float>(sample_tea_float64<UInt32>),
          "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_float64));

    m.attr("sample_tea_float") = m.attr(
        sizeof(Float) != sizeof(Float64) ? "sample_tea_float32" : "sample_tea_float64");
}
