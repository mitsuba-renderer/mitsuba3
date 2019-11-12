#include <mitsuba/core/quad.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT_VARIANTS(quad) {
    using FloatX = DynamicArray<Packet<float>>;
    m.def_method(quad, gauss_legendre<FloatX>, "n"_a)
     .def_method(quad, gauss_lobatto<FloatX>, "n"_a)
     .def_method(quad, composite_simpson<FloatX>, "n"_a)
     .def_method(quad, composite_simpson_38<FloatX>, "n"_a)
    ;
}
