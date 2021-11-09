#include <mitsuba/render/interaction.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(RayFlags) {
    auto e = py::enum_<RayFlags>(m, "RayFlags", py::arithmetic())
        .def_value(RayFlags, None)
        .def_value(RayFlags, Minimal)
        .def_value(RayFlags, UV)
        .def_value(RayFlags, dPdUV)
        .def_value(RayFlags, dNGdUV)
        .def_value(RayFlags, dNSdUV)
        .def_value(RayFlags, ShadingFrame)
        .def_value(RayFlags, AttachShape)
        .def_value(RayFlags, NonDifferentiable)
        .def_value(RayFlags, Coherent)
        .def_value(RayFlags, All)
        .def_value(RayFlags, AllNonDifferentiable);

        MTS_PY_DECLARE_ENUM_OPERATORS(RayFlags, e)
}
