#include <mitsuba/core/qmc.h>
#include "python.h"

using namespace qmc;

MTS_PY_EXPORT(qmc) {
    MTS_PY_IMPORT_MODULE(qmc, "mitsuba.core.qmc");

    py::class_<PermutationStorage, Object, ref<PermutationStorage>>(
        qmc, "PermutationStorage", DM(qmc, PermutationStorage))
        .def(py::init<int>(), py::arg("scramble") = -1)
        .def("scramble", &PermutationStorage::scramble,
             DM(qmc, PermutationStorage, scramble))
        .def("permutation", [](py::object self, uint32_t index) {
                const PermutationStorage &s = py::cast<const PermutationStorage&>(self);
                return py::array_t<uint16_t>(primeBase(index), s.permutation(index), self);
             }, DM(qmc, PermutationStorage, permutation))
        .def("inversePermutation", &PermutationStorage::inversePermutation,
             DM(qmc, PermutationStorage, inversePermutation));

    qmc.def("primeBase", &primeBase, DM(qmc, primeBase));

    qmc.def("radicalInverse",
            py::overload_cast<size_t, UInt64Packet>(&radicalInverse),
            DM(qmc, radicalInverse, 2));

    qmc.def("radicalInverse",
            [](size_t base, py::array_t<uint64_t> index) {
                return py::vectorize([base](uint64_t index) {
                    return radicalInverse(base, index);
                })(index);
            },
            DM(qmc, radicalInverse));

    qmc.def("scrambledRadicalInverse",
            [](size_t base, UInt64Packet index, py::array_t<uint16_t> perm) {
                Assert(perm.size() >= primeBase(index));
                return scrambledRadicalInverse(base, index, perm.data());
            }, DM(qmc, scrambledRadicalInverse));

    qmc.def("scrambledRadicalInverse",
            [](size_t base, py::array_t<uint64_t> index, py::array_t<uint16_t> perm) {
                Assert(perm.size() >= primeBase(index));
                return py::vectorize([base, &perm](uint64_t index) {
                    return scrambledRadicalInverse(base, index, perm.data());
                })(index);
            }, DM(qmc, scrambledRadicalInverse));
}
