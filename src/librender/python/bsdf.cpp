#include <mitsuba/python/python.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/core/properties.h>

MTS_PY_EXPORT(BSDFContext) {
    py::enum_<TransportMode>(m, "TransportMode", D(TransportMode))
        .value("Radiance", TransportMode::Radiance, D(TransportMode, Radiance))
        .value("Importance", TransportMode::Importance, D(TransportMode, Importance));

    py::enum_<BSDFFlags>(m, "BSDFFlags", D(BSDFFlags))
        .value("None", BSDFFlags::None, D(BSDFFlags, None))
        .value("Null", BSDFFlags::Null, D(BSDFFlags, Null))
        .value("DiffuseReflection", BSDFFlags::DiffuseReflection, D(BSDFFlags, DiffuseReflection))
        .value("DiffuseTransmission", BSDFFlags::DiffuseTransmission, D(BSDFFlags, DiffuseTransmission))
        .value("GlossyReflection", BSDFFlags::GlossyReflection, D(BSDFFlags, GlossyReflection))
        .value("GlossyTransmission", BSDFFlags::GlossyTransmission, D(BSDFFlags, GlossyTransmission))
        .value("DeltaReflection", BSDFFlags::DeltaReflection, D(BSDFFlags, DeltaReflection))
        .value("DeltaTransmission", BSDFFlags::DeltaTransmission, D(BSDFFlags, DeltaTransmission))
        .value("Anisotropic", BSDFFlags::Anisotropic, D(BSDFFlags, Anisotropic))
        .value("SpatiallyVarying", BSDFFlags::SpatiallyVarying, D(BSDFFlags, SpatiallyVarying))
        .value("NonSymmetric", BSDFFlags::NonSymmetric, D(BSDFFlags, NonSymmetric))
        .value("FrontSide", BSDFFlags::FrontSide, D(BSDFFlags, FrontSide))
        .value("BackSide", BSDFFlags::BackSide, D(BSDFFlags, BackSide))
        .value("Reflection", BSDFFlags::Reflection, D(BSDFFlags, Reflection))
        .value("Transmission", BSDFFlags::Transmission, D(BSDFFlags, Transmission))
        .value("Diffuse", BSDFFlags::Diffuse, D(BSDFFlags, Diffuse))
        .value("Glossy", BSDFFlags::Glossy, D(BSDFFlags, Glossy))
        .value("Smooth", BSDFFlags::Smooth, D(BSDFFlags, Smooth))
        .value("Delta", BSDFFlags::Delta, D(BSDFFlags, Delta))
        .value("Delta1D", BSDFFlags::Delta1D, D(BSDFFlags, Delta1D))
        .value("All", BSDFFlags::All, D(BSDFFlags, All))
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
