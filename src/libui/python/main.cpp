#include <mitsuba/ui/texture.h>
#include <mitsuba/python/python.h>

PYBIND11_DECLARE_HOLDER_TYPE(T, nanogui::ref<T>)

MTS_PY_EXPORT(Texture) {
    MTS_PY_CHECK_ALIAS(mitsuba::GPUTexture, "Texture") {
        py::class_<mitsuba::GPUTexture, nanogui::Texture, nanogui::ref<mitsuba::GPUTexture>>(m, "Texture", D(Texture))
            .def(py::init<const Bitmap *, nanogui::Texture::InterpolationMode,
                        nanogui::Texture::InterpolationMode, nanogui::Texture::WrapMode>(),
                D(GPUTexture, GPUTexture),"bitmap"_a,
                "min_interpolation_mode"_a = nanogui::Texture::InterpolationMode::Bilinear,
                "mag_interpolation_mode"_a = nanogui::Texture::InterpolationMode::Bilinear,
                "wrap_mode"_a              = nanogui::Texture::WrapMode::ClampToEdge);
    }
}

PYBIND11_MODULE(mitsuba_ui_ext, m_) {
    (void) m_; /* unused */
    py::module::import("nanogui");

    py::module m = py::module::import("mitsuba");
    std::vector<py::module> submodule_list;
    MTS_PY_DEF_SUBMODULE(submodule_list, ui)

    MTS_PY_IMPORT(Texture);
}
