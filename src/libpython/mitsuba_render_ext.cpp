#include "python.h"

MTS_PY_DECLARE(Scene);
MTS_PY_DECLARE(Shape);
MTS_PY_DECLARE(ShapeKDTree);

PYBIND11_PLUGIN(mitsuba_render_ext) {
    py::module m_("mitsuba_render_ext"); // unused
    py::module m = py::module::import("mitsuba.render");

    MTS_PY_IMPORT(Scene);
    MTS_PY_IMPORT(Shape);
    MTS_PY_IMPORT(ShapeKDTree);

    return m_.ptr();
}
