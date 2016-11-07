#include "python.h"

#include <mitsuba/ui/gltexture.h>

MTS_PY_EXPORT(GLTexture) {
    MTS_PY_CLASS(GLTexture, Object)
        .def(py::init<const Bitmap *>(), DM(GLTexture, GLTexture));
}

PYBIND11_PLUGIN(mitsuba_ui_ext) {
    py::module m_("mitsuba_ui_ext"); // unused
    py::module m = py::module::import("mitsuba.ui");

    MTS_PY_IMPORT(GLTexture);

    return m_.ptr();
}
