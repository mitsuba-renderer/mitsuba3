#include <mitsuba/core/ray.h>
#include <mitsuba/python/python.h>

template<typename Ray>
void bind_ray(py::module &m, const char *name) {
    MI_PY_IMPORT_TYPES()
    using Vector = typename Ray::Vector;
    using Point  = typename Ray::Point;

    MI_PY_CHECK_ALIAS(Ray, name) {
        auto ray = py::class_<Ray>(m, name, D(Ray))
            .def(py::init<>(), "Create an uninitialized ray")
            .def(py::init<const Ray &>(), "Copy constructor", "other"_a)
            .def(py::init<Point, Vector, Float, const Wavelength &>(),
                 D(Ray, Ray, 2),
                 "o"_a, "d"_a, "time"_a=0.0, "wavelengths"_a=Wavelength())
            .def(py::init<Point, Vector, Float, Float, const Wavelength &>(),
                 D(Ray, Ray, 3),
                "o"_a, "d"_a, "maxt"_a, "time"_a, "wavelengths"_a)
            .def(py::init<const Ray &, Float>(),
                D(Ray, Ray, 4), "other"_a, "maxt"_a)
            .def("__call__", &Ray::operator(), D(Ray, operator, call), "t"_a)
            .def_field(Ray, o,           D(Ray, o))
            .def_field(Ray, d,           D(Ray, d))
            .def_field(Ray, maxt,        D(Ray, maxt))
            .def_field(Ray, time,        D(Ray, time))
            .def_field(Ray, wavelengths, D(Ray, wavelengths))
            .def_repr(Ray);
        MI_PY_DRJIT_STRUCT(ray, Ray, o, d, maxt, time, wavelengths)
    }
}

MI_PY_EXPORT(Ray) {
    MI_PY_IMPORT_TYPES()

    bind_ray<Ray<Point2f, Spectrum>>(m, "Ray2f");
    bind_ray<Ray3f>(m, "Ray3f");

    {
        auto raydiff = py::class_<RayDifferential3f, Ray3f>(m, "RayDifferential3f", D(RayDifferential))
            .def(py::init<>(), "Create an uninitialized ray")
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

        MI_PY_DRJIT_STRUCT(raydiff, RayDifferential3f, o, d, maxt, time,
                            wavelengths, o_x, o_y, d_x, d_y)
    }

    py::implicitly_convertible<Ray3f, RayDifferential3f>();
}
