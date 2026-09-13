#include <mitsuba/core/ray.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/optional.h>

template <typename RayCone>
void bind_ray_cone(nb::module_ &m, const char *name) {
    using Float = typename RayCone::Float;
    using Mask  = dr::mask_t<Float>;

    MI_PY_CHECK_ALIAS(RayCone, name) {
        auto cone = nb::class_<RayCone>(m, name, D(RayCone))
            .def(nb::init<>(), "Create a zero ray cone")
            .def(nb::init<const RayCone &>(), "Copy constructor", "other"_a)
            .def(nb::init<const Float &, const Float &>(),
                 "width"_a, "spread"_a, D(RayCone, RayCone))
            .def("propagate", &RayCone::propagate, "t"_a, D(RayCone, propagate))
            .def("scale", &RayCone::scale, "s"_a, D(RayCone, scale))
            .def_field(RayCone, width,  D(RayCone, width))
            .def_field(RayCone, spread, D(RayCone, spread))
            .def_repr(RayCone);

        if constexpr (dr::is_jit_v<Float>) {
            MI_PY_DRJIT_STRUCT(cone, RayCone, width, spread);
        }
    }
}

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
            .def("__init__",
                [](Ray *ray, const Point& o, const Vector& d, RayFloat time,
                   const std::optional<RayWavelength> wavelengths_) {
                    RayWavelength wavelengths = wavelengths_.has_value() ?
                                                wavelengths_.value() :
                                                dr::zeros<RayWavelength>();
                    new (ray) Ray(o, d, time, wavelengths);
                },
                D(Ray, Ray, 2),
                "o"_a, "d"_a, "time"_a=(RayScalarFloat) 0.0, "wavelengths"_a=nb::none()
            )
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
            .def_field(Ray, cone,        D(Ray, cone))
            .def_repr(Ray);

        if constexpr (dr::is_jit_v<RayFloat>) {
            MI_PY_DRJIT_STRUCT(ray, Ray, o, d, maxt, time, wavelengths, cone);
        }
    }
}

MI_PY_EXPORT(Ray) {
    MI_PY_IMPORT_TYPES()

    bind_ray_cone<RayCone<Float>>(m, "RayCone");
    bind_ray_cone<RayCone<ScalarFloat>>(m, "ScalarRayCone");

    bind_ray<Ray<Point2f, Spectrum>>(m, "Ray2f");
    bind_ray<Ray3f>(m, "Ray3f");

    using ScalarSpectrum = scalar_spectrum_t<Spectrum>;
    bind_ray<Ray<ScalarPoint2f, ScalarSpectrum>>(m, "ScalarRay2f");
    bind_ray<Ray<ScalarPoint3f, ScalarSpectrum>>(m, "ScalarRay3f");
}
