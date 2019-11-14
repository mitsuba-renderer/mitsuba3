#include <mitsuba/python/python.h>
#include <mitsuba/render/fresnel.h>

MTS_PY_EXPORT(fresnel) {
    MTS_IMPORT_CORE_TYPES()
    using Vector3fP = Vector<FloatP, 3>;
    using Normal3fP = Normal<FloatP, 3>;
    m.def("fresnel",
        vectorize<Float>(&fresnel<FloatP>),
        "cos_theta_i"_a, "eta"_a, D(fresnel))
    .def("fresnel_conductor",
        vectorize<Float>(&fresnel_conductor<FloatP>),
        "cos_theta_i"_a, "eta"_a, D(fresnel_conductor))
    .def("fresnel_polarized",
        vectorize<Float>(py::overload_cast<FloatP, Complex<FloatP>>(&fresnel_polarized<FloatP>)),
        "cos_theta_i"_a, "eta"_a, D(fresnel_polarized, 2))
    .def("reflect",
        vectorize<Float>(py::overload_cast<const Vector3fP &>(&reflect<FloatP>)),
        "wi"_a, D(reflect))
    .def("reflect",
        vectorize<Float>(
            py::overload_cast<const Vector3fP &, const Normal3fP &>(&reflect<FloatP>)),
        "wi"_a, "m"_a, D(reflect, 2))
    .def("refract",
        vectorize<Float>(
            py::overload_cast<const Vector3fP &, FloatP, FloatP>(&refract<FloatP>)),
        "wi"_a, "cos_theta_t"_a, "eta_ti"_a, D(refract))
    .def("refract",
        vectorize<Float>(py::overload_cast<const Vector3fP &, const Normal3fP &, FloatP, FloatP>(
              &refract<FloatP>)),
        "wi"_a, "m"_a, "cos_theta_t"_a, "eta_ti"_a, D(refract, 2));
}
