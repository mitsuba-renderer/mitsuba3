#include <mitsuba/render/emitter.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(EmitterExtras) {
    py::enum_<EmitterFlags>(m, "EmitterFlags", D(EmitterFlags))
        .def_value(EmitterFlags, None)
        .def_value(EmitterFlags, DeltaPosition)
        .def_value(EmitterFlags, DeltaDirection)
        .def_value(EmitterFlags, Infinite)
        .def_value(EmitterFlags, Surface)
        .def_value(EmitterFlags, SpatiallyVarying)
        .def_value(EmitterFlags, Delta)
        .def(py::self == py::self)
        .def(py::self | py::self)
        .def(int() | py::self)
        .def(py::self & py::self)
        .def(int() & py::self)
        .def(+py::self)
        .def(~py::self)
        .def("__pos__", [](const EmitterFlags &f) {
            return static_cast<uint32_t>(f);
        }, py::is_operator());
}
