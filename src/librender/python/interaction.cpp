#include <mitsuba/render/interaction.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(HitComputeFlags) {
    auto e = py::enum_<HitComputeFlags>(m, "HitComputeFlags", py::arithmetic())
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
        .def_value(HitComputeFlags, AllNonDifferentiable);

        MTS_PY_DECLARE_ENUM_OPERATORS(HitComputeFlags, e)
}
