#include <mitsuba/core/util.h>
#include "python.h"

MTS_PY_EXPORT(util) {
    py::module util = m.def_submodule("util", "Miscellaneous utility routines");

    util.def("getCoreCount", &util::getCoreCount, DM(util, getCoreCount));
    util.def("timeString", &util::timeString, py::arg("time"),
          py::arg("precise") = false, DM(util, timeString));
    util.def("memString", &util::memString, py::arg("size"),
          py::arg("precise") = false, DM(util, memString));
    util.def("trapDebugger", &util::trapDebugger, DM(util, trapDebugger));
}
