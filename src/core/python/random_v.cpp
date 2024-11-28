#include <mitsuba/core/random.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/pair.h>

MI_PY_EXPORT(sample_tea) {
    MI_PY_IMPORT_TYPES()

    if constexpr (dr::is_jit_v<UInt32>) {
        m.def("sample_tea_32", sample_tea_32<uint32_t>,
               "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_32));

        m.def("sample_tea_64", sample_tea_64<uint32_t>,
               "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_64));

        m.def("sample_tea_float32",
              sample_tea_float32<uint32_t>,
              "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_float32));

        m.def("sample_tea_float64",
              sample_tea_float64<uint32_t>,
              "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_float64));
    }

    m.def("sample_tea_32", sample_tea_32<UInt32>,
           "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_32));

    m.def("sample_tea_64", sample_tea_64<UInt32>,
           "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_64));

    m.def("sample_tea_float32",
          sample_tea_float32<UInt32>,
          "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_float32));

    m.def("sample_tea_float64",
          sample_tea_float64<UInt32>,
          "v0"_a, "v1"_a, "rounds"_a = 4, D(sample_tea_float64));

    m.attr("sample_tea_float") = m.attr(
        sizeof(dr::scalar_t<Float>) != sizeof(dr::scalar_t<Float64>) ? "sample_tea_float32" : "sample_tea_float64");

    m.def("permute",
          permute<UInt32>,
          "value"_a, "size"_a, "seed"_a, "rounds"_a = 4, D(permute));

    m.def("permute_kensler",
          permute_kensler<UInt32>,
          "i"_a, "l"_a, "p"_a, "active"_a = true, D(permute_kensler));
}
