#include <mitsuba/core/qmc.h>
#include <mitsuba/python/python.h>

#include <nanobind/ndarray.h>

MI_PY_EXPORT(qmc) {
    MI_PY_IMPORT_TYPES()

    MI_PY_CHECK_ALIAS(RadicalInverse, "RadicalInverse") {
        nb::class_<RadicalInverse, Object>(m, "RadicalInverse", D(RadicalInverse))
            .def(nb::init<size_t, int>(), "max_base"_a = 8161, "scramble"_a = -1)
            .def("base", &RadicalInverse::base, D(RadicalInverse, base))
            .def("bases", &RadicalInverse::bases, D(RadicalInverse, bases))
            .def("scramble", &RadicalInverse::scramble, D(RadicalInverse, scramble))
            .def("eval", &RadicalInverse::eval<Float>, "base_index"_a, "index"_a,
                 D(RadicalInverse, eval))
            // .def("eval_scrambled", &RadicalInverse::eval_scrambled<Float>, "base_index"_a,
                //  "index"_a, D(RadicalInverse, eval_scrambled))
            .def("permutation",
                 [](nb::object self, uint32_t index) {
                     const RadicalInverse &s = nb::cast<const RadicalInverse &>(self);
                     return nb::ndarray<uint16_t, nb::numpy>(s.permutation(index), {s.base(index)}, self);
                 },
                 D(RadicalInverse, permutation))
            .def("inverse_permutation", &RadicalInverse::inverse_permutation,
                 D(RadicalInverse, inverse_permutation));

        m.def("radical_inverse_2", radical_inverse_2<UInt32>,
              "index"_a, "scramble"_a, D(radical_inverse_2));

        m.def("sobol_2", sobol_2<UInt32>,
              "index"_a, "scramble"_a, D(sobol_2));
    }
}
