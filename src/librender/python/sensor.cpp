#include <mitsuba/render/sensor.h>
#include <mitsuba/python/python.h>


MTS_PY_EXPORT(Sensor) {
    auto sensor = MTS_PY_CLASS(Sensor, Endpoint)
        .def("sample_ray_differential",
             py::overload_cast<Float, Float, const Point2f &, const Point2f &, bool>(
                &Sensor::sample_ray_differential, py::const_),
             D(Sensor, sample_ray_differential),
             "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "unused"_a = true)
        .def("sample_ray_differential",
             enoki::vectorize_wrapper(
                py::overload_cast<FloatP, FloatP, const Point2fP &, const Point2fP &, MaskP>(
                    &Sensor::sample_ray_differential, py::const_)),
             "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true)
#if defined(MTS_ENABLE_AUTODIFF)
        .def("sample_ray_differential",
            py::overload_cast<FloatD, FloatD, const Point2fD &, const Point2fD &, MaskD>(
                &Sensor::sample_ray_differential, py::const_),
             "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true)
#endif
        .def("sample_ray_differential_pol",
            py::overload_cast<Float, Float, const Point2f &, const Point2f &, bool>(
                &Sensor::sample_ray_differential_pol, py::const_),
            D(Sensor, sample_ray_differential_pol),
            "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "unused"_a = true)
        .def("sample_ray_differential_pol",
            enoki::vectorize_wrapper(
                py::overload_cast<FloatP, FloatP, const Point2fP &, const Point2fP &, MaskP>(
                    &Sensor::sample_ray_differential_pol, py::const_)),
            "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true)
        .mdef(Sensor, shutter_open)
        .mdef(Sensor, shutter_open_time)
        .mdef(Sensor, needs_aperture_sample)
        .def("film", py::overload_cast<>(&Sensor::film, py::const_), D(Sensor, film))
        .def("sampler", py::overload_cast<>(&Sensor::sampler, py::const_), D(Sensor, sampler));

    MTS_PY_CLASS(ProjectiveCamera, Sensor)
        .mdef(ProjectiveCamera, near_clip)
        .mdef(ProjectiveCamera, far_clip)
        .mdef(ProjectiveCamera, focus_distance);
}
