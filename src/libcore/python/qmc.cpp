#include <mitsuba/core/qmc.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(qmc) {
    py::class_<RadicalInverse, Object, ref<RadicalInverse>>(m, "RadicalInverse", D(RadicalInverse))
        .def(py::init<size_t, int>(), "max_base"_a = 8161, "scramble"_a = -1)
        .def("base", &RadicalInverse::base, D(RadicalInverse, base))
        .def("bases", &RadicalInverse::bases, D(RadicalInverse, bases))
        .def("scramble", &RadicalInverse::scramble, D(RadicalInverse, scramble))
        .def("eval",
             py::overload_cast<size_t, uint64_t>(&RadicalInverse::eval, py::const_),
             "base_index"_a, "index"_a, D(RadicalInverse, eval))
        .def("eval",
             enoki::vectorize_wrapper(py::overload_cast<size_t, UInt64P>(
                  &RadicalInverse::eval, py::const_)),
             "base_index"_a, "index"_a, D(RadicalInverse, eval, 2))
        .def("eval_scrambled",
             py::overload_cast<size_t, uint64_t>(&RadicalInverse::eval_scrambled, py::const_),
             "base_index"_a, "index"_a, D(RadicalInverse, eval_scrambled))
        .def("eval_scrambled",
             enoki::vectorize_wrapper(py::overload_cast<size_t, UInt64P>(
                 &RadicalInverse::eval_scrambled, py::const_)),
             "base_index"_a, "index"_a, D(RadicalInverse, eval_scrambled, 2))
        .def("permutation",
            [](py::object self, uint32_t index) {
                const RadicalInverse &s = py::cast<const RadicalInverse&>(self);
                return py::array_t<uint16_t>(s.base(index), s.permutation(index), self);
            }, D(RadicalInverse, permutation))
        .def("inverse_permutation", &RadicalInverse::inverse_permutation,
             D(RadicalInverse, inverse_permutation));
}
