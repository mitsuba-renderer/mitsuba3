#include "python.h"

#include <mitsuba/ui/gltexture.h>

MTS_PY_EXPORT(GLTexture) {
    auto gltexture = MTS_PY_CLASS(GLTexture, Object)
        .def(py::init<>(), D(GLTexture, GLTexture))
        .mdef(GLTexture, set_interpolation)
        .mdef(GLTexture, id)
        .mdef(GLTexture, init)
        .mdef(GLTexture, free)
        .mdef(GLTexture, bind)
        .mdef(GLTexture, release)
        .mdef(GLTexture, refresh);

    py::enum_<GLTexture::EInterpolation>(gltexture, "EInterpolation")
        .value("ENearest", GLTexture::ENearest)
        .value("ELinear", GLTexture::ELinear)
        .value("EMipMapLinear", GLTexture::EMipMapLinear)
        .export_values();
}

PYBIND11_PLUGIN(mitsuba_ui_ext) {
    py::module m_("mitsuba_ui_ext"); // unused
    py::module m = py::module::import("mitsuba.ui");

    MTS_PY_IMPORT(GLTexture);

    return m_.ptr();
}
