#include <mitsuba/core/vector.h>
#include "python.h"

MTS_PY_EXPORT(vector) {
#if defined(SINGLE_PRECISION)
    m.attr("float_dtype") = py::dtype("f");
#else
    m.attr("float_dtype") = py::dtype("d");
#endif
    m.attr("PacketSize") = py::cast(PacketSize);

    m.def("coordinateSystem", &coordinateSystem, DM(coordinateSystem));
}
