#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/xml.h>
#include <mitsuba/python/python.h>

extern py::object py_cast(Object *o);

MTS_PY_EXPORT(xml) {
    MTS_PY_IMPORT_MODULE(xml, "mitsuba.core.xml");

    xml.def(
        "load_file",
        [](const std::string &name, const xml::ParameterList &param, bool monochrome) {
            py::gil_scoped_release release;
            return py_cast(xml::load_file(name, param, monochrome));
        },
        "path"_a, "parameters"_a = py::list(), "monochrome"_a = false, D(xml, load_file));

    xml.def(
        "load_string",
        [](const std::string &name, const xml::ParameterList &param, bool monochrome) {
            py::gil_scoped_release release;
            return py_cast(xml::load_string(name, param, monochrome));
        },
        "string"_a, "parameters"_a = py::list(), "monochrome"_a = false, D(xml, load_string));
}
