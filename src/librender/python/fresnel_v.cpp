#include <mitsuba/render/fresnel.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(fresnel) {
    MTS_PY_IMPORT_TYPES()
    m.def("fresnel",
        vectorize(&fresnel<Float>),
        "cos_theta_i"_a, "eta"_a, D(fresnel))
    .def("fresnel_conductor",
        vectorize(&fresnel_conductor<Float>),
        "cos_theta_i"_a, "eta"_a, D(fresnel_conductor))
    .def("fresnel_polarized",
        vectorize(py::overload_cast<Float, Complex<Float>>(&fresnel_polarized<Float>)),
        "cos_theta_i"_a, "eta"_a, D(fresnel_polarized, 2))
    .def("reflect",
        vectorize(py::overload_cast<const Vector3f &>(&reflect<Float>)),
        "wi"_a, D(reflect))
    .def("reflect",
        vectorize(py::overload_cast<const Vector3f &, const Normal3f &>(&reflect<Float>)),
        "wi"_a, "m"_a, D(reflect, 2))
    .def("refract",
        vectorize(
            py::overload_cast<const Vector3f &, Float, Float>(&refract<Float>)),
        "wi"_a, "cos_theta_t"_a, "eta_ti"_a, D(refract))
    .def("refract",
        vectorize(py::overload_cast<const Vector3f &, const Normal3f &, Float, Float>(
              &refract<Float>)),
        "wi"_a, "m"_a, "cos_theta_t"_a, "eta_ti"_a, D(refract, 2));
}
