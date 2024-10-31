#include <mitsuba/core/ray.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>

template<typename Ray>
void bind_ray(nb::module_ &m, const char *name) {
    MI_PY_IMPORT_TYPES()
    // Re-import this specific `Ray`'s types in cased of mixed precision
    // between Float and Spectrum.
    using RayFloat       = typename Ray::Float;
    using RayScalarFloat = typename Ray::ScalarFloat;
    using RayWavelength  = typename Ray::Wavelength;
    using Vector         = typename Ray::Vector;
    using Point          = typename Ray::Point;

    MI_PY_CHECK_ALIAS(Ray, name) {
        auto ray = nb::class_<Ray>(m, name, D(Ray))
            .def(nb::init<>(), "Create an uninitialized ray")
            .def(nb::init<const Ray &>(), "Copy constructor", "other"_a)
            .def(nb::init<Point, Vector, RayFloat, const RayWavelength &>(),
                 D(Ray, Ray, 2),
                 "o"_a, "d"_a, "time"_a=(RayScalarFloat) 0.0, "wavelengths"_a=RayWavelength())
            .def(nb::init<Point, Vector, RayFloat, RayFloat, const RayWavelength &>(),
                 D(Ray, Ray, 3),
                 "o"_a, "d"_a, "maxt"_a, "time"_a, "wavelengths"_a)
            .def(nb::init<const Ray &, RayFloat>(),
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
    bind_ray<Ray<Point3d, Spectrum>>(m, "Ray3d");

    {
        auto raydiff = nb::class_<RayDifferential3f, Ray3f>(m, "RayDifferential3f", D(RayDifferential))
            .def(nb::init_implicit<Ray3f>())
            .def(nb::init<>(), "Create an uninitialized ray")
            .def(nb::init<const Ray3f &>(), "ray"_a)
            .def(nb::init<Point3f, Vector3f, Float, const Wavelength &>(),
                 "Initialize without differentials.",
                 "o"_a, "d"_a, "time"_a=(ScalarFloat) 0.0, "wavelengths"_a=Wavelength())
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

    nb::implicitly_convertible<Ray3f, RayDifferential3f>();
}
