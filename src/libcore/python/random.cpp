#include <mitsuba/core/random.h>
#include <mitsuba/python/python.h>

#define DE(...) "doc disabled" // TODO re-enable this DOC(enoki, ##__VA_ARGS__)

MTS_PY_EXPORT_STRUCT(random) {
    MTS_IMPORT_CORE_TYPES()
    using PCG32 = mitsuba::PCG32<ScalarUInt32>;
    MTS_PY_CHECK_ALIAS(PCG32, m) {
        auto pcg32 = py::class_<PCG32>(m, "PCG32", DE(PCG32))
            .def(py::init<ScalarUInt64, ScalarUInt64>(),
                "initstate"_a = PCG32_DEFAULT_STATE,
                "initseq"_a = PCG32_DEFAULT_STREAM, DE(PCG32, PCG32))
            .def(py::init<const PCG32 &>(), "Copy constructor")
            .def("seed", &PCG32::seed, "initstate"_a, "initseq"_a = 1u, DE(PCG32, seed))
            .def("next_uint32", (ScalarUInt32 (PCG32::*)()) &PCG32::next_uint32, DE(PCG32, next_uint32))
            .def("next_uint32", [](PCG32 &rng, const std::vector<size_t> &shape) {
                py::array_t<ScalarUInt32> result(shape);
                for (py::ssize_t i = 0; i < result.size(); ++i)
                    result.mutable_data()[i] = rng.next_uint32();
                return result;
            }, "shape"_a)
            .def("next_uint32_bounded", [](PCG32 &rng, ScalarUInt32 bound) {
                    return rng.next_uint32_bounded(bound);
                }, "bound"_a, DE(PCG32, next_uint32_bounded))
            .def("next_uint32_bounded", [](PCG32 &rng, ScalarUInt32 bound, const std::vector<size_t> &shape) {
                py::array_t<ScalarUInt32> result(shape);
                for (py::ssize_t i = 0; i < result.size(); ++i)
                    result.mutable_data()[i] = rng.next_uint32_bounded(bound);
                return result;
            }, "bound"_a, "shape"_a)
            .def("next_uint64", (ScalarUInt64 (PCG32::*)()) &PCG32::next_uint64, DE(PCG32, next_uint64))
            .def("next_uint64", [](PCG32 &rng, const std::vector<size_t> &shape) {
                py::array_t<ScalarUInt64> result(shape);
                for (py::ssize_t i = 0; i < result.size(); ++i)
                    result.mutable_data()[i] = rng.next_uint64();
                return result;
            }, "shape"_a)
            .def("next_uint64_bounded", [](PCG32 &rng, uint64_t bound) {
                    return rng.next_uint64_bounded(bound);
                }, "bound"_a, DE(PCG32, next_uint64_bounded))
            .def("next_uint64_bounded",
                [](PCG32 &rng, uint64_t bound, const std::vector<size_t> &shape) {
                    py::array_t<ScalarUInt64> result(shape);
                    for (py::ssize_t i = 0; i < result.size(); ++i)
                        result.mutable_data()[i] = rng.next_uint64_bounded(bound);
                    return result;
                }, "bound"_a, "shape"_a)
            .def("next_float32", (ScalarFloat32 (PCG32::*)()) &PCG32::next_float32, DE(PCG32, next_float32))
            .def("next_float32", [](PCG32 &rng, const std::vector<size_t> &shape) {
                py::array_t<ScalarFloat32> result(shape);
                for (py::ssize_t i = 0; i < result.size(); ++i)
                    result.mutable_data()[i] = rng.next_float32();
                return result;
            }, "shape"_a)
            .def("next_float64", (ScalarFloat64 (PCG32::*)()) &PCG32::next_float64, DE(PCG32, next_float64))
            .def("next_float64", [](PCG32 &rng, const std::vector<size_t> &shape) {
                py::array_t<ScalarFloat64> result(shape);
                for (py::ssize_t i = 0; i < result.size(); ++i)
                    result.mutable_data()[i] = rng.next_float64();
                return result;
            }, "shape"_a)
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
            .def_repr(PCG32)
            ;
        pcg32.def("next_float",
            [p = py::handle(pcg32)](py::args args, py::kwargs kwargs) -> py::object {
                if constexpr (is_float_v<ScalarFloat>)
                    return p.attr("next_float32")(*args, **kwargs);
                else
                    return p.attr("next_float64")(*args, **kwargs);
            });
    }
}

MTS_PY_EXPORT(sample_tea) {
    MTS_IMPORT_CORE_TYPES()
    m.def("sample_tea_float32",
          vectorize<Float>(sample_tea_float32<UInt32>),
          "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_float32));

    m.def("sample_tea_float64",
          vectorize<Float>(sample_tea_float64<UInt32>),
          "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_float64));

    m.attr("sample_tea_float") = m.attr(
        sizeof(Float) != sizeof(Float64) ? "sample_tea_float32" : "sample_tea_float64");
}
