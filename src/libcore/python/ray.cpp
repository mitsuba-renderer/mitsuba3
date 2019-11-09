#include <mitsuba/core/ray.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT_VARIANTS(Ray) {
    MTS_IMPORT_CORE_TYPES()
    using Ray3f = Ray<Point3f, Spectrum>;
    using Wavelength = wavelength_t<Spectrum>;
    MTS_PY_CHECK_ALIAS(Ray3f)

    py::class_<Ray3f>(m, "Ray3f", D(Ray3f))
        .def(py::init<>(), "Create an unitialized ray")
        .def(py::init<const Ray3f &>(), "Copy constructor", "other"_a)
        .def(py::init<Point3f, Vector3f, Float, const Wavelength &>(),
             D(Ray3f, Ray3f, 5), "o"_a, "d"_a, "time"_a, "wavelength"_a)
        .def(py::init<Point3f, Vector3f, Float, Float, Float, const Wavelength &>(),
             D(Ray3f, Ray3f, 6), "o"_a, "d"_a, "mint"_a, "maxt"_a, "time"_a, "wavelength"_a)
        .def(py::init<const Ray3f &, Float, Float>(),
             D(Ray3f, Ray3f, 7), "other"_a, "mint"_a, "maxt"_a)
        .def_method(Ray3f, update)
        .def("__call__", &Ray3f::operator(), D(Ray3f, operator, call))
        .def_field(Ray3f, o)
        .def_field(Ray3f, d)
        .def_field(Ray3f, d_rcp)
        .def_field(Ray3f, mint)
        .def_field(Ray3f, maxt)
        .def_field(Ray3f, time)
        .def_field(Ray3f, wavelength)
        .def_repr(Ray3f)
        ;

    using RayDifferential3f = RayDifferential<Point3f, Spectrum>;
    py::class_<RayDifferential3f, Ray3f>(m, "RayDifferential3f", D(RayDifferential3f))
        .def_method(RayDifferential3f, scale_differential)
        .def_field(RayDifferential3f, o_x)
        .def_field(RayDifferential3f, o_y)
        .def_field(RayDifferential3f, d_x)
        .def_field(RayDifferential3f, d_y)
        .def_field(RayDifferential3f, has_differentials)
        ;
}
