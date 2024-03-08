#include <mitsuba/core/quad.h>
#include <mitsuba/python/python.h>
#include <drjit/dynamic.h>

#include <nanobind/stl/pair.h>

MI_PY_EXPORT(quad) {
    MI_PY_IMPORT_TYPES()
    using FloatX = DynamicBuffer<Float>;
    m.def("gauss_legendre", &quad::gauss_legendre<FloatX>, "n"_a, D(quad, gauss_legendre));
    m.def("gauss_lobatto", &quad::gauss_lobatto<FloatX>, "n"_a, D(quad, gauss_lobatto));
    m.def("composite_simpson", &quad::composite_simpson<FloatX>, "n"_a,
          D(quad, composite_simpson));
    m.def("composite_simpson_38", &quad::composite_simpson_38<FloatX>, "n"_a,
          D(quad, composite_simpson_38));
    m.def("chebyshev", &quad::chebyshev<FloatX>, "n"_a,
          D(quad, chebyshev));
}
