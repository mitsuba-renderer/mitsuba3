#include <mitsuba/core/quad.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(quad) {
    py::module quad = m.def_submodule("quad", "Functions for numerical quadrature");
    using FloatX = DynamicArray<Packet<float>>;

    quad.def("gauss_legendre", &quad::gauss_legendre<FloatX>, "n"_a, D(quad, gauss_legendre));
    quad.def("gauss_lobatto", &quad::gauss_lobatto<FloatX>, "n"_a, D(quad, gauss_lobatto));
    quad.def("composite_simpson", &quad::composite_simpson<FloatX>, "n"_a,
             D(quad, composite_simpson));
    quad.def("composite_simpson_38", &quad::composite_simpson_38<FloatX>, "n"_a,
             D(quad, composite_simpson_38));
}
