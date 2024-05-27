#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/optional.h>

MI_PY_EXPORT(BSDFContext) {
    nb::enum_<TransportMode>(m, "TransportMode", D(TransportMode))
        .def_value(TransportMode, Radiance)
        .def_value(TransportMode, Importance);

    auto e = nb::enum_<BSDFFlags>(m, "BSDFFlags", nb::is_arithmetic(), D(BSDFFlags))
        .def_value(BSDFFlags, Empty)
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
        .def_value(BSDFFlags, All);

    nb::class_<BSDFContext>(m, "BSDFContext", D(BSDFContext))
        .def(nb::init<TransportMode>(),
            "mode"_a = TransportMode::Radiance, D(BSDFContext, BSDFContext))
        .def(nb::init<TransportMode, uint32_t, uint32_t>(),
            "mode"_a, "type_mask"_a, "component"_a, D(BSDFContext, BSDFContext, 2))
        .def("__init__",
            [](BSDFContext *ctx, TransportMode mode, uint32_t type_mask,
               std::optional<uint32_t> component) {
                new (ctx) BSDFContext(mode, type_mask, component.has_value() ? component.value() : -1);
            }, "mode"_a, "type_mask"_a, "component"_a = nb::none(), D(BSDFContext, BSDFContext, 2)
        )
        .def_method(BSDFContext, reverse)
        .def_method(BSDFContext, is_enabled, "type"_a, "component"_a = 0)
        .def_field(BSDFContext, mode,      D(BSDFContext, mode))
        .def_field(BSDFContext, type_mask, D(BSDFContext, type_mask))
        .def_field(BSDFContext, component, D(BSDFContext, component))
        .def_repr(BSDFContext);
}
