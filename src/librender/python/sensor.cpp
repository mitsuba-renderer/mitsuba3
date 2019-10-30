#include <mitsuba/render/sensor.h>
#include <mitsuba/python/python.h>

MTS_PY_EXPORT_VARIANTS(Sensor) {
    using Base = typename Sensor::Base;
    MTS_PY_CLASS(Sensor, Base)
        .mdef(Sensor, sample_ray_differential,
              "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true)
        .mdef(Sensor, shutter_open)
        .mdef(Sensor, shutter_open_time)
        .mdef(Sensor, needs_aperture_sample)
        .mdef(Sensor, set_crop_window, "crop_size"_a, "crop_offset"_a)
        .def("film", py::overload_cast<>(&Sensor::film, py::const_), D(Sensor, film))
        .def("sampler", py::overload_cast<>(&Sensor::sampler, py::const_), D(Sensor, sampler));

    using ProjectiveCamera = ProjectiveCamera<Float, Spectrum>;
    MTS_PY_CLASS(ProjectiveCamera, Sensor)
        .mdef(ProjectiveCamera, near_clip)
        .mdef(ProjectiveCamera, far_clip)
        .mdef(ProjectiveCamera, focus_distance);
}
