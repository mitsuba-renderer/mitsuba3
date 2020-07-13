#include <mitsuba/render/interaction.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(HitComputeFlags) {
    py::enum_<HitComputeFlags>(m, "HitComputeFlags", py::arithmetic())
        .def_value(HitComputeFlags, None)
        .def_value(HitComputeFlags, Minimal)
        .def_value(HitComputeFlags, UV)
        .def_value(HitComputeFlags, dPdUV)
        .def_value(HitComputeFlags, dNGdUV)
        .def_value(HitComputeFlags, dNSdUV)
        .def_value(HitComputeFlags, ShadingFrame)
        .def_value(HitComputeFlags, NonDifferentiable)
        .def_value(HitComputeFlags, All)
        .def_value(HitComputeFlags, AllNonDifferentiable)
        .def(py::self == py::self)
        .def(py::self | py::self)
        .def(py::self & py::self)
        .def(~py::self);
}