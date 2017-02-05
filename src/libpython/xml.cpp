#include <mitsuba/core/xml.h>
#include <mitsuba/core/filesystem.h>
#include "python.h"

MTS_PY_EXPORT(xml) {
    MTS_PY_IMPORT_MODULE(xml, "mitsuba.core.xml");

    xml.def("load_file", &xml::load_file, "path"_a, D(xml, load_file));
    xml.def("load_string", &xml::load_string, "string"_a, D(xml, load_string));
}

