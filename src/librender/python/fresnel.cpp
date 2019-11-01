#include <mitsuba/python/python.h>
#include <mitsuba/render/fresnel.h>

MTS_PY_EXPORT_FLOAT_VARIANTS(fresnel) {
    MTS_IMPORT_CORE_TYPES()
    m.def("fresnel",
        vectorize<Float>(&fresnel<Float>),
        "cos_theta_i"_a, "eta"_a, D(fresnel))
    .def("fresnel_conductor",
        vectorize<Float>(&fresnel_conductor<Float>),
        "cos_theta_i"_a, "eta"_a, D(fresnel_conductor))
    .def("fresnel_polarized",
        vectorize<Float>(py::overload_cast<Float, Complex<Float>>(&fresnel_polarized<Float>)),
        "cos_theta_i"_a, "eta"_a, D(fresnel_polarized, 2))
    .def("reflect",
        vectorize<Float>(py::overload_cast<const Vector3f &>(&reflect<Vector3f>)),
        "wi"_a, D(reflect))
    .def("reflect",
        vectorize<Float>(
            py::overload_cast<const Vector3f &, const Normal3f &>(&reflect<Vector3f, Normal3f>)),
        "wi"_a, "m"_a, D(reflect, 2))
    .def("refract",
        vectorize<Float>(
            py::overload_cast<const Vector3f &, Float, Float>(&refract<Vector3f, Float>)),
        "wi"_a, "cos_theta_t"_a, "eta_ti"_a, D(refract))
    .def("refract",
        vectorize<Float>(py::overload_cast<const Vector3f &, const Normal3f &, Float, Float>(
              &refract<Vector3f, Normal3f, Float>)),
        "wi"_a, "m"_a, "cos_theta_t"_a, "eta_ti"_a, D(refract, 2))
    ;
}
