#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/render/sensor.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/pair.h>
#include <drjit/python.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PySensor : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Sensor)
    NB_TRAMPOLINE(Sensor, 13);

    PySensor(const Properties &props) : Sensor(props) { }

    std::pair<Ray3f, Spectrum>
    sample_ray(Float time, Float sample1, const Point2f &sample2,
               const Point2f &sample3, Mask active) const override {
        using Return = std::pair<Ray3f, Spectrum>;
        NB_OVERRIDE_PURE(sample_ray, time, sample1, sample2, sample3, active);
    }

    std::pair<RayDifferential3f, Spectrum>
    sample_ray_differential(Float time, Float sample1, const Point2f &sample2,
                            const Point2f &sample3, Mask active) const override {
        using Return = std::pair<RayDifferential3f, Spectrum>;
        NB_OVERRIDE(sample_ray_differential, time, sample1, sample2, sample3, active);
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &ref,
                     const Point2f &sample,
                     Mask active) const override {
        using Return = std::pair<DirectionSample3f, Spectrum>;
        NB_OVERRIDE_PURE(sample_direction, ref, sample, active);
    }

    Float pdf_direction(const Interaction3f &ref,
                        const DirectionSample3f &ds,
                        Mask active) const override {
        NB_OVERRIDE_PURE(pdf_direction, ref, ds, active);
    }

    Spectrum eval_direction(const Interaction3f &ref,
                            const DirectionSample3f &ds,
                            Mask active)  const override {
        NB_OVERRIDE_PURE(eval_direction, ref, ds, active);
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f &sample,
                    Mask active) const override {
        using Return = std::pair<PositionSample3f, Float>;
        NB_OVERRIDE_PURE(sample_position, time, sample, active);
    }

    Float pdf_position(const PositionSample3f &ps,
                               Mask active) const override {
        NB_OVERRIDE_PURE(pdf_position, ps, active);
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        NB_OVERRIDE_PURE(eval, si, active);
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        using Return = std::pair<Wavelength, Spectrum>;
        NB_OVERRIDE(sample_wavelengths, si, sample, active);
    }

    ScalarBoundingBox3f bbox() const override {
        NB_OVERRIDE_PURE(bbox);
    }

    std::string to_string() const override {
        NB_OVERRIDE_PURE(to_string);
    }

    void traverse(TraversalCallback *cb) override {
        NB_OVERRIDE(traverse, cb);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        NB_OVERRIDE(parameters_changed, keys);
    }

    using Sensor::m_needs_sample_2;
    using Sensor::m_needs_sample_3;
    using Sensor::m_film;
};

template <typename Ptr, typename Cls> void bind_sensor_generic(Cls &cls) {
    MI_PY_IMPORT_TYPES()

    using RetShape = std::conditional_t<drjit::is_array_v<Ptr>, ShapePtr, drjit::scalar_t<ShapePtr>>;

    cls.def("sample_ray",
            [](Ptr ptr, Float time, Float sample1, const Point2f &sample2,
               const Point2f &sample3, Mask active) {
                return ptr->sample_ray(time, sample1, sample2, sample3, active);
            },
            "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true,
            D(Endpoint, sample_ray))
    .def("sample_ray_differential",
            [](Ptr ptr, Float time, Float sample1, const Point2f &sample2,
               const Point2f &sample3, Mask active) {
                return ptr->sample_ray_differential(time, sample1, sample2, sample3, active);
            },
            "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true,
            D(Sensor, sample_ray_differential))
    .def("sample_direction",
            [](Ptr ptr, const Interaction3f &it, const Point2f &sample, Mask active) {
                return ptr->sample_direction(it, sample, active);
            },
            "it"_a, "sample"_a, "active"_a = true,
            D(Endpoint, sample_direction))
    .def("pdf_direction",
            [](Ptr ptr, const Interaction3f &it, const DirectionSample3f &ds, Mask active) {
                return ptr->pdf_direction(it, ds, active);
            },
            "it"_a, "ds"_a, "active"_a = true,
            D(Endpoint, pdf_direction))
    .def("eval_direction",
            [](Ptr ptr, const Interaction3f &it, const DirectionSample3f &ds, Mask active) {
                return ptr->eval_direction(it, ds, active);
            },
            "it"_a, "ds"_a, "active"_a = true,
            D(Endpoint, eval_direction))
    .def("sample_position",
            [](Ptr ptr, Float time, const Point2f &sample, Mask active) {
                return ptr->sample_position(time, sample, active);
            },
            "time"_a, "sample"_a, "active"_a = true,
            D(Endpoint, sample_position))
    .def("pdf_position",
            [](Ptr ptr, const PositionSample3f &ps, Mask active) {
                return ptr->pdf_position(ps, active);
            },
            "ps"_a, "active"_a = true,
            D(Endpoint, pdf_position))
    .def("eval",
            [](Ptr ptr, const SurfaceInteraction3f &si, Mask active) {
                return ptr->eval(si, active);
            },
            "si"_a, "active"_a = true, D(Endpoint, eval))
    .def("sample_wavelengths",
            [](Ptr ptr, const SurfaceInteraction3f &si, Float sample, Mask active) {
                return ptr->sample_wavelengths(si, sample, active);
            },
            "si"_a, "sample"_a, "active"_a = true,
            D(Endpoint, sample_wavelengths))
    .def("get_shape", [](Ptr ptr) -> RetShape {
                return ptr->shape();
            },
            D(Endpoint, shape));
}

MI_PY_EXPORT(Sensor) {
    MI_PY_IMPORT_TYPES(Sensor, ProjectiveCamera, Endpoint)
    using PySensor = PySensor<Float, Spectrum>;
    using Properties = PropertiesV<Float>;

    auto sensor = MI_PY_TRAMPOLINE_CLASS(PySensor, Sensor, Endpoint)
        .def(nb::init<const Properties&>())
        .def_method(Sensor, shutter_open)
        .def_method(Sensor, shutter_open_time)
        .def_method(Sensor, needs_aperture_sample)
        .def("film", nb::overload_cast<>(&Sensor::film, nb::const_), D(Sensor, film))
        .def("sampler", nb::overload_cast<>(&Sensor::sampler, nb::const_), D(Sensor, sampler))
        .def_field(PySensor, m_needs_sample_2, D(Endpoint, m_needs_sample_3))
        .def_field(PySensor, m_needs_sample_3, D(Endpoint, m_needs_sample_3))
        .def_field(PySensor, m_film);

    bind_sensor_generic<Sensor *>(sensor);

    if constexpr (dr::is_array_v<SensorPtr>) {
        dr::ArrayBinding b;
        auto sensor_ptr = dr::bind_array_t<SensorPtr>(b, m, "SensorPtr");
        bind_sensor_generic<SensorPtr>(sensor_ptr);
    }

    MI_PY_REGISTER_OBJECT("register_sensor", Sensor)

    MI_PY_CLASS(ProjectiveCamera, Sensor)
        .def_method(ProjectiveCamera, near_clip)
        .def_method(ProjectiveCamera, far_clip)
        .def_method(ProjectiveCamera, focus_distance);

    m.def("perspective_projection", &perspective_projection<Float>,
          "film_size"_a, "crop_size"_a, "crop_offset"_a, "fov_x"_a, "near_clip"_a, "far_clip"_a,
          D(perspective_projection));

    m.def("orthographic_projection", &orthographic_projection<Float>,
          "film_size"_a, "crop_size"_a, "crop_offset"_a, "near_clip"_a, "far_clip"_a,
          D(orthographic_projection));
}
