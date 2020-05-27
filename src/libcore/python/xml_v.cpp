#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/xml.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>
#include <map>

using Caster = py::object(*)(mitsuba::Object *);
extern Caster cast_object;

// Forward declaration
template <typename Float, typename Spectrum>
ref<Object> load_dict(const py::dict& dict, std::map<std::string, ref<Object>> &instances);

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
            std::map<std::string, ref<Object>> instances;
            return cast_object(load_dict<Float, Spectrum>(dict, instances));
        },
        "dict"_a,
R"doc(Load a Mitsuba scene or object from an Python dictionary

Parameter ``dict``:
    Python dictionary containing the object description

)doc");
}

// Helper function to find a value of "type" in a Python dictionary
std::string get_type(const py::dict &dict) {
    for (auto& [key, value] : dict)
        if (key.template cast<std::string>() == "type")
            return value.template cast<std::string>();

    Throw("Missing key 'type' in dictionary: %s", dict);
}

#define SET_PROPS(PyType, Type, Setter)         \
    if (py::isinstance<PyType>(value)) {        \
        props.Setter(key, value.template cast<Type>());  \
        continue;                               \
    }

/// Helper function to give the object a chance to recursively expand into sub-objects
void expand_and_set_object(Properties &props, const std::string &name, const ref<Object> &obj) {
    std::vector<ref<Object>> children = obj->expand();
    if (children.empty()) {
        props.set_object(name, obj);
    } else if (children.size() == 1) {
        props.set_object(name, children[0]);
    } else {
        int ctr = 0;
        for (auto c : children)
            props.set_object(name + "_" + std::to_string(ctr++), c);
    }
}

template <typename Float, typename Spectrum>
ref<Object> load_dict(const py::dict &dict, std::map<std::string, ref<Object>> &instances) {

    MTS_IMPORT_CORE_TYPES()
    using ScalarArray3f = Array<ScalarFloat, 3>;

    std::string type = get_type(dict);
    bool is_scene = (type == "scene");

    const Class *class_;
    if (is_scene)
        class_ = Class::for_name("Scene", GET_VARIANT());
    else
        class_ = PluginManager::instance()->get_plugin_class(type, GET_VARIANT());

    bool within_emitter = (class_->parent()->alias() == "emitter");
    Properties props(type);

    for (auto& [k, value] : dict) {
        std::string key = k.template cast<std::string>();

        if (key == "type")
            continue;

        if (key == "id") {
            props.set_id(value.template cast<std::string>());
            continue;
        }

        SET_PROPS(py::bool_, bool, set_bool);
        SET_PROPS(py::int_, int64_t, set_long);
        SET_PROPS(py::float_, Properties::Float, set_float);
        SET_PROPS(py::str, std::string, set_string);
        SET_PROPS(ScalarArray3f, ScalarArray3f, set_array3f);
        SET_PROPS(ScalarTransform4f, ScalarTransform4f, set_transform);

        // Load nested dictionary
        if (py::isinstance<py::dict>(value)) {
            py::dict dict2 = value.template cast<py::dict>();
            std::string type2 = get_type(dict2);

            // Treat nested dictionary differently when their type is "rgb" or "spectrum"
            if (type2 == "rgb") {
                if (dict2.size() != 2) {
                    Throw("'rgb' dictionary should always contain 2 entries "
                          "('type' and 'value'), got %u.", dict2.size());
                }
                // Read info from the dictionary
                Properties::Color3f color;
                for (auto& [k2, value2] : dict2) {
                    std::string key2 = k2.template cast<std::string>();
                    if (key2 == "value")
                        color = value2.template cast<Properties::Color3f>();
                    else if (key2 != "type")
                        Throw("Unexpected key in rgb dictionary: %s", key2);
                }
                // Update the properties struct
                ref<Object> obj = mitsuba::xml::detail::create_texture_from_rgb(
                    key, color, GET_VARIANT(), within_emitter);
                props.set_object(key, obj);
                continue;
            }
            if (type2 == "spectrum") {
                if (dict2.size() != 2) {
                    Throw("'spectrum' dictionary should always contain 2 "
                          "entries ('type' and 'value'), got %u.", dict2.size());
                }
                // Read info from the dictionary
                Properties::Float const_value(1.f);
                std::vector<Properties::Float> wavelengths;
                std::vector<Properties::Float> values;
                for (auto& [k2, value2] : dict2) {
                    std::string key2 = k2.template cast<std::string>();
                    if (key2 == "filename") {
                        spectrum_from_file(value2.template cast<std::string>(), wavelengths, values);
                    } else if (key2 == "value") {
                        if (py::isinstance<py::float_>(value2) ||
                            py::isinstance<py::int_>(value2)) {
                            const_value = value2.template cast<Properties::Float>();
                        } else if (py::isinstance<py::list>(value2)) {
                            py::list list = value2.template cast<py::list>();
                            wavelengths.resize(list.size());
                            values.resize(list.size());
                            for (size_t i = 0; i < list.size(); ++i) {
                                auto pair = list[i].template cast<py::tuple>();
                                wavelengths[i] = pair[0].template cast<Properties::Float>();
                                values[i]      = pair[1].template cast<Properties::Float>();
                            }
                        } else {
                            Throw("Unexpected value type in 'spectrum' dictionary: %s", value2);
                        }
                    } else if (key2 != "type") {
                        Throw("Unexpected key in spectrum dictionary: %s", key2);
                    }
                }
                // Update the properties struct
                ref<Object> obj =
                    mitsuba::xml::detail::create_texture_from_spectrum(
                        key, const_value, wavelengths, values, GET_VARIANT(),
                        within_emitter, is_spectral_v<Spectrum>,
                        is_monochromatic_v<Spectrum>);
                props.set_object(key, obj);
                continue;
            }

            // Nested dict with type == "ref" specify a reference to another
            // object previously instanciated
            if (type2 == "ref") {
                if (is_scene)
                    Throw("Reference found at the scene level: %s", key);

                for (auto& [k2, value2] : value.template cast<py::dict>()) {
                    std::string key2 = k2.template cast<std::string>();
                    if (key2 == "id") {
                        std::string id = value2.template cast<std::string>();
                        if (instances.count(id) == 1)
                            expand_and_set_object(props, key, instances[id]);
                        else
                            Throw("Referenced id \"%s\" not found: %s", id, key);
                    }  else if (key2 != "type") {
                        Throw("Unexpected key in ref dictionary: %s", key2);
                    }
                }
                continue;
            }

            // Load the dictionary recursively
            ref<Object> obj = load_dict<Float, Spectrum>(dict2, instances);
            expand_and_set_object(props, key, obj);

            // Add instanced object to the instance map for later references
            if (is_scene) {
                // An object can be referenced using its key
                if (instances.count(key) != 0)
                    Throw("%s has duplicate id: %s", key, key);
                instances[key] = obj;

                // An object can also be referenced using its "id" if it has one
                std::string id = obj->id();
                if (!id.empty() && id != key) {
                    if (instances.count(id) != 0)
                        Throw("%s has duplicate id: %s", key, id);
                    instances[id] = obj;
                }
            }

            continue;
        }

        // Try to cast to Array3f (list, tuple, numpy.array, ...)
        try {
            props.set_array3f(key, value.template cast<Properties::Array3f>());
            continue;
        } catch (const pybind11::cast_error &e) { }

        // Try to cast entry to an object
        try {
            auto obj = value.template cast<ref<Object>>();
            expand_and_set_object(props, key, obj);
            continue;
        } catch (const pybind11::cast_error &e) { }

        // Didn't match any of the other types above
        Throw("Unkown value type: %s", value.get_type());
    }

    // Construct the object with the parsed Properties
    auto obj = PluginManager::instance()->create_object(props, class_);

    if (!props.unqueried().empty())
        Throw("Unreferenced attribute %s in %s", props.unqueried()[0], type);

    return obj;
}

#undef SET_PROPS
