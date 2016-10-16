#include <mitsuba/core/xml.h>
#include <mitsuba/core/filesystem.h>
#include "python.h"

MTS_PY_EXPORT(xml) {
    MTS_PY_IMPORT_MODULE(xml, "mitsuba.core.xml");

    xml.def("loadFile", &xml::loadFile, py::arg("path"), DM(xml, loadFile));
    xml.def("loadString", &xml::loadString, py::arg("string"), DM(xml, loadString));
}

