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
        .def_value(HitComputeFlags, Sticky)
        .def_value(HitComputeFlags, Coherent)
        .def_value(HitComputeFlags, All)
        .def_value(HitComputeFlags, AllNonDifferentiable)
        .def(py::self == py::self)
        .def(py::self | py::self)
        .def(int() | py::self)
        .def(py::self & py::self)
        .def(int() & py::self)
        .def(+py::self)
        .def(~py::self)
        .def("__pos__", [](const HitComputeFlags &f) {
            return static_cast<uint32_t>(f);
        }, py::is_operator());
}
