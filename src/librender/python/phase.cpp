#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>



MTS_PY_EXPORT(PhaseFunctionExtras) {
    py::enum_<PhaseFunctionFlags>(m, "PhaseFunctionFlags", D(PhaseFunctionFlags))
        .value("None", PhaseFunctionFlags::None, D(BSDFFlags, None))
        .value("Isotropic", PhaseFunctionFlags::Isotropic, D(PhaseFunctionFlags, Isotropic))
        .value("Anisotropic", PhaseFunctionFlags::Anisotropic,
                D(PhaseFunctionFlags, Anisotropic))
        .value("Microflake", PhaseFunctionFlags::Microflake, D(PhaseFunctionFlags, Microflake))
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