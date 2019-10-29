#include <mitsuba/render/sensor.h>
#include <mitsuba/python/python.h>
    

MTS_PY_EXPORT_VARIANTS(Sensor) {
    auto sensor = MTS_PY_CLASS(Sensor, Endpoint)
        .def("sample_ray_differential",
             py::overload_cast<Float, Float, const Point2f &, const Point2f &, bool>(
                &Sensor::sample_ray_differential, py::const_),
             D(Sensor, sample_ray_differential),
             "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "unused"_a = true)
        .mdef(Sensor, shutter_open)
        .mdef(Sensor, shutter_open_time)
        .mdef(Sensor, needs_aperture_sample)
        .mdef(Sensor, set_crop_window, "crop_size"_a, "crop_offset"_a)
        .def("film", py::overload_cast<>(&Sensor::film, py::const_), D(Sensor, film))
        .def("sampler", py::overload_cast<>(&Sensor::sampler, py::const_), D(Sensor, sampler));

    MTS_PY_CLASS(ProjectiveCamera, Sensor)
        .mdef(ProjectiveCamera, near_clip)
        .mdef(ProjectiveCamera, far_clip)
        .mdef(ProjectiveCamera, focus_distance);
}
