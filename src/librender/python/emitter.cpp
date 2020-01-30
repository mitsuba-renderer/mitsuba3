#include <mitsuba/python/python.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/core/properties.h>

MTS_PY_EXPORT(EmitterExtras) {
    py::enum_<EmitterFlags>(m, "EmitterFlags", D(EmitterFlags))
        .value("None", EmitterFlags::None, D(EmitterFlags, None))
        .value("DeltaPosition", EmitterFlags::DeltaPosition, D(EmitterFlags, DeltaPosition))
        .value("DeltaDirection", EmitterFlags::DeltaDirection, D(EmitterFlags, DeltaDirection))
        .value("Infinite", EmitterFlags::Infinite, D(EmitterFlags, Infinite))
        .value("Surface", EmitterFlags::Surface, D(EmitterFlags, Surface))
        .value("SpatiallyVarying", EmitterFlags::SpatiallyVarying, D(EmitterFlags, SpatiallyVarying))
        .value("Delta", EmitterFlags::Delta, D(EmitterFlags, Delta))
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
