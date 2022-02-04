#include <mitsuba/core/ray.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT(Ray) {
    MTS_PY_IMPORT_TYPES()
    {
        auto ray = py::class_<Ray3f>(m, "Ray3f", D(Ray))
            .def(py::init<>(), "Create an unitialized ray")
            .def(py::init<const Ray3f &>(), "Copy constructor", "other"_a)
            .def(py::init<Point3f, Vector3f, Float, const Wavelength &>(),
                 D(Ray, Ray, 5),
                 "o"_a, "d"_a, "time"_a=0.0, "wavelengths"_a=Wavelength())
            .def(py::init<Point3f, Vector3f, Float, Float, const Wavelength &>(),
                 D(Ray, Ray, 6),
                "o"_a, "d"_a, "maxt"_a, "time"_a, "wavelengths"_a)
            .def(py::init<const Ray3f &, Float>(),
                D(Ray, Ray, 7), "other"_a, "maxt"_a)
            .def("__call__", &Ray3f::operator(), D(Ray, operator, call), "t"_a)
            .def_field(Ray3f, o,           D(Ray, o))
            .def_field(Ray3f, d,           D(Ray, d))
            .def_field(Ray3f, maxt,        D(Ray, maxt))
            .def_field(Ray3f, time,        D(Ray, time))
            .def_field(Ray3f, wavelengths, D(Ray, wavelengths))
            .def_repr(Ray3f);

        MTS_PY_DRJIT_STRUCT(ray, Ray3f, o, d, maxt, time, wavelengths)
    }

    {
        auto raydiff = py::class_<RayDifferential3f, Ray3f>(m, "RayDifferential3f", D(RayDifferential))
            .def(py::init<>(), "Create an unitialized ray")
            .def(py::init<const Ray3f &>(), "ray"_a)
            .def(py::init<Point3f, Vector3f, Float, const Wavelength &>(),
                 "Initialize without differentials.",
                 "o"_a, "d"_a, "time"_a=0.0, "wavelengths"_a=Wavelength())
            .def("scale_differential", &RayDifferential3f::scale_differential,
                 "amount"_a, D(RayDifferential, scale_differential))
            .def_field(RayDifferential3f, o_x, D(RayDifferential, o_x))
            .def_field(RayDifferential3f, o_y, D(RayDifferential, o_y))
            .def_field(RayDifferential3f, d_x, D(RayDifferential, d_x))
            .def_field(RayDifferential3f, d_y, D(RayDifferential, d_y))
            .def_field(RayDifferential3f, has_differentials, D(RayDifferential, has_differentials));

        MTS_PY_DRJIT_STRUCT(raydiff, RayDifferential3f, o, d, maxt, time,
                            wavelengths, o_x, o_y, d_x, d_y)
    }

    py::implicitly_convertible<Ray3f, RayDifferential3f>();
}
