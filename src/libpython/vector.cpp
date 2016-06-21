#include <mitsuba/core/vector.h>
#include "python.h"

MTS_PY_EXPORT(vector) {
    m.def("coordinateSystem", &coordinateSystem, DM(coordinateSystem));
}

