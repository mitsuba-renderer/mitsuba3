#include <mitsuba/render/denoiser.h>
#include <mitsuba/python/python.h>

MI_PY_EXPORT(denoiser) {
    MI_PY_IMPORT_TYPES(Denoiser)
    MI_PY_CLASS(Denoiser, Object)
        .def(py::init<const ScalarVector2u &, bool, bool, bool>(),
             "input_size"_a, "albedo"_a = false, "normals"_a = false,
             "temporal"_a = false)
        .def("denoise",
             py::overload_cast<const TensorXf &, const TensorXf *,
                               const TensorXf *, const TensorXf *,
                               const TensorXf *>(&Denoiser::denoise),
             "noisy"_a, "albedo"_a = nullptr, "normals"_a = nullptr,
             "flow"_a = nullptr, "previous_denoised"_a = nullptr,
             D(Denoiser, denoise1))
        .def("denoise",
             py::overload_cast<const ref<Bitmap> &, const std::string &,
                               const std::string &, const std::string &,
                               const std::string &, const std::string &>(
                 &Denoiser::denoise),
             "noisy"_a, "albedo_ch"_a = "", "normals_ch"_a = "",
             "flow_ch"_a = "", "previous_denoised_ch"_a = "",
             "noisy_ch"_a = "<root>", D(Denoiser, denoise));
}
