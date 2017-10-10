#include <mitsuba/render/sensor.h>
#include <mitsuba/python/python.h>


MTS_PY_EXPORT(Sensor) {
    auto sensor = MTS_PY_CLASS(Sensor, Endpoint)
        .def("sample_ray",
             py::overload_cast<const Point2f&, const Point2f&, Float>(
                &Sensor::sample_ray, py::const_),
             D(Sensor, sample_ray),
             "position_sample"_a, "aperture_sample"_a, "time_sample"_a)
        .def("sample_ray", enoki::vectorize_wrapper(
             py::overload_cast<const Point2fP&, const Point2fP&, FloatP>(
                &Sensor::sample_ray, py::const_)),
             D(Sensor, sample_ray),
             "position_sample"_a, "aperture_sample"_a, "time_sample"_a)

        .def("sample_ray_differential",
             py::overload_cast<const Point2f&, const Point2f&, Float>(
                &Sensor::sample_ray_differential, py::const_),
             D(Sensor, sample_ray_differential),
             "position_sample"_a, "aperture_sample"_a, "time_sample"_a)
        .def("sample_ray_differential", enoki::vectorize_wrapper(
             py::overload_cast<const Point2fP&, const Point2fP&, FloatP>(
                &Sensor::sample_ray_differential, py::const_)),
             D(Sensor, sample_ray_differential),
             "position_sample"_a, "aperture_sample"_a, "time_sample"_a)

        .def("sample_time", &Sensor::sample_time<Float>,
             D(Sensor, sample_time), "sample"_a)
        .def("sample_time", enoki::vectorize_wrapper(&Sensor::sample_time<FloatP>),
             D(Sensor, sample_time), "sample"_a)

        .def("eval",
             py::overload_cast<const SurfaceInteraction3f&, const Vector3f&>(
                &Sensor::eval, py::const_),
             D(Sensor, eval), "its"_a, "d"_a)
        .def("eval", enoki::vectorize_wrapper(
             py::overload_cast<const SurfaceInteraction3fP&, const Vector3fP&>(
                &Sensor::eval, py::const_)),
             D(Sensor, eval), "its"_a, "d"_a)

        .def("get_sample_position",
             py::overload_cast<const PositionSample3f&, const DirectionSample3f&>(
                &Sensor::get_sample_position, py::const_),
             D(Sensor, get_sample_position), "p_rec"_a, "d_rec"_a)
        .def("get_sample_position", enoki::vectorize_wrapper(
             py::overload_cast<const PositionSample3fP&, const DirectionSample3fP&>(
                &Sensor::get_sample_position, py::const_)),
             D(Sensor, get_sample_position), "p_rec"_a, "d_rec"_a)

        .def("pdf_time", &Sensor::pdf_time<Float>,
             D(Sensor, pdf_time), "ray"_a, "measure"_a)
        .def("pdf_time", enoki::vectorize_wrapper(&Sensor::pdf_time<FloatP>),
             D(Sensor, pdf_time), "ray"_a, "measure"_a)

        .def("shutter_open", &Sensor::shutter_open, D(Sensor, shutter_open))
        .def("set_shutter_open", &Sensor::set_shutter_open,
             D(Sensor, set_shutter_open), "time"_a)
        .def("shutter_open_time", &Sensor::shutter_open_time, D(Sensor, shutter_open_time))
        .def("set_shutter_open_time", &Sensor::set_shutter_open_time,
             D(Sensor, set_shutter_open_time), "time"_a)
        .def("needs_time_sample", &Sensor::needs_time_sample, D(Sensor, needs_time_sample))
        .def("needs_aperture_sample", &Sensor::needs_aperture_sample, D(Sensor, needs_aperture_sample))
        .def("film", py::overload_cast<>(&Sensor::film, py::const_), D(Sensor, film))
        .def("aspect", &Sensor::aspect, D(Sensor, aspect))
        .def("sampler", py::overload_cast<>(&Sensor::sampler, py::const_), D(Sensor, sampler))
        ;


    py::enum_<Sensor::ESensorFlags>(sensor, "ESensorFlags",
        "This list of flags is used to additionally characterize"
        " and classify the response functions of different types of sensors.",
        py::arithmetic())
        .value("EDeltaTime", Sensor::EDeltaTime)
               //D(Sensor_ESensorFlags, EDeltaTime))
        .value("ENeedsApertureSample", Sensor::ENeedsApertureSample)
               //D(Sensor_ESensorFlags, ENeedsApertureSample))
        .value("EProjectiveCamera", Sensor::EProjectiveCamera)
               //D(Sensor_ESensorFlags, EProjectiveCamera))
        .value("EPerspectiveCamera", Sensor::EPerspectiveCamera)
               //D(Sensor_ESensorFlags, EPerspectiveCamera))
        .value("EOrthographicCamera", Sensor::EOrthographicCamera)
               //D(Sensor_ESensorFlags, EOrthographicCamera))
        .value("EPositionSampleMapsToPixels", Sensor::EPositionSampleMapsToPixels)
               //D(Sensor_ESensorFlags, EPositionSampleMapsToPixels))
        .value("EDirectionSampleMapsToPixels", Sensor::EDirectionSampleMapsToPixels)
               //D(Sensor_ESensorFlags, EDirectionSampleMapsToPixels))
        .export_values();

    MTS_PY_CLASS(ProjectiveCamera, Sensor)
        .mdef(ProjectiveCamera, view_transform, "t"_a)
        // There's also the world_transform method from Endpoint
        .def("world_transform",
             py::overload_cast<Float>(
                &ProjectiveCamera::world_transform, py::const_),
             D(ProjectiveCamera, world_transform), "t"_a)

        .def("projection_transform", &ProjectiveCamera::projection_transform,
             D(ProjectiveCamera, projection_transform),
             "aperture_sample"_a, "aa_sample"_a)

        .mdef(ProjectiveCamera, near_clip)
        .mdef(ProjectiveCamera, set_near_clip, "near_clip"_a)
        .mdef(ProjectiveCamera, far_clip)
        .mdef(ProjectiveCamera, set_far_clip, "far_clip"_a)
        .mdef(ProjectiveCamera, focus_distance)
        .mdef(ProjectiveCamera, set_focus_distance, "focus_distance"_a)
        ;

    MTS_PY_CLASS(PerspectiveCamera, ProjectiveCamera)
        .mdef(PerspectiveCamera, x_fov)
        .mdef(PerspectiveCamera, set_x_fov, "x_fov"_a)
        .mdef(PerspectiveCamera, y_fov)
        .mdef(PerspectiveCamera, set_y_fov, "y_fov"_a)
        .mdef(PerspectiveCamera, diagonal_fov)
        .mdef(PerspectiveCamera, set_diagonal_fov, "diagonal_fov"_a)
        ;
}
