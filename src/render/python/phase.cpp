#include <mitsuba/core/properties.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(PhaseFunctionExtras) {
    auto e = nb::enum_<PhaseFunctionFlags>(m, "PhaseFunctionFlags", nb::is_arithmetic(), D(PhaseFunctionFlags))
        .def_value(PhaseFunctionFlags, Empty)
        .def_value(PhaseFunctionFlags, Isotropic)
        .def_value(PhaseFunctionFlags, Anisotropic)
        .def_value(PhaseFunctionFlags, Microflake);
}
