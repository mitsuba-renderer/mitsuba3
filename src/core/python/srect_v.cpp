#include <mitsuba/core/srect.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/string.h>

template <typename SRect> auto bind_srect(nb::module_ &m, const char *name) {
    using Point3  = typename SRect::Point3;
    using Vector3 = typename SRect::Vector3;

    MI_PY_CHECK_ALIAS(SRect, name) {
        nb::class_<SRect>(m, name, D(SphericalRectangle))
            .def(nb::init<const Point3 &, const Point3 &, const Vector3 &,
                          const Vector3 &>(),
                 "p"_a, "center"_a, "dx"_a, "dy"_a,
                 D(SphericalRectangle, SphericalRectangle))
            .def("contains", &SRect::contains, "d"_a, D(SphericalRectangle, contains))
            .def("sample", &SRect::sample, "sample"_a, D(SphericalRectangle, sample))
            .def("pdf", &SRect::pdf, "d"_a, D(SphericalRectangle, pdf))
            .def_ro("solid_angle", &SRect::solid_angle, D(SphericalRectangle, solid_angle))
            .def_ro("degenerate", &SRect::degenerate, D(SphericalRectangle, degenerate))
            .def_repr(SRect);
    }
}

MI_PY_EXPORT(SphericalRectangle) {
    MI_PY_IMPORT_TYPES()

    bind_srect<SphericalRectangle3f>(m, "SphericalRectangle3f");
    bind_srect<ScalarSphericalRectangle3f>(m, "ScalarSphericalRectangle3f");
}
