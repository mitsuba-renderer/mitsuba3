#include <mitsuba/render/field.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(FieldTypes) {
    nb::enum_<FieldValueType>(m, "FieldValueType", "Semantic field output type")
        .value("Float", FieldValueType::Float)
        .value("Spectrum", FieldValueType::Spectrum)
        .value("Color3", FieldValueType::Color3)
        .value("Array2", FieldValueType::Array2)
        .value("Array3", FieldValueType::Array3)
        .value("Features", FieldValueType::Features);

    nb::enum_<FieldDomain>(m, "FieldDomain", "Structured field query domain")
        .value("Surface", FieldDomain::Surface)
        .value("Interaction", FieldDomain::Interaction)
        .value("SurfaceAndInteraction", FieldDomain::SurfaceAndInteraction);
}
