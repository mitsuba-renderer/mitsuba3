#include <mitsuba/render/interaction.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(RayFlags) {
    auto e = py::enum_<RayFlags>(m, "RayFlags", py::arithmetic())
        .def_value(RayFlags, Empty)
        .def_value(RayFlags, Minimal)
        .def_value(RayFlags, UV)
        .def_value(RayFlags, dPdUV)
        .def_value(RayFlags, dNGdUV)
        .def_value(RayFlags, dNSdUV)
        .def_value(RayFlags, ShadingFrame)
        .def_value(RayFlags, FollowShape)
        .def_value(RayFlags, DetachShape)
        .def_value(RayFlags, BoundaryTest)
        .def_value(RayFlags, All)
        .def_value(RayFlags, AllNonDifferentiable);

        MI_PY_DECLARE_ENUM_OPERATORS(RayFlags, e)
}
