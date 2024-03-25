#if defined(MI_ENABLE_CUDA)

#include <nanobind/nanobind.h>
#include <mitsuba/render/optixdenoiser.h>
#include <mitsuba/python/python.h>

#include <nanobind/stl/string.h>

MI_PY_EXPORT(OptixDenoiser) {
    MI_PY_IMPORT_TYPES(OptixDenoiser)
    MI_PY_CLASS(OptixDenoiser, Object)
        .def(nb::init<const ScalarVector2u &, bool, bool, bool>(),
             "input_size"_a, "albedo"_a = false, "normals"_a = false,
             "temporal"_a = false, D(OptixDenoiser, OptixDenoiser))
        .def(
            "__call__",
            [](const OptixDenoiser &denoiser, const TensorXf &noisy,
               bool denoise_alpha, const TensorXf &albedo,
               const TensorXf &normals, const nb::object &transform,
               const TensorXf &flow, const TensorXf &previous_denoised) {
                Transform4f to_sensor;
                if (!transform.is(nb::none()))
                    to_sensor = nb::cast<Transform4f>(transform);

                return denoiser(noisy, denoise_alpha, albedo, normals,
                                to_sensor, flow, previous_denoised);
            },
            "noisy"_a, "denoise_alpha"_a = true, "albedo"_a = TensorXf(),
            "normals"_a = TensorXf(), "to_sensor"_a = nb::none(),
            "flow"_a = TensorXf(), "previous_denoised"_a = TensorXf(),
            D(OptixDenoiser, operator_call))
        .def(
            "__call__",
            [](const OptixDenoiser &denoiser, const ref<Bitmap> &noisy,
               bool denoise_alpha, const std::string &albedo_ch,
               const std::string &normals_ch, const nb::object &transform,
               const std::string &flow_ch,
               const std::string &previous_denoised_ch,
               const std::string &noisy_ch) {
                Transform4f to_sensor;
                if (!transform.is(nb::none()))
                    to_sensor = nb::cast<Transform4f>(transform);

                return denoiser(noisy, denoise_alpha, albedo_ch, normals_ch,
                                to_sensor, flow_ch, previous_denoised_ch,
                                noisy_ch);
            },
            "noisy"_a, "denoise_alpha"_a = true, "albedo_ch"_a = "",
            "normals_ch"_a = "", "to_sensor"_a = nb::none(), "flow_ch"_a = "",
            "previous_denoised_ch"_a = "", "noisy_ch"_a = "<root>",
            D(OptixDenoiser, operator_call, 2));
}

#endif // defined(MI_ENABLE_CUDA)
