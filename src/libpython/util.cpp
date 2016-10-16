#include <mitsuba/core/util.h>
#include "python.h"

MTS_PY_EXPORT(util) {
    MTS_PY_IMPORT_MODULE(util, "mitsuba.core.util");

    util.def("coreCount", &util::coreCount, DM(util, coreCount));
    util.def("timeString", &util::timeString, py::arg("time"),
          py::arg("precise") = false, DM(util, timeString));
    util.def("memString", &util::memString, py::arg("size"),
          py::arg("precise") = false, DM(util, memString));
    util.def("trapDebugger", &util::trapDebugger, DM(util, trapDebugger));
}
