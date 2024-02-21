#include <mitsuba/core/properties.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(MicrofacetType) {
    nb::enum_<MicrofacetType>(m, "MicrofacetType", D(MicrofacetType), nb::is_arithmetic())
        .def_value(MicrofacetType, Beckmann)
        .def_value(MicrofacetType, GGX);
}
