#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/python/python.h>



MTS_PY_EXPORT(PhaseFunctionExtras) {
    py::enum_<PhaseFunctionFlags>(m, "PhaseFunctionFlags", D(PhaseFunctionFlags))
        .def_value(PhaseFunctionFlags, None)
        .def_value(PhaseFunctionFlags, Isotropic)
        .def_value(PhaseFunctionFlags, Anisotropic)
        .def_value(PhaseFunctionFlags, Microflake)
        .def(py::self == py::self)
        .def(py::self | py::self)
        .def(int() | py::self)
        .def(py::self & py::self)
        .def(int() & py::self)
        .def(+py::self)
        .def(~py::self)
        .def("__pos__", [](const PhaseFunctionFlags &f) { return static_cast<uint32_t>(f); },
                py::is_operator());
}