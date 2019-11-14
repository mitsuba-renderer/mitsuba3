#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/xml.h>
#include <mitsuba/python/python.h>

extern py::object py_cast(Object *o);

MTS_PY_EXPORT(xml) {

    // Create dedicated submodule
    auto xml = m.def_submodule("xml", "Mitsuba scene XML parser");

    xml.def(
        "load_file",
        [](const std::string &name, const xml::ParameterList &param,
           const std::string & variant, bool update_scene) {
            py::gil_scoped_release release;
            return py_cast(xml::load_file(name, variant, param, update_scene));
        },
        "path"_a, "variant"_a = "", "parameters"_a = py::list(),
        "update_scene"_a = false, D(xml, load_file));

    xml.def(
        "load_string",
        [](const std::string &name, const xml::ParameterList &param, const std::string & variant) {
            py::gil_scoped_release release;
            return py_cast(xml::load_string(name, variant, param));
        },
        "string"_a, "variant"_a = "", "parameters"_a = py::list(), D(xml, load_string));
}
