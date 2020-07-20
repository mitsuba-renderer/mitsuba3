#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(BSDFContext) {
    py::enum_<TransportMode>(m, "TransportMode", D(TransportMode))
        .def_value(TransportMode, Radiance)
        .def_value(TransportMode, Importance);

    py::enum_<BSDFFlags>(m, "BSDFFlags", D(BSDFFlags))
        .def_value(BSDFFlags, None)
        .def_value(BSDFFlags, Null)
        .def_value(BSDFFlags, DiffuseReflection)
        .def_value(BSDFFlags, DiffuseTransmission)
        .def_value(BSDFFlags, GlossyReflection)
        .def_value(BSDFFlags, GlossyTransmission)
        .def_value(BSDFFlags, DeltaReflection)
        .def_value(BSDFFlags, DeltaTransmission)
        .def_value(BSDFFlags, Anisotropic)
        .def_value(BSDFFlags, SpatiallyVarying)
        .def_value(BSDFFlags, NonSymmetric)
        .def_value(BSDFFlags, FrontSide)
        .def_value(BSDFFlags, BackSide)
        .def_value(BSDFFlags, Reflection)
        .def_value(BSDFFlags, Transmission)
        .def_value(BSDFFlags, Diffuse)
        .def_value(BSDFFlags, Glossy)
        .def_value(BSDFFlags, Smooth)
        .def_value(BSDFFlags, Delta)
        .def_value(BSDFFlags, Delta1D)
        .def_value(BSDFFlags, All)
        .def(py::self == py::self)
        .def(py::self | py::self)
        .def(int() | py::self)
        .def(py::self & py::self)
        .def(int() & py::self)
        .def(+py::self)
        .def(~py::self)
        .def("__pos__", [](const BSDFFlags &f) {
            return static_cast<uint32_t>(f);
        }, py::is_operator());

    py::class_<BSDFContext>(m, "BSDFContext", D(BSDFContext))
        .def(py::init<TransportMode>(),
            "mode"_a = TransportMode::Radiance, D(BSDFContext, BSDFContext))
        .def(py::init<TransportMode, uint32_t, uint32_t>(),
            "mode"_a, "type_mak"_a, "component"_a, D(BSDFContext, BSDFContext, 2))
        .def_method(BSDFContext, reverse)
        .def_method(BSDFContext, is_enabled, "type"_a, "component"_a = 0)
        .def_field(BSDFContext, mode,      D(BSDFContext, mode))
        .def_field(BSDFContext, type_mask, D(BSDFContext, type_mask))
        .def_field(BSDFContext, component, D(BSDFContext, component))
        .def_repr(BSDFContext);
}
