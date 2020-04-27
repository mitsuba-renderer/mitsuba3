#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/xml.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>
#include <pybind11/numpy.h>

using Caster = py::object(*)(mitsuba::Object *);
extern Caster cast_object;

// Forward declaration
MTS_VARIANT ref<Object> load_dict(const std::string& name, const py::dict& dict);

/// Shorthand notation for accessing the MTS_VARIANT string
#define GET_VARIANT() mitsuba::detail::get_variant<Float, Spectrum>()

MTS_PY_EXPORT(xml) {
    MTS_PY_IMPORT_TYPES()

    m.def(
        "load_file",
        [](const std::string &name, bool update_scene, py::kwargs kwargs) {
            xml::ParameterList param;
            if (kwargs) {
                for (auto [k, v] : kwargs)
                    param.emplace_back(
                        (std::string) py::str(k),
                        (std::string) py::str(v)
                    );
            }
            py::gil_scoped_release release;
            return cast_object(xml::load_file(name, GET_VARIANT(), param, update_scene));
        },
        "path"_a, "update_scene"_a = false, D(xml, load_file));

    m.def(
        "load_string",
        [](const std::string &name, py::kwargs kwargs) {
            xml::ParameterList param;
            if (kwargs) {
                for (auto [k, v] : kwargs)
                    param.emplace_back(
                        (std::string) py::str(k),
                        (std::string) py::str(v)
                    );
            }
            py::gil_scoped_release release;
            return cast_object(xml::load_string(name, GET_VARIANT(), param));
        },
        "string"_a, D(xml, load_string));

    m.def(
        "load_dict",
        [](const py::dict dict) {
            if (dict.size() != 1)
                Throw("Should only contain a single (key, value) at root level!");
            return cast_object(load_dict<Float, Spectrum>("root", dict));
        },
        "dict"_a);
}

#define SET_PROPS(PyType, Type, Setter)         \
    if (py::isinstance<PyType>(value)) {        \
        props.Setter(key, value.cast<Type>());  \
        continue;                               \
    }

MTS_VARIANT ref<Object> load_dict(const std::string &name, const py::dict &dict) {
    bool within_emitter  = (name == "emitter");
    bool within_spectrum = (name == "spectrum");
    bool is_root         = (name == "root");
    bool is_scene        = (name == "scene");

    Properties props;

    if (is_scene)
        props.set_plugin_name("scene");

    for (auto& [k, value] : dict) {
        std::string key = k.cast<std::string>();
        if (key == "type") {
            if (is_scene)
                Throw("Scene dictionary shouldn't contain 'type' entry.");
            props.set_plugin_name(value.cast<std::string>());
            continue;
        }
        if (!is_root && key == "rgb") {
            // Read info from the dictionary
            Properties::Color3f color;
            std::string rgb_name;
            for (auto& [k2, value2] : value.cast<py::dict>()) {
                std::string key2 = k2.cast<std::string>();
                if (key2 == "name")
                    rgb_name = value2.cast<std::string>();
                else if (key2 == "value")
                    color = value2.cast<Properties::Color3f>();
                else
                    Throw("Unexpected item in rgb dictionary: %s", key2);
            }
            // Update the properties struct
            if (!within_spectrum) {
                ref<Object> obj = mitsuba::xml::detail::create_texture_from_rgb(
                    rgb_name, color, GET_VARIANT(), within_emitter);
                props.set_object(rgb_name, obj);
            } else {
                props.set_color("color", color);
            }
            continue;
        }
        if (!is_root && key == "spectrum") {
            // Read info from the dictionary
            std::string spec_name;
            Properties::Float const_value(1.f);
            std::vector<Properties::Float> wavelengths;
            std::vector<Properties::Float> values;
            for (auto& [k2, value2] : value.cast<py::dict>()) {
                std::string key2 = k2.cast<std::string>();
                if (key2 == "name") {
                    spec_name = value2.cast<std::string>();
                } else if (key2 == "filename") {
                    spectrum_from_file(value2.cast<std::string>(), wavelengths, values);
                } else if (key2 == "value") {
                    if (py::isinstance<py::float_>(value2)) {
                        const_value = value2.cast<Properties::Float>();
                    } else if (py::isinstance<py::list>(value2)) {
                        py::list list = value2.cast<py::list>();
                        wavelengths.resize(list.size());
                        values.resize(list.size());
                        for (size_t i = 0; i < list.size(); ++i) {
                            auto pair = list[i].cast<py::tuple>();
                            wavelengths[i] = pair[0].cast<Properties::Float>();
                            values[i]      = pair[1].cast<Properties::Float>();
                        }
                    } else {
                        Throw("Unexpected value type in spectrum dictionary: %s", value2);
                    }
                } else {
                    Throw("Unexpected item in spectrum dictionary: %s", key2);
                }
            }
            // Update the properties struct
            ref<Object> obj =
                mitsuba::xml::detail::create_texture_from_spectrum(
                    spec_name, const_value, wavelengths, values, GET_VARIANT(),
                    within_emitter, is_spectral_v<Spectrum>,
                    is_monochromatic_v<Spectrum>);
            props.set_object(spec_name, obj);
            continue;
        }

        SET_PROPS(py::bool_, bool, set_bool);
        SET_PROPS(py::int_, int64_t, set_long);
        SET_PROPS(py::float_, Properties::Float, set_float);
        SET_PROPS(py::str, std::string, set_string);
        SET_PROPS(Properties::Point3f, Properties::Point3f, set_point3f);
        // TODO vector3f always get cast to point3f ATM
        // SET_PROPS(Properties::Vector3f, Properties::Vector3f, set_vector3f);
        SET_PROPS(Properties::Transform4f, Properties::Transform4f, set_transform);

        // Cast py::list and numpy.ndarray to Array3f
        if (py::isinstance<py::list>(value) ||
            strcmp(value.ptr()->ob_type->tp_name, "numpy.ndarray") == 0) {
            if (PyObject_Length(value.ptr()) != 3)
                Throw("Expect array of size 3, found: %u", PyObject_Length(value.ptr()));
            props.set_array3f(key, value.cast<Properties::Array3f>());
            continue;
        }

        // Load nested dictionary
        if (py::isinstance<py::dict>(value)) {
            auto obj = load_dict<Float, Spectrum>(key, value.cast<py::dict>());
            if (is_root)
                return obj;
            props.set_object(key, obj);
            continue;
        }

        // Try to cast entry to an object if it didn't match any of the other types above
        try {
            auto obj = value.cast<ref<Object>>();
            props.set_object(key, obj);
            continue;
        } catch (const pybind11::cast_error &e) {
            Throw("Unkown value type: %s", value.get_type());
        }
    }

    if (props.plugin_name().empty())
        Throw("Missing key 'type' in dictionary: %s", dict);

    // Construct the object with the parsed Properties
    const Class *class_ = mitsuba::xml::detail::tag_to_class(name, GET_VARIANT());
    auto obj = PluginManager::instance()->create_object(props, class_);
    if (!props.unqueried().empty())
        Throw("Unreferenced attribute %s in %s", props.unqueried()[0], name);

    return obj;
}

#undef SET_PROPS