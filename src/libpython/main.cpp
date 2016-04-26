#include "python.h"

MTS_PY_DECLARE(pcg32);
MTS_PY_DECLARE(object);

PYBIND11_PLUGIN(mitsuba) {
    py::module m("mitsuba", "Mitsuba Python plugin");

    MTS_PY_IMPORT(pcg32);
    MTS_PY_IMPORT(object);

    return m.ptr();
}
