#include <mitsuba/core/quad.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(quad) {
    py::module quad = m.def_submodule("quad", "Functions for numerical quadrature");

    quad.mdef(quad, gauss_legendre, "n"_a)
        .mdef(quad, gauss_lobatto, "n"_a)
        .mdef(quad, composite_simpson, "n"_a)
        .mdef(quad, composite_simpson_38, "n"_a)
    ;
}
