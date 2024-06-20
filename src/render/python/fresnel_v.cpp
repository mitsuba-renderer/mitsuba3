#include <mitsuba/render/ior.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/tuple.h>
#include <nanobind/stl/string.h>

MI_PY_EXPORT(fresnel) {
    MI_PY_IMPORT_TYPES()
    m.def("fresnel",
         &fresnel<Float>,
         "cos_theta_i"_a, "eta"_a, D(fresnel))
    .def("fresnel_conductor",
         &fresnel_conductor<Float>,
         "cos_theta_i"_a, "eta"_a, D(fresnel_conductor))
    .def("fresnel_polarized",
         nb::overload_cast<Float, dr::Complex<Float>>(&fresnel_polarized<Float>),
         "cos_theta_i"_a, "eta"_a, D(fresnel_polarized, 2))
    .def("reflect",
         nb::overload_cast<const Vector3f &>(&reflect<Float>),
         "wi"_a, D(reflect))
    .def("reflect",
         nb::overload_cast<const Vector3f &, const Normal3f &>(&reflect<Float>),
         "wi"_a, "m"_a, D(reflect, 2))
    .def("refract",
         nb::overload_cast<const Vector3f &, Float, Float>(&refract<Float>),
         "wi"_a, "cos_theta_t"_a, "eta_ti"_a, D(refract))
    .def("refract",
         nb::overload_cast<const Vector3f &, const Normal3f &, Float, Float>(&refract<Float>),
         "wi"_a, "m"_a, "cos_theta_t"_a, "eta_ti"_a, D(refract, 2))
    .def("fresnel_diffuse_reflectance", &fresnel_diffuse_reflectance<Float>,
         "eta"_a, D(fresnel_diffuse_reflectance))
    .def("lookup_ior",
         [](const Properties &props, const std::string& name, nb::object def) {
            if (nb::isinstance<nb::float_>(def))
                return lookup_ior(props, name, (float) nb::cast<Properties::Float>(def));
            return lookup_ior(props, name, nb::cast<std::string>(def));
         },
         "properties"_a, "name"_a, "default"_a,
         "Lookup IOR value in table.");
}
