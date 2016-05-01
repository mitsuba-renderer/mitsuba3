#include <pcg32.h>
#include "python.h"

MTS_PY_EXPORT(pcg32) {
    py::class_<pcg32>(m, "pcg32", D(pcg32))
        .def(py::init<>(), D(pcg32, pcg32))
        .def(py::init<uint64_t, uint64_t>(), D(pcg32, pcg32, 2))
        .def(py::init<const pcg32 &>(), "Copy constructor")
        .def("seed", &pcg32::seed, py::arg("initstate"), py::arg("initseq") = 1u, D(pcg32, seed))
        .def("nextUInt", (uint32_t (pcg32::*)(void)) &pcg32::nextUInt, D(pcg32, nextUInt))
        .def("nextUInt", (uint32_t (pcg32::*)(uint32_t)) &pcg32::nextUInt, py::arg("bound"), D(pcg32, nextUInt, 2))
        .def("nextFloat", &pcg32::nextFloat, D(pcg32, nextFloat))
        .def("nextDouble", &pcg32::nextDouble, D(pcg32, nextDouble))
        .def("advance", &pcg32::advance, py::arg("delta"), D(pcg32, advance))
        .def("shuffle", [](pcg32 &p, py::list l) {
            auto vec = l.cast<std::vector<py::object>>();
            p.shuffle(vec.begin(), vec.end());
            for (size_t i = 0; i < vec.size(); ++i)
                l[i] = vec[i];
        }, D(pcg32, shuffle))
        .def(py::self == py::self, D(pcg32, operator, eq))
        .def(py::self != py::self, D(pcg32, operator, ne))
        .def(py::self - py::self, D(pcg32, operator, sub))
        .def("__repr__", [](const pcg32 &p) {
            std::ostringstream oss;
            oss << "pcg32[state=0x" << std::hex << p.state << ", inc=0x" << std::hex << p.inc << "]";
            return oss.str();
        });
}
