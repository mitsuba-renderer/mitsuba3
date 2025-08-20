#include <mitsuba/render/emitter.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(EndpointFlags) {
    auto e = nb::enum_<EndpointFlags>(m, "EndpointFlags", nb::is_arithmetic(), D(EndpointFlags))
        .def_value(EndpointFlags, Empty)
        .def_value(EndpointFlags, DeltaPosition)
        .def_value(EndpointFlags, DeltaDirection)
        .def_value(EndpointFlags, Infinite)
        .def_value(EndpointFlags, Surface)
        .def_value(EndpointFlags, SpatiallyVarying)
        .def_value(EndpointFlags, Delta);
}
