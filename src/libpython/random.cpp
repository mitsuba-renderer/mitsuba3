#include <pcg32.h>
#include <mitsuba/core/random.h>
#include "python.h"

MTS_PY_EXPORT(random) {
    py::class_<pcg32> pcg32_(m, "pcg32", D(pcg32));
    pcg32_
        .def(py::init<>(), D(pcg32, pcg32))
        .def(py::init<uint64_t, uint64_t>(), D(pcg32, pcg32, 2))
        .def(py::init<const pcg32 &>(), "Copy constructor")
        .def("seed", &pcg32::seed, py::arg("initstate"), py::arg("initseq") = 1u, D(pcg32, seed))
        .def("nextUInt", py::overload_cast<>(&pcg32::nextUInt), D(pcg32, nextUInt))
        .def("nextUInt", py::overload_cast<uint32_t>(&pcg32::nextUInt), py::arg("bound"), D(pcg32, nextUInt, 2))
        .def("nextSingle", &pcg32::nextFloat, D(pcg32, nextFloat))
        .def("nextSingle", [](pcg32 &rng, size_t n) {
            py::array_t<float> result(n);
            for (size_t i = 0; i < n; ++i)
                result.mutable_data()[i] = rng.nextFloat();
            return result;
        }, py::arg("n"))
        .def("nextSingle", [](pcg32 &rng, size_t m, size_t n) {
            py::array_t<float> result({m, n});
            for (size_t i = 0; i < n*m; ++i)
                result.mutable_data()[i] = rng.nextFloat();
            return result;
        }, py::arg("m"), py::arg("n"))
        .def("nextDouble", &pcg32::nextDouble, D(pcg32, nextDouble))
        .def("nextDouble", [](pcg32 &rng, size_t n) {
            py::array_t<float> result(n);
            for (size_t i = 0; i < n; ++i)
                result.mutable_data()[i] = rng.nextDouble();
            return result;
        }, py::arg("n"))
        .def("nextDouble", [](pcg32 &rng, size_t m, size_t n) {
            py::array_t<float> result({m, n});
            for (size_t i = 0; i < n*m; ++i)
                result.mutable_data()[i] = rng.nextDouble();
            return result;
        }, py::arg("m"), py::arg("n"))
        .def("nextFloat", [p = py::handle(pcg32_)](py::args args, py::kwargs kwargs) -> py::object {
            #if defined(SINGLE_PRECISION)
                return p.attr("nextSingle")(*args, **kwargs);
            #else
                return p.attr("nextDouble")(*args, **kwargs);
            #endif
        })
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

    m.def("sampleTEASingle",
          py::overload_cast<uint32_t, uint32_t, int>(&sampleTEASingle),
          py::arg("v0"), py::arg("v1"), py::arg("rounds") = 4, DM(sampleTEASingle));

    m.def("sampleTEASingle",
          py::overload_cast<UInt32Packet, UInt32Packet, int>(&sampleTEASingle),
          py::arg("v0"), py::arg("v1"), py::arg("rounds") = 4, DM(sampleTEASingle, 2));

    m.def("sampleTEADouble",
          py::overload_cast<uint32_t, uint32_t, int>(&sampleTEADouble),
          py::arg("v0"), py::arg("v1"), py::arg("rounds") = 4, DM(sampleTEADouble));

    m.def("sampleTEADouble",
          py::overload_cast<UInt32Packet, UInt32Packet, int>(&sampleTEADouble),
          py::arg("v0"), py::arg("v1"), py::arg("rounds") = 4, DM(sampleTEADouble, 2));

    m.attr("sampleTEAFloat") = m.attr(
        sizeof(Float) == sizeof(float) ? "sampleTEASingle" : "sampleTEADouble");
}
