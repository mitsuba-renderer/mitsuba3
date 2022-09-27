#if defined(MI_ENABLE_CUDA)

#include <mitsuba/render/optixdenoiser.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(OptixDenoiser) {
    MI_PY_IMPORT_TYPES(OptixDenoiser)
    MI_PY_CLASS(OptixDenoiser, Object)
        .def(py::init<const ScalarVector2u &, bool, bool, bool>(),
             "input_size"_a, "albedo"_a = false, "normals"_a = false,
             "temporal"_a = false, D(OptixDenoiser, OptixDenoiser))
        .def("__call__",
             py::overload_cast<const TensorXf &, bool, const TensorXf *,
                               const TensorXf *, const Transform4f *,
                               const TensorXf *, const TensorXf *>(
             &OptixDenoiser::operator(), py::const_),
             "noisy"_a, "denoise_alpha"_a = true, "albedo"_a = nullptr,
             "normals"_a = nullptr, "n_frame"_a = nullptr, "flow"_a = nullptr,
             "previous_denoised"_a = nullptr,
             D(OptixDenoiser, operator_call))
        .def("__call__",
             py::overload_cast<const ref<Bitmap> &, bool, const std::string &,
                               const std::string &, const Transform4f &,
                               const std::string &, const std::string &,
                               const std::string &>(
             &OptixDenoiser::operator(), py::const_),
             "noisy"_a, "denoise_alpha"_a = true, "albedo_ch"_a = "",
             "normals_ch"_a = "", "n_frame"_a = Transform4f(), "flow_ch"_a = "",
             "previous_denoised_ch"_a = "", "noisy_ch"_a = "<root>",
             D(OptixDenoiser, operator_call, 2));
}

#endif // defined(MI_ENABLE_CUDA)
