#include <mitsuba/core/qmc.h>
#include <pybind11/numpy.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(qmc) {
    MTS_PY_IMPORT_TYPES()

    py::class_<RadicalInverse, Object, ref<RadicalInverse>>(m, "RadicalInverse", D(RadicalInverse),
                                                            py::module_local())
        .def(py::init<size_t, int>(), "max_base"_a = 8161, "scramble"_a = -1)
        .def("base", &RadicalInverse::base, D(RadicalInverse, base))
        .def("bases", &RadicalInverse::bases, D(RadicalInverse, bases))
        .def("scramble", &RadicalInverse::scramble, D(RadicalInverse, scramble))
        .def("eval", vectorize(&RadicalInverse::eval<Float>), "base_index"_a, "index"_a,
             D(RadicalInverse, eval))
        .def("eval_scrambled", vectorize(&RadicalInverse::eval_scrambled<Float>), "base_index"_a,
             "index"_a, D(RadicalInverse, eval_scrambled))
        .def("permutation",
             [](py::object self, uint32_t index) {
                 const RadicalInverse &s = py::cast<const RadicalInverse &>(self);
                 return py::array_t<uint16_t>(s.base(index), s.permutation(index), self);
             },
             D(RadicalInverse, permutation))
        .def("inverse_permutation", &RadicalInverse::inverse_permutation,
             D(RadicalInverse, inverse_permutation));

    m.def("radical_inverse_2", vectorize(radical_inverse_2<UInt32>),
          "index"_a, "scramble"_a, D(radical_inverse_2));

    m.def("sobol_2", vectorize(sobol_2<UInt32>),
          "index"_a, "scramble"_a, D(sobol_2));
}
