#if defined(MI_ENABLE_CUDA) && defined(MI_ENABLE_DLSS)

#include <nanobind/nanobind.h>
#include <mitsuba/render/dlssdenoiser.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/string.h>

MI_PY_EXPORT(DLSSDenoiser) {
    MI_PY_IMPORT_TYPES(DLSSDenoiser)
    MI_PY_CLASS(DLSSDenoiser, Object)
        .def_static("is_available", &DLSSDenoiser::is_available,
                    D(DLSSDenoiser, is_available))
        .def(nb::init<const ScalarVector2u &, const ScalarVector2u &,
                      const std::string &>(),
             "input_size"_a, "output_size"_a = ScalarVector2u(0, 0),
             "quality"_a = "high", D(DLSSDenoiser, DLSSDenoiser))
        .def(
            "__call__",
            [](const DLSSDenoiser &denoiser, const TensorXf &noisy,
               const TensorXf &albedo, const TensorXf &normals,
               const TensorXf &depth, const TensorXf &specular_albedo,
               const TensorXf &roughness, const TensorXf &flow,
               const TensorXf &specular_flow, const nb::object &transform,
               const ScalarPoint2f &jitter, bool reset) {
                AffineTransform4f to_sensor;
                if (!transform.is(nb::none()))
                    to_sensor = nb::cast<AffineTransform4f>(transform);

                return denoiser(noisy, albedo, normals, depth, specular_albedo,
                                roughness, flow, specular_flow, to_sensor,
                                jitter, reset);
            },
            "noisy"_a, "albedo"_a, "normals"_a, "depth"_a,
            "specular_albedo"_a = TensorXf(), "roughness"_a = TensorXf(),
            "flow"_a = TensorXf(), "specular_flow"_a = TensorXf(),
            "to_sensor"_a = nb::none(), "jitter"_a = ScalarPoint2f(0.f),
            "reset"_a = false, D(DLSSDenoiser, operator_call))
        .def(
            "__call__",
            [](const DLSSDenoiser &denoiser, const ref<Bitmap> &noisy,
               const std::string &albedo_ch, const std::string &normals_ch,
               const std::string &depth_ch,
               const std::string &specular_albedo_ch,
               const std::string &roughness_ch, const std::string &flow_ch,
               const std::string &specular_flow_ch,
               const nb::object &transform, const ScalarPoint2f &jitter,
               bool reset, const std::string &noisy_ch) {
                AffineTransform4f to_sensor;
                if (!transform.is(nb::none()))
                    to_sensor = nb::cast<AffineTransform4f>(transform);

                return denoiser(noisy, albedo_ch, normals_ch, depth_ch,
                                specular_albedo_ch, roughness_ch, flow_ch,
                                specular_flow_ch, to_sensor, jitter, reset,
                                noisy_ch);
            },
            "noisy"_a, "albedo_ch"_a, "normals_ch"_a, "depth_ch"_a,
            "specular_albedo_ch"_a = "", "roughness_ch"_a = "",
            "flow_ch"_a = "", "specular_flow_ch"_a = "",
            "to_sensor"_a = nb::none(), "jitter"_a = ScalarPoint2f(0.f),
            "reset"_a = false, "noisy_ch"_a = "<root>",
            D(DLSSDenoiser, operator_call, 2));
}

#else // defined(MI_ENABLE_CUDA) && defined(MI_ENABLE_DLSS)

#include <nanobind/nanobind.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/python/python.h>
#include <drjit/tensor.h>

#include <nanobind/stl/string.h>

NAMESPACE_BEGIN(mitsuba)

// Stand-in for the denoiser on builds without DLSS support. It mirrors the
// interface of the real class so that the generated documentation does not
// depend on the platform it was produced on, and raises an exception when used.
template <typename Float, typename Spectrum> struct DLSSDenoiserStub : Object {
    [[noreturn]] static void fail() {
        Throw("DLSSDenoiser is only available in CUDA-enabled builds of "
              "Mitsuba which were compiled with MI_ENABLE_DLSS.");
    }
};

NAMESPACE_END(mitsuba)

MI_PY_EXPORT(DLSSDenoiser) {
    MI_PY_IMPORT_TYPES()
    using Denoiser = DLSSDenoiserStub<Float, Spectrum>;

    nb::class_<Denoiser, Object>(m, "DLSSDenoiser", D(DLSSDenoiser))
        .def_static("is_available", []() { return false; },
                    D(DLSSDenoiser, is_available))
        .def(
            "__init__",
            [](Denoiser *, const ScalarVector2u &, const ScalarVector2u &,
               const std::string &) { Denoiser::fail(); },
            "input_size"_a, "output_size"_a = ScalarVector2u(0, 0),
            "quality"_a = "high", D(DLSSDenoiser, DLSSDenoiser))
        .def(
            "__call__",
            [](const Denoiser &, const TensorXf &, const TensorXf &,
               const TensorXf &, const TensorXf &, const TensorXf &,
               const TensorXf &, const TensorXf &, const TensorXf &,
               const nb::object &, const ScalarPoint2f &,
               bool) -> TensorXf { Denoiser::fail(); },
            "noisy"_a, "albedo"_a, "normals"_a, "depth"_a,
            "specular_albedo"_a = TensorXf(), "roughness"_a = TensorXf(),
            "flow"_a = TensorXf(), "specular_flow"_a = TensorXf(),
            "to_sensor"_a = nb::none(), "jitter"_a = ScalarPoint2f(0.f),
            "reset"_a = false, D(DLSSDenoiser, operator_call))
        .def(
            "__call__",
            [](const Denoiser &, const ref<Bitmap> &, const std::string &,
               const std::string &, const std::string &, const std::string &,
               const std::string &, const std::string &, const std::string &,
               const nb::object &, const ScalarPoint2f &, bool,
               const std::string &) -> ref<Bitmap> { Denoiser::fail(); },
            "noisy"_a, "albedo_ch"_a, "normals_ch"_a, "depth_ch"_a,
            "specular_albedo_ch"_a = "", "roughness_ch"_a = "",
            "flow_ch"_a = "", "specular_flow_ch"_a = "",
            "to_sensor"_a = nb::none(), "jitter"_a = ScalarPoint2f(0.f),
            "reset"_a = false, "noisy_ch"_a = "<root>",
            D(DLSSDenoiser, operator_call, 2));
}

#endif // defined(MI_ENABLE_CUDA) && defined(MI_ENABLE_DLSS)
