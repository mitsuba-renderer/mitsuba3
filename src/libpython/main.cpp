#include "python.h"

MTS_PY_DECLARE(pcg32);

PYBIND11_PLUGIN(mitsuba) {
    py::module m("mitsuba", "Mitsuba Python plugin");

    MTS_PY_IMPORT(pcg32);

    return m.ptr();
}
