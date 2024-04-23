#include <mitsuba/render/emitter.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(EmitterExtras) {
    auto e = nb::enum_<EmitterFlags>(m, "EmitterFlags", nb::is_arithmetic(), D(EmitterFlags))
        .def_value(EmitterFlags, Empty)
        .def_value(EmitterFlags, DeltaPosition)
        .def_value(EmitterFlags, DeltaDirection)
        .def_value(EmitterFlags, Infinite)
        .def_value(EmitterFlags, Surface)
        .def_value(EmitterFlags, SpatiallyVarying)
        .def_value(EmitterFlags, Delta);
}
