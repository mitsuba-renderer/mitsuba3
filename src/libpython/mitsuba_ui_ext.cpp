#include "python.h"


PYBIND11_PLUGIN(mitsuba_ui_ext) {
    py::module::import("mitsuba.ui");

    py::module m("mitsuba_uir_ext");

    return m.ptr();
}
