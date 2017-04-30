#include <mitsuba/core/xml.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/python/python.h>

extern py::object py_cast(Object *o);

MTS_PY_EXPORT(xml) {
    MTS_PY_IMPORT_MODULE(xml, "mitsuba.core.xml");

    xml.def(
        "load_file",
        [](const std::string &name) { return py_cast(xml::load_file(name)); },
        "path"_a, D(xml, load_file));

    xml.def(
        "load_string",
        [](const std::string &name) { return py_cast(xml::load_string(name)); },
        "string"_a, D(xml, load_string));
}

