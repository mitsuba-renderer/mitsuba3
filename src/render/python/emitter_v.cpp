#include <mitsuba/render/emitter.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <nanobind/trampoline.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <drjit/python.h>

/// Trampoline for derived types implemented in Python
MI_VARIANT class PyEmitter : public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_TYPES(Emitter, Scene, Medium, Shape)
    NB_TRAMPOLINE(Emitter, 16);

    PyEmitter(const Properties &props) : Emitter(props) { }

    std::pair<Ray3f, Spectrum>
    sample_ray(Float time, Float sample1, const Point2f &sample2,
           const Point2f &sample3, Mask active) const override {
        NB_OVERRIDE_PURE(sample_ray, time, sample1, sample2, sample3, active);
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &ref,
                     const Point2f &sample,
                     Mask active) const override {
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
        NB_OVERRIDE_PURE(sample_wavelengths, si, sample, active);
    }

    Spectrum pdf_wavelengths(const Spectrum &wavelengths,
                             Mask active = true) const override {
        NB_OVERRIDE_PURE(pdf_wavelengths, wavelengths, active);
    };

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

    using Emitter::m_flags;
    using Emitter::m_needs_sample_2;
    using Emitter::m_needs_sample_3;
};

template <typename Ptr, typename Cls> void bind_emitter_generic(Cls &cls) {
    MI_PY_IMPORT_TYPES()

    using RetShape = std::conditional_t<drjit::is_array_v<Ptr>, ShapePtr, drjit::scalar_t<ShapePtr>>;
    using RetMedium = std::conditional_t<drjit::is_array_v<Ptr>, MediumPtr, drjit::scalar_t<MediumPtr>>;

    cls.def("sample_ray",
            [](Ptr ptr, Float time, Float sample1, const Point2f &sample2,
            const Point2f &sample3, Mask active) {
                return ptr->sample_ray(time, sample1, sample2, sample3, active);
            },
            "time"_a, "sample1"_a, "sample2"_a, "sample3"_a, "active"_a = true,
            D(Endpoint, sample_ray))
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
    .def("flags", [](Ptr ptr) { return ptr->flags(); }, D(Emitter, flags))
    .def("get_shape", [](Ptr ptr) -> RetShape { return ptr->shape(); }, D(Endpoint, shape))
    .def("get_medium", [](Ptr ptr) -> RetMedium { return ptr->medium(); }, D(Endpoint, medium))
    .def("sampling_weight", [](Ptr ptr) { return ptr->sampling_weight(); }, D(Emitter, sampling_weight))
    .def("is_environment", [](Ptr ptr) { return ptr->is_environment(); }, D(Emitter, is_environment));
}

MI_PY_EXPORT(Emitter) {
    MI_PY_IMPORT_TYPES(Emitter, EmitterPtr)
    using PyEmitter = PyEmitter<Float, Spectrum>;
    using Properties = PropertiesV<Float>;

    m.def("has_flag", [](uint32_t flags, EmitterFlags f) {return has_flag(flags, f);});
    m.def("has_flag", [](UInt32   flags, EmitterFlags f) {return has_flag(flags, f);});

    auto emitter = MI_PY_TRAMPOLINE_CLASS(PyEmitter, Emitter, Endpoint)
        .def(nb::init<const Properties&>(), "props"_a)
        .def_method(Emitter, is_environment)
        .def_method(Emitter, sampling_weight)
        .def_method(Emitter, flags, "active"_a = true)
        .def_field(PyEmitter, m_needs_sample_2, D(Endpoint, m_needs_sample_2))
        .def_field(PyEmitter, m_needs_sample_3, D(Endpoint, m_needs_sample_3))
        .def_field(PyEmitter, m_flags, D(Emitter, m_flags));

    if constexpr (dr::is_array_v<EmitterPtr>) {
        dr::ArrayBinding b;
        auto emitter_ptr = dr::bind_array_t<EmitterPtr>(b, m, "EmitterPtr");
        bind_emitter_generic<EmitterPtr>(emitter_ptr);
    }

    MI_PY_REGISTER_OBJECT("register_emitter", Emitter)
}
