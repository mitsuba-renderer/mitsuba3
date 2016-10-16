#include "python.h"

MTS_PY_DECLARE(Scene);
MTS_PY_DECLARE(Shape);
MTS_PY_DECLARE(ShapeKDTree);

PYBIND11_PLUGIN(mitsuba_render_ext) {
    py::module::import("mitsuba.core");

    py::module m("mitsuba_render_ext");

    MTS_PY_IMPORT(Scene);
    MTS_PY_IMPORT(Shape);
    MTS_PY_IMPORT(ShapeKDTree);

    return m.ptr();
}
