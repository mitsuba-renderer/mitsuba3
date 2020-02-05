#include <mitsuba/core/properties.h>
#include <mitsuba/python/python.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/phase.h>
#include <mitsuba/render/scene.h>

/// Trampoline for derived types implemented in Python
MTS_VARIANT class PyMedium : public Medium<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Medium, Sampler, Scene)

    PyMedium(const Properties &props) : Medium(props) {}

    std::tuple<SurfaceInteraction3f, MediumInteraction3f, Spectrum>
    sample_distance(const Scene *scene, const Ray3f &ray, const Point2f &sample, Sampler *sampler,
                    Mask active = true) const override {
        using Return = std::tuple<SurfaceInteraction3f, MediumInteraction3f, Spectrum>;
        PYBIND11_OVERLOAD_PURE(Return, Medium, sample_distance, scene, ray, sample, sampler,
                               active);
    }

    Spectrum eval_transmittance(const Ray3f &ray, Sampler *sampler,
                                Mask active = true) const override {
        PYBIND11_OVERLOAD_PURE(Spectrum, Medium, eval_transmittance, ray, sampler, active);
    }

    std::tuple<Mask, Float, Float> intersect_aabb(const Ray3f &ray) const override {
        using Return = std::tuple<Mask, Float, Float>;
        PYBIND11_OVERLOAD_PURE(Return, Medium, intersect_aabb, ray);
    }

    UnpolarizedSpectrum get_combined_extinction(const MediumInteraction3f &mi, Mask active = true) const override {
        PYBIND11_OVERLOAD_PURE(UnpolarizedSpectrum, Medium, get_combined_extinction, mi, active);
    }

    std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>
    get_scattering_coefficients(const MediumInteraction3f &mi, Mask active = true) const override {
        using Return = std::tuple<UnpolarizedSpectrum, UnpolarizedSpectrum, UnpolarizedSpectrum>;
        PYBIND11_OVERLOAD_PURE(Return, Medium, get_scattering_coefficients, mi, active);
    }

    std::string to_string() const override {
        PYBIND11_OVERLOAD_PURE(std::string, Medium, to_string, );
    }
};

MTS_PY_EXPORT(Medium) {
    MTS_PY_IMPORT_TYPES(Medium, MediumPtr, Scene, Sampler)
    using PyMedium = PyMedium<Float, Spectrum>;

    auto phase = py::class_<Medium, PyMedium, Object, ref<Medium>>(m, "Medium", D(Medium))
            .def(py::init<const Properties &>())
            .def("sample_distance", vectorize(&Medium::sample_distance), "scene"_a,
                    "ray"_a, "sample"_a, "sampler"_a, "active"_a = true,
                    D(Medium, sample_distance))
            .def("eval_transmittance", vectorize(&Medium::eval_transmittance), "ray"_a,
                    "sampler"_a, "active"_a = true, D(Medium, eval_transmittance))
            .def_method(Medium, phase_function)
            .def_method(Medium, use_emitter_sampling)
            .def_method(Medium, id)
            .def("__repr__", &Medium::to_string);

    if constexpr (is_cuda_array_v<Float>) {
        pybind11_type_alias<UInt64, MediumPtr>();
    }

    if constexpr (is_array_v<Float>) {
        phase.def_static(
            "sample_distance_vec",
            vectorize([](const MediumPtr &ptr, const Scene *scene, const Ray3f &ray,
                                const Point2f &sample, Sampler *sampler, Mask active)
                                { return ptr->sample_distance(scene, ray, sample, sampler, active); }),
            "ptr"_a, "scene"_a, "ray"_a, "sample"_a, "sampler"_a, "active"_a = true,
            D(Medium, sample_distance));
        phase.def_static("eval_transmittance_vec",
                            vectorize([](const MediumPtr &ptr, const Ray3f &ray,
                                                Sampler *sampler, Mask active)
                                                { return ptr->eval_transmittance(ray, sampler, active); }),
                            "ptr"_a, "ray"_a, "sampler"_a, "active"_a = true,
                            D(Medium, eval_transmittance));
    }

    MTS_PY_REGISTER_OBJECT("register_medium", Medium)
}
