#include <mitsuba/render/sensor.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

/// Trampoline for derived types implemented in Python
MTS_VARIANT class PySensor : public Sensor<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Sensor)

    PySensor(const Properties &props) : Sensor(props) { }

    std::pair<Ray3f, Spectrum>
    sample_ray(Float time, Float sample1, const Point2f &sample2,
           const Point2f &sample3, Mask active) const override {
        using Return = std::pair<Ray3f, Spectrum>;
        PYBIND11_OVERLOAD_PURE(Return, Sensor, sample_ray, time, sample1, sample2, sample3,
                               active);
    }

    std::pair<RayDifferential3f, Spectrum>
    sample_ray_differential(Float time, Float sample1, const Point2f &sample2,
                            const Point2f &sample3, Mask active) const override {
        using Return = std::pair<RayDifferential3f, Spectrum>;
        PYBIND11_OVERLOAD_PURE(Return, Sensor, sample_ray_differential, time, sample1, sample2, sample3,
                               active);
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &ref,
                     const Point2f &sample,
                     Mask active) const override {
        using Return = std::pair<DirectionSample3f, Spectrum>;
        PYBIND11_OVERLOAD_PURE(Return, Sensor, sample_direction, ref, sample, active);
    }

    Float pdf_direction(const Interaction3f &ref,
                        const DirectionSample3f &ds,
                        Mask active) const override {
        PYBIND11_OVERLOAD_PURE(Float, Sensor, pdf_direction, ref, ds, active);
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        PYBIND11_OVERLOAD_PURE(Spectrum, Sensor, eval, si, active);
    }

    ScalarBoundingBox3f bbox() const override {
        PYBIND11_OVERLOAD_PURE(ScalarBoundingBox3f, Sensor, bbox,);
    }

    std::string to_string() const override {
        PYBIND11_OVERLOAD_PURE(std::string, Sensor, to_string,);
    }
};

MTS_PY_EXPORT(Sensor) {
    MTS_PY_IMPORT_TYPES(Sensor, ProjectiveCamera, Endpoint)
    using PySensor = PySensor<Float, Spectrum>;

    py::class_<Sensor, PySensor, Endpoint, ref<Sensor>>(m, "Sensor", D(Sensor))
        .def(py::init<const Properties&>())
        .def("sample_ray_differential", vectorize(&Sensor::sample_ray_differential),
            "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true)
        .def_method(Sensor, shutter_open)
        .def_method(Sensor, shutter_open_time)
        .def_method(Sensor, needs_aperture_sample)
        .def("film", py::overload_cast<>(&Sensor::film, py::const_), D(Sensor, film))
        .def("sampler", py::overload_cast<>(&Sensor::sampler, py::const_), D(Sensor, sampler));

    MTS_PY_REGISTER_OBJECT("register_sensor", Sensor)

    MTS_PY_CLASS(ProjectiveCamera, Sensor)
        .def_method(ProjectiveCamera, near_clip)
        .def_method(ProjectiveCamera, far_clip)
        .def_method(ProjectiveCamera, focus_distance);
}
