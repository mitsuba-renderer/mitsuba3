#include <mitsuba/render/emitter.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(EmitterExtras) {
    auto e = py::enum_<EmitterFlags>(m, "EmitterFlags", D(EmitterFlags))
        .def_value(EmitterFlags, Empty)
        .def_value(EmitterFlags, DeltaPosition)
        .def_value(EmitterFlags, DeltaDirection)
        .def_value(EmitterFlags, Infinite)
        .def_value(EmitterFlags, Surface)
        .def_value(EmitterFlags, SpatiallyVarying)
        .def_value(EmitterFlags, Delta);

        MI_PY_DECLARE_ENUM_OPERATORS(EmitterFlags, e)
}
