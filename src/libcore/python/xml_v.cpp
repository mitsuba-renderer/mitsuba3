#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/xml.h>
#include <mitsuba/python/python.h>

using Caster = py::object(*)(mitsuba::Object *);
extern Caster cast_object;

MTS_PY_EXPORT(xml) {
    MTS_PY_IMPORT_TYPES()

    m.def(
        "load_file",
        [](const std::string &name, const xml::ParameterList &param, bool update_scene) {
            py::gil_scoped_release release;
            return cast_object(
                xml::load_file(
                    name, mitsuba::detail::get_variant<Float, Spectrum>(), param, update_scene));
        },
        "path"_a, "parameters"_a = py::list(), "update_scene"_a = false, D(xml, load_file));

    m.def(
        "load_string",
        [](const std::string &name, const xml::ParameterList &param) {
            py::gil_scoped_release release;
            return cast_object(
                xml::load_string(name, mitsuba::detail::get_variant<Float, Spectrum>(), param));
        },
        "string"_a, "parameters"_a = py::list(), D(xml, load_string));
}
