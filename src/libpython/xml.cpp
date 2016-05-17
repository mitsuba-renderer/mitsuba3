#include <mitsuba/core/xml.h>
#include <mitsuba/core/filesystem.h>
#include "python.h"

MTS_PY_EXPORT(xml) {
    auto m2 = m.def_submodule("xml", "Mitsuba scene XML parser");

    m2.def("loadFile", &xml::loadFile, py::arg("path"), DM(xml, loadFile));
    m2.def("loadString", &xml::loadString, py::arg("string"), DM(xml, loadString));
}

