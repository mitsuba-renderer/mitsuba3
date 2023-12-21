#include <mitsuba/render/emitter.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyEmitter : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Emitter)

    PyEmitter(const Properties &props) : Emitter(props) { }

    std::pair<Ray3f, Spectrum>
    sample_ray(Float time, Float sample1, const Point2f &sample2,
           const Point2f &sample3, Mask active) const override {
        using Return = std::pair<Ray3f, Spectrum>;
        PYBIND11_OVERRIDE_PURE(Return, Emitter, sample_ray, time, sample1, sample2, sample3,
                               active);
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &ref,
                     const Point2f &sample,
                     Mask active) const override {
        using Return = std::pair<DirectionSample3f, Spectrum>;
        PYBIND11_OVERRIDE_PURE(Return, Emitter, sample_direction, ref, sample, active);
    }

    Float pdf_direction(const Interaction3f &ref,
                        const DirectionSample3f &ds,
                        Mask active) const override {
        PYBIND11_OVERRIDE_PURE(Float, Emitter, pdf_direction, ref, ds, active);
    }

    Spectrum eval_direction(const Interaction3f &ref,
                            const DirectionSample3f &ds,
                            Mask active)  const override {
        PYBIND11_OVERRIDE_PURE(Spectrum, Emitter, eval_direction, ref, ds, active);
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float time, const Point2f &sample,
                    Mask active) const override {
        using Return = std::pair<PositionSample3f, Float>;
        PYBIND11_OVERRIDE_PURE(Return, Emitter, sample_position, time, sample, active);
    }

    Float pdf_position(const PositionSample3f &ps,
                               Mask active) const override {
        PYBIND11_OVERRIDE_PURE(Float, Emitter, pdf_position, ps, active);
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        PYBIND11_OVERRIDE_PURE(Spectrum, Emitter, eval, si, active);
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        using Return = std::pair<Wavelength, Spectrum>;
        PYBIND11_OVERRIDE_PURE(Return, Emitter, sample_wavelengths, si, sample, active);
    }

    ScalarBoundingBox3f bbox() const override {
        PYBIND11_OVERRIDE_PURE(ScalarBoundingBox3f, Emitter, bbox,);
    }


    std::string to_string() const override {
        PYBIND11_OVERRIDE_PURE(std::string, Emitter, to_string,);
    }

    void traverse(TraversalCallback *cb) override {
        PYBIND11_OVERRIDE(void, Emitter, traverse, cb);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        PYBIND11_OVERRIDE(void, Emitter, parameters_changed, keys);
    }

    using Emitter::m_flags;
    using Emitter::m_needs_sample_2;
    using Emitter::m_needs_sample_3;
};

MI_PY_EXPORT(Emitter) {
    MI_PY_IMPORT_TYPES()
    using PyEmitter = PyEmitter<Float, Spectrum>;

    m.def("has_flag", [](uint32_t flags, EmitterFlags f) {return has_flag(flags, f);});
    m.def("has_flag", [](UInt32   flags, EmitterFlags f) {return has_flag(flags, f);});

    MI_PY_TRAMPOLINE_CLASS(PyEmitter, Emitter, Endpoint)
        .def(py::init<const Properties&>())
        .def_method(Emitter, is_environment)
        .def_method(Emitter, sampling_weight)
        .def_method(Emitter, flags, "active"_a = true)
        .def_readwrite("m_needs_sample_2", &PyEmitter::m_needs_sample_2)
        .def_readwrite("m_needs_sample_3", &PyEmitter::m_needs_sample_3)
        .def_property("m_flags",
            [](PyEmitter &emitter){ return emitter.m_flags; },
            [](PyEmitter &emitter, uint32_t flags){
                emitter.m_flags = flags;
                dr::set_attr(&emitter, "flags", flags);
            }
        );

    if constexpr (dr::is_array_v<EmitterPtr>) {
        py::object dr       = py::module_::import("drjit"),
                   dr_array = dr.attr("ArrayBase");

        py::class_<EmitterPtr> cls(m, "EmitterPtr", dr_array);

        cls.def("sample_ray",
                [](EmitterPtr ptr, Float time, Float sample1, const Point2f &sample2,
                const Point2f &sample3, Mask active) {
                    return ptr->sample_ray(time, sample1, sample2, sample3, active);
                },
                "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true,
                D(Endpoint, sample_ray))
        .def("sample_direction",
                [](EmitterPtr ptr, const Interaction3f &it, const Point2f &sample, Mask active) {
                    return ptr->sample_direction(it, sample, active);
                },
                "it"_a, "sample"_a, "active"_a = true,
                D(Endpoint, sample_direction))
        .def("pdf_direction",
                [](EmitterPtr ptr, const Interaction3f &it, const DirectionSample3f &ds, Mask active) {
                    return ptr->pdf_direction(it, ds, active);
                },
                "it"_a, "ds"_a, "active"_a = true,
                D(Endpoint, pdf_direction))
        .def("eval_direction",
                [](EmitterPtr ptr, const Interaction3f &it, const DirectionSample3f &ds, Mask active) {
                    return ptr->eval_direction(it, ds, active);
                },
                "it"_a, "ds"_a, "active"_a = true,
                D(Endpoint, eval_direction))
        .def("sample_position",
                [](EmitterPtr ptr, Float time, const Point2f &sample, Mask active) {
                    return ptr->sample_position(time, sample, active);
                },
                "time"_a, "sample"_a, "active"_a = true,
                D(Endpoint, sample_position))
        .def("pdf_position",
                [](EmitterPtr ptr, const PositionSample3f &ps, Mask active) {
                    return ptr->pdf_position(ps, active);
                },
                "ps"_a, "active"_a = true,
                D(Endpoint, pdf_position))
        .def("eval",
                [](EmitterPtr ptr, const SurfaceInteraction3f &si, Mask active) {
                    return ptr->eval(si, active);
                },
                "si"_a, "active"_a = true, D(Endpoint, eval))
        .def("sample_wavelengths",
                [](EmitterPtr ptr, const SurfaceInteraction3f &si, Float sample, Mask active) {
                    return ptr->sample_wavelengths(si, sample, active);
                },
                "si"_a, "sample"_a, "active"_a = true,
                D(Endpoint, sample_wavelengths))
        .def("flags", [](EmitterPtr ptr) { return ptr->flags(); }, D(Emitter, flags))
        .def("shape", [](EmitterPtr ptr) { return ptr->shape(); }, D(Endpoint, shape))
        .def("sampling_weight", [](EmitterPtr ptr) { return ptr->sampling_weight(); }, D(Emitter, sampling_weight))
        .def("is_environment",
             [](EmitterPtr ptr) { return ptr->is_environment(); },
             D(Emitter, is_environment));

        bind_drjit_ptr_array(cls);

        pybind11_type_alias<UInt32, dr::replace_scalar_t<UInt32, EmitterFlags>>();
    }

    MI_PY_REGISTER_OBJECT("register_emitter", Emitter)
}
