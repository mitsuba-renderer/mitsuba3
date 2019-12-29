#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/xml.h>
#include <mitsuba/python/python.h>

extern py::object cast_object(Object *o, py::handle parent);

MTS_PY_EXPORT(xml) {
    MTS_PY_IMPORT_TYPES()

    // Create dedicated submodule
    auto xml = m.def_submodule("xml", "Mitsuba scene XML parser");

    xml.def(
        "load_file",
        [](const std::string &name, const xml::ParameterList &param,
           bool update_scene) {
            py::gil_scoped_release release;
            return cast_object(
                xml::load_file(
                    name, mitsuba::detail::get_variant<Float, Spectrum>(), param, update_scene),
                py::handle());
        },
        "path"_a, "parameters"_a = py::list(), "update_scene"_a = false, D(xml, load_file));

    xml.def(
        "load_string",
        [](const std::string &name, const xml::ParameterList &param) {
            py::gil_scoped_release release;
            return cast_object(
                xml::load_string(name, mitsuba::detail::get_variant<Float, Spectrum>(), param),
                py::handle());
        },
        "string"_a, "parameters"_a = py::list(), D(xml, load_string));
}
