#include <mitsuba/render/sensor.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PySensor : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Sensor)

    PySensor(const Properties &props) : Sensor(props) { }

    std::pair<Ray3f, Spectrum>
    sample_ray(Float time, Float sample1, const Point2f &sample2,
           const Point2f &sample3, Mask active) const override {
        using Return = std::pair<Ray3f, Spectrum>;
        PYBIND11_OVERRIDE_PURE(Return, Sensor, sample_ray, time, sample1, sample2, sample3,
                               active);
    }

    std::pair<RayDifferential3f, Spectrum>
    sample_ray_differential(Float time, Float sample1, const Point2f &sample2,
                            const Point2f &sample3, Mask active) const override {
        using Return = std::pair<RayDifferential3f, Spectrum>;
        PYBIND11_OVERRIDE(Return, Sensor, sample_ray_differential, time, sample1, sample2, sample3,
                               active);
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &ref,
                     const Point2f &sample,
                     Mask active) const override {
        using Return = std::pair<DirectionSample3f, Spectrum>;
        PYBIND11_OVERRIDE_PURE(Return, Sensor, sample_direction, ref, sample, active);
    }

    Float pdf_direction(const Interaction3f &ref,
                        const DirectionSample3f &ds,
                        Mask active) const override {
        PYBIND11_OVERRIDE_PURE(Float, Sensor, pdf_direction, ref, ds, active);
    }

    Spectrum eval_direction(const Interaction3f &ref,
                            const DirectionSample3f &ds,
                            Mask active)  const override {
        PYBIND11_OVERRIDE_PURE(Spectrum, Sensor, eval_direction, ref, ds, active);
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f &sample,
                    Mask active) const override {
        using Return = std::pair<PositionSample3f, Float>;
        PYBIND11_OVERRIDE_PURE(Return, Sensor, sample_position, time, sample, active);
    }

    Float pdf_position(const PositionSample3f &ps,
                               Mask active) const override {
        PYBIND11_OVERRIDE_PURE(Float, Sensor, pdf_position, ps, active);
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        PYBIND11_OVERRIDE_PURE(Spectrum, Sensor, eval, si, active);
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        using Return = std::pair<Wavelength, Spectrum>;
        PYBIND11_OVERRIDE(Return, Sensor, sample_wavelengths, si, sample, active);
    }

    ScalarBoundingBox3f bbox() const override {
        PYBIND11_OVERRIDE_PURE(ScalarBoundingBox3f, Sensor, bbox,);
    }

    std::string to_string() const override {
        PYBIND11_OVERRIDE_PURE(std::string, Sensor, to_string,);
    }

    using Sensor::m_needs_sample_2;
    using Sensor::m_needs_sample_3;
};

MI_PY_EXPORT(Sensor) {
    MI_PY_IMPORT_TYPES(Sensor, ProjectiveCamera, Endpoint)
    using PySensor = PySensor<Float, Spectrum>;

    MI_PY_TRAMPOLINE_CLASS(PySensor, Sensor, Endpoint)
        .def(py::init<const Properties&>())
        .def("sample_ray_differential", &Sensor::sample_ray_differential,
            "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true)
        .def_method(Sensor, shutter_open)
        .def_method(Sensor, shutter_open_time)
        .def_method(Sensor, needs_aperture_sample)
        .def("film", py::overload_cast<>(&Sensor::film, py::const_), D(Sensor, film))
        .def("sampler", py::overload_cast<>(&Sensor::sampler, py::const_), D(Sensor, sampler))
        .def_readwrite("m_needs_sample_2", &PySensor::m_needs_sample_2)
        .def_readwrite("m_needs_sample_3", &PySensor::m_needs_sample_3);

    MI_PY_REGISTER_OBJECT("register_sensor", Sensor)

    MI_PY_CLASS(ProjectiveCamera, Sensor)
        .def_method(ProjectiveCamera, near_clip)
        .def_method(ProjectiveCamera, far_clip)
        .def_method(ProjectiveCamera, focus_distance);

    m.def("perspective_projection", &perspective_projection<Float>,
          "film_size"_a, "crop_size"_a, "crop_offset"_a, "fov_x"_a, "near_clip"_a, "far_clip"_a,
          D(perspective_projection));

    if constexpr (dr::is_array_v<SensorPtr>) {
        py::object dr       = py::module_::import("drjit"),
                   dr_array = dr.attr("ArrayBase");

        py::class_<SensorPtr> cls(m, "SensorPtr", dr_array);

        cls.def("sample_ray",
                [](SensorPtr ptr, Float time, Float sample1, const Point2f &sample2,
                const Point2f &sample3, Mask active) {
                    return ptr->sample_ray(time, sample1, sample2, sample3, active);
                },
                "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true,
                D(Endpoint, sample_ray))
        .def("sample_direction",
                [](SensorPtr ptr, const Interaction3f &it, const Point2f &sample, Mask active) {
                    return ptr->sample_direction(it, sample, active);
                },
                "it"_a, "sample"_a, "active"_a = true,
                D(Endpoint, sample_direction))
        .def("pdf_direction",
                [](SensorPtr ptr, const Interaction3f &it, const DirectionSample3f &ds, Mask active) {
                    return ptr->pdf_direction(it, ds, active);
                },
                "it"_a, "ds"_a, "active"_a = true,
                D(Endpoint, pdf_direction))
        .def("eval_direction",
                [](SensorPtr ptr, const Interaction3f &it, const DirectionSample3f &ds, Mask active) {
                    return ptr->eval_direction(it, ds, active);
                },
                "it"_a, "ds"_a, "active"_a = true,
                D(Endpoint, eval_direction))
        .def("sample_position",
                [](SensorPtr ptr, Float time, const Point2f &sample, Mask active) {
                    return ptr->sample_position(time, sample, active);
                },
                "time"_a, "sample"_a, "active"_a = true,
                D(Endpoint, sample_position))
        .def("pdf_position",
                [](SensorPtr ptr, const PositionSample3f &ps, Mask active) {
                    return ptr->pdf_position(ps, active);
                },
                "ps"_a, "active"_a = true,
                D(Endpoint, pdf_position))
        .def("eval",
                [](SensorPtr ptr, const SurfaceInteraction3f &si, Mask active) {
                    return ptr->eval(si, active);
                },
                "si"_a, "active"_a = true, D(Endpoint, eval))
        .def("sample_wavelengths",
                [](SensorPtr ptr, const SurfaceInteraction3f &si, Float sample, Mask active) {
                    return ptr->sample_wavelengths(si, sample, active);
                },
                "si"_a, "sample"_a, "active"_a = true,
                D(Endpoint, sample_wavelengths))
        .def("shape", [](SensorPtr ptr) { return ptr->shape(); }, D(Endpoint, shape));

        bind_drjit_ptr_array(cls);
    }

    MI_PY_REGISTER_OBJECT("register_sensor", Sensor)
}
