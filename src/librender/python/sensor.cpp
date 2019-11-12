#include <mitsuba/python/python.h>
#include <mitsuba/render/sensor.h>

MTS_PY_EXPORT_VARIANTS(Sensor) {
    MTS_IMPORT_TYPES()
    MTS_IMPORT_OBJECT_TYPES()
    using Base = typename Sensor::Base;
    MTS_PY_CHECK_ALIAS(Sensor, m) {
        MTS_PY_CLASS(Sensor, Base)
            .def_method(Sensor, sample_ray_differential,
                "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true)
            .def_method(Sensor, shutter_open)
            .def_method(Sensor, shutter_open_time)
            .def_method(Sensor, needs_aperture_sample)
            .def_method(Sensor, set_crop_window, "crop_size"_a, "crop_offset"_a)
            .def("film", py::overload_cast<>(&Sensor::film, py::const_), D(Sensor, film))
            .def("sampler", py::overload_cast<>(&Sensor::sampler, py::const_), D(Sensor, sampler));
    }

    using ProjectiveCamera = ProjectiveCamera<Float, Spectrum>;
    MTS_PY_CHECK_ALIAS(ProjectiveCamera, m) {
        MTS_PY_CLASS(ProjectiveCamera, Sensor)
            .def_method(ProjectiveCamera, near_clip)
            .def_method(ProjectiveCamera, far_clip)
            .def_method(ProjectiveCamera, focus_distance);
    }
}
