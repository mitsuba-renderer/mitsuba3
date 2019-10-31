#include <mitsuba/core/quad.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT_FLOAT_VARIANTS(quad) {
    py::module quad = m.def_submodule("quad", "Functions for numerical quadrature");

    quad.def_method(quad, gauss_legendre<Float>, "n"_a)
        .def_method(quad, gauss_lobatto<Float>, "n"_a)
        .def_method(quad, composite_simpson<Float>, "n"_a)
        .def_method(quad, composite_simpson_38<Float>, "n"_a)
    ;
}
