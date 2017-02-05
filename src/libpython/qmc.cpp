#include <mitsuba/core/qmc.h>
#include <mitsuba/core/logger.h>
#include "python.h"

using namespace qmc;

MTS_PY_EXPORT(qmc) {
    MTS_PY_IMPORT_MODULE(qmc, "mitsuba.core.qmc");

    py::class_<PermutationStorage, Object, ref<PermutationStorage>>(
        qmc, "PermutationStorage", D(qmc, PermutationStorage))
        .def(py::init<int>(), "scramble"_a = -1)
        .def("scramble", &PermutationStorage::scramble,
             D(qmc, PermutationStorage, scramble))
        .def("permutation", [](py::object self, uint32_t index) {
                const PermutationStorage &s = py::cast<const PermutationStorage&>(self);
                return py::array_t<uint16_t>(prime_base(index), s.permutation(index), self);
             }, D(qmc, PermutationStorage, permutation))
        .def("inverse_permutation", &PermutationStorage::inverse_permutation,
             D(qmc, PermutationStorage, inverse_permutation));

    qmc.def("prime_base", &prime_base, D(qmc, prime_base));

#if 0
    qmc.def("radical_inverse",
            py::overload_cast<size_t, UInt64P>(&radical_inverse),
            D(qmc, radical_inverse, 2));

    qmc.def("radical_inverse",
            [](size_t base, py::array_t<uint64_t> index) {
                return py::vectorize([base](uint64_t index) {
                    return radical_inverse(base, index);
                })(index);
            },
            D(qmc, radical_inverse));

    qmc.def("scrambled_radical_inverse",
            [](size_t base, UInt64P index, py::array_t<uint16_t> perm) {
                Assert(perm.size() >= prime_base(base));
                return scrambled_radical_inverse(base, index, perm.data());
            }, D(qmc, scrambled_radical_inverse));

    qmc.def("scrambled_radical_inverse",
            [](size_t base, py::array_t<uint64_t> index, py::array_t<uint16_t> perm) {
                Assert(perm.size() >= prime_base(base));
                return py::vectorize([base, &perm](uint64_t index) {
                    return scrambled_radical_inverse(base, index, perm.data());
                })(index);
            }, D(qmc, scrambled_radical_inverse));
#endif
}
