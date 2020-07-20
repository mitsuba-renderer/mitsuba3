#include <mitsuba/render/emitter.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>

/// Trampoline for derived types implemented in Python
MTS_VARIANT class PyEmitter : public Emitter<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Emitter)

    PyEmitter(const Properties &props) : Emitter(props) { }

    std::pair<Ray3f, Spectrum>
    sample_ray(Float time, Float sample1, const Point2f &sample2,
           const Point2f &sample3, Mask active) const override {
        using Return = std::pair<Ray3f, Spectrum>;
        PYBIND11_OVERLOAD_PURE(Return, Emitter, sample_ray, time, sample1, sample2, sample3,
                               active);
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &ref,
                     const Point2f &sample,
                     Mask active) const override {
        using Return = std::pair<DirectionSample3f, Spectrum>;
        PYBIND11_OVERLOAD_PURE(Return, Emitter, sample_direction, ref, sample, active);
    }

    Float pdf_direction(const Interaction3f &ref,
                        const DirectionSample3f &ds,
                        Mask active) const override {
        PYBIND11_OVERLOAD_PURE(Float, Emitter, pdf_direction, ref, ds, active);
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        PYBIND11_OVERLOAD_PURE(Spectrum, Emitter, eval, si, active);
    }

    ScalarBoundingBox3f bbox() const override {
        PYBIND11_OVERLOAD_PURE(ScalarBoundingBox3f, Emitter, bbox,);
    }


    std::string to_string() const override {
        PYBIND11_OVERLOAD_PURE(std::string, Emitter, to_string,);
    }
};

MTS_PY_EXPORT(Emitter) {
    MTS_PY_IMPORT_TYPES()
    using PyEmitter = PyEmitter<Float, Spectrum>;

    auto emitter = py::class_<Emitter, PyEmitter, Endpoint, ref<Emitter>>(m, "Emitter", D(Emitter))
        .def(py::init<const Properties&>())
        .def_method(Emitter, is_environment)
        .def_method(Emitter, flags);

    if constexpr (is_cuda_array_v<Float>)
        pybind11_type_alias<UInt64, EmitterPtr>();

    if constexpr (is_array_v<Float>) {
        emitter.def_static("sample_ray_vec",
                            vectorize([](const EmitterPtr &ptr, Float time, Float sample1,
                                         const Point2f &sample2, const Point2f &sample3,
                                         Mask active) {
                                 return ptr->sample_ray(time, sample1, sample2, sample3, active);
                            }),
                            "ptr"_a, "time"_a, "sample1"_a, "sample2"_a, "sample3"_a,
                            "active"_a = true,
                            D(Endpoint, sample_ray));
        emitter.def_static("sample_direction_vec",
                            vectorize([](const EmitterPtr &ptr, const Interaction3f &it,
                                         const Point2f &sample, Mask active) {
                                return ptr->sample_direction(it, sample, active);
                            }),
                            "ptr"_a, "it"_a, "sample"_a, "active"_a = true,
                            D(Endpoint, sample_direction));
        emitter.def_static("pdf_direction_vec",
                            vectorize([](const EmitterPtr &ptr, const Interaction3f &it,
                                         const DirectionSample3f &ds, Mask active) {
                                return ptr->pdf_direction(it, ds, active);
                            }),
                            "ptr"_a, "it"_a, "ds"_a, "active"_a = true,
                            D(Endpoint, pdf_direction));
        emitter.def_static("eval_vec",
                           vectorize([](const EmitterPtr &ptr, const SurfaceInteraction3f &si,
                                        Mask active) { return ptr->eval(si, active); }),
                           "ptr"_a, "si"_a, "active"_a = true, D(Endpoint, eval));
    }

    MTS_PY_REGISTER_OBJECT("register_emitter", Emitter)
}
