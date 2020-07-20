#include <mitsuba/core/properties.h>
#include <mitsuba/render/microfacet.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(MicrofacetType) {
    py::enum_<MicrofacetType>(m, "MicrofacetType", D(MicrofacetType), py::arithmetic())
        .def_value(MicrofacetType, Beckmann)
        .def_value(MicrofacetType, GGX);
}
