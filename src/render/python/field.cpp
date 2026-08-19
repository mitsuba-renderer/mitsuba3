#include <mitsuba/render/field.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(FieldTypes) {
    nb::enum_<FieldValueType>(
        m, "FieldValueType", D(FieldValueType))
        .def_value(FieldValueType, Float)
        .def_value(FieldValueType, Spectrum)
        .def_value(FieldValueType, Color3)
        .def_value(FieldValueType, Array2)
        .def_value(FieldValueType, Array3)
        .def_value(FieldValueType, Features);

    nb::enum_<FieldDomain>(
        m, "FieldDomain", D(FieldDomain))
        .def_value(FieldDomain, Surface)
        .def_value(FieldDomain, Interaction)
        .def_value(FieldDomain, SurfaceAndInteraction);
}
