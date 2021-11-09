#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/python/python.h>



MTS_PY_EXPORT(PhaseFunctionExtras) {
    auto e = py::enum_<PhaseFunctionFlags>(m, "PhaseFunctionFlags", D(PhaseFunctionFlags))
        .def_value(PhaseFunctionFlags, None)
        .def_value(PhaseFunctionFlags, Isotropic)
        .def_value(PhaseFunctionFlags, Anisotropic)
        .def_value(PhaseFunctionFlags, Microflake);

    MTS_PY_DECLARE_ENUM_OPERATORS(HitComputeFlags, e)
}