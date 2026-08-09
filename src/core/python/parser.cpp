#include <nanobind/nanobind.h> // Needs to be first, to get `ref<T>` caster
#include <mitsuba/python/python.h>
#include <mitsuba/core/parser.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/fresolver.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/bind_vector.h>

using namespace mitsuba::parser;

// Make std::vector<SceneNode> opaque for bind_vector
NB_MAKE_OPAQUE(std::vector<parser::SceneNode>);

// For casting ref<Object> to Python
extern nb::object cast_object(Object *o);

// Helper to parse RGB/spectrum dictionaries
static void parse_color_spectrum(ParserState &state, size_t parent_idx,
                                 std::string_view key, const nb::dict &dict,
                                 std::string_view type, std::string_view path) {
    if (dict.size() != 2)
        Throw("[%s] '%s' dictionary should contain 2 entries ('type' and "
              "either 'value' or 'filename'), got %zu.", path, type, dict.size());

    nb::object value = dict.get("value", nb::handle());

    if (type == "rgb") {
        if (!value.is_valid())
            Throw("[%s] rgb dictionary lacks \"value\" entry!");

        try {
            state.nodes[parent_idx].props.set(key, nb::cast<Color<double, 3>>(value));
        } catch (const nb::cast_error &) {
            Throw("[%s] could not interpret \"%s\" as a color.", path,
                  nb::str(value).c_str());
        }
    } else {
        Properties::Spectrum spec;

        if (value.is_valid()) {
            double value_d;
            if (nb::try_cast<double>(value, value_d)) {
                // Uniform spectrum
                spec.values.push_back(value_d);
            } else if (nb::isinstance<nb::sequence>(value)) {
                // Wavelength-value pairs
                size_t size = nb::len(value);
                std::vector<double> wavelengths, values;
                wavelengths.reserve(size);
                values.reserve(size);

                for (nb::handle entry: value) {
                    if (nb::len(entry) != 2)
                        Throw("[%s] Spectrum wavelength-value pairs must have "
                              "exactly 2 elements, got %zu.",
                              path, nb::len(entry));

                    double f0, f1;
                    if (!nb::try_cast(entry[0], f0))
                        Throw("[%s] invalid wavelength '%s' in spectrum.",
                              path, nb::str((nb::handle) entry[0]).c_str());

                    if (!nb::try_cast(entry[1], f1))
                        Throw("[%s] invalid value '%s' in spectrum.",
                              path, nb::str((nb::handle) entry[1]).c_str());

                    wavelengths.push_back(f0);
                    values.push_back(f1);
                }

                // Use the constructor that validates wavelength order
                spec = Properties::Spectrum(std::move(wavelengths), std::move(values));
            } else {
                Throw("[%s] Unexpected value type in 'spectrum' dictionary: %s",
                      path, nb::inst_name(value).c_str());
            }
        } else {
            nb::object filename = dict.get("filename", nb::handle());
            if (!filename.is_valid())
                Throw("[%s] Spectrum dictionary must contain either 'value' or 'filename'", path);
            std::string filename_str;
            fs::path filename_path;
            if (nb::try_cast(filename, filename_str)) {
                // success
            } else if (nb::try_cast(filename, filename_path)) {
                filename_str = filename_path.string();
            } else {
                Throw("[%s] Could not convert filename '%s' to string",
                      path, nb::str(filename).c_str());
            }
            spectrum_from_file(filename_str, spec.wavelengths, spec.values);
        }

        state.nodes[parent_idx].props.set(key, std::move(spec));
    }
}

// Helper function to parse dictionary entries recursively
static void parse_dict_impl(ParserState &state, const nb::dict &d,
                            size_t parent_idx, std::string_view parent_path) {
    for (auto [key_o, value] : d) {
        if (!nb::isinstance<nb::str>(key_o))
            Throw("[%s]: dictionary keys must be strings", parent_path);

        std::string_view key = nb::cast<std::string_view>(key_o);

        // Skip the "type" and "id" keys as they're already processed
        if (key == "type" || key == "id")
            continue;

        // Validate key name - dots are reserved for path delimiters
        if (key.find('.') != std::string_view::npos)
            Throw("[%s] The object key '%s' contains a '.' character, which is "
                  "reserved as a delimiter in object paths. Please use '_' instead.",
                  parent_path, key);

        // Build the current property path (skip root prefix like old parser)
        bool is_root = parent_idx == 0;
        std::string path = is_root ? std::string(key)
                                   : std::string(parent_path) + "." + std::string(key);

        // Check if value is a nested dictionary (child object)
        if (nb::isinstance<nb::dict>(value)) {
            nb::dict child_dict = nb::borrow<nb::dict>(value);

            if (!child_dict.contains("type"))
                Throw("[%s] missing 'type' attribute", path);

            // Get child type and create node
            std::string_view type = nb::cast<std::string_view>(child_dict["type"]);

            if (type == "ref") {
                // Handle reference dictionary: {"type": "ref", "id": "object_id"}
                if (!child_dict.contains("id"))
                    Throw("[%s] Reference dictionary is missing 'id' attribute", path);

                state.nodes[parent_idx].props.set(
                    key, Properties::Reference(
                             nb::cast<std::string_view>(child_dict["id"])));
                continue;
            } else if (type == "rgb" || type == "spectrum") {
                // Handle special "rgb" and "spectrum" dictionaries
                parse_color_spectrum(state, parent_idx, key, child_dict, type, path);
                continue;
            } else if (type == "resources") {
                // "resources" declarations must be direct children of the root dictionary
                if (parent_idx != 0)
                    Throw("[%s] Declarations of resource folders must be done "
                          "at the root of the dictionary.", path);

                if (!child_dict.contains("path"))
                    Throw("[%s] Resource path is missing 'path' attribute", path);

                fs::path resource_path(nb::cast<std::string_view>(child_dict["path"]));

                if (!resource_path.is_absolute() && !fs::exists(resource_path))
                    resource_path = state.resolver->resolve(resource_path);

                if (!fs::exists(resource_path))
                    Throw("[%s] Folder \"%s\" not found", path, resource_path);

                state.resolver->prepend(resource_path);
                continue;
            }

            // Register the object for cross-referencing only when 'id' is
            // specified. Otherwise, still set the key as 'id' in the Properties
            // object for convenience (to more easily identify scene objects).
            bool has_id = child_dict.contains("id");
            std::string_view id{};
            if (has_id)
                id = nb::cast<std::string_view>(child_dict["id"]);

            // Create child node
            size_t child_idx = state.nodes.size();
            state.nodes.emplace_back();
            state.node_paths.push_back(path);
            SceneNode &child = state.nodes.back();

            // Set plugin name and object type
            child.props.set_plugin_name(type);
            child.type = PluginManager::instance()->plugin_type(type);
            child.props.set_id(has_id ? id : key);

            // Register explicit IDs in the global map
            if (has_id && !id.empty()) {
                auto [it, success] = state.id_to_index.try_emplace(std::string(id), child_idx);
                if (!success)
                    Throw("[%s] node has duplicate ID \"%s\"", path, id);
            }

            // The dictionary parser registers the dictionary path as an implicit ID as well
            if (!path.empty()) {
                auto [it, success] = state.id_to_index.try_emplace(path, child_idx);
                if (!success && it->second != child_idx)
                    Throw("[%s] path '%s' conflicts with existing identifier", path.c_str(), path.c_str());
            }

            // Reference from parent
            state.nodes[parent_idx].props.set(
                key, Properties::ResolvedReference(child_idx));

            parse_dict_impl(state, child_dict, child_idx, path);
        } else {
            {
                Object* obj_ptr;
                if (nb::try_cast<Object*>(value, obj_ptr))
                    obj_ptr->set_id(key);
            }

            try {
                nb::object props_o =
                    nb::cast(state.nodes[parent_idx].props, nb::rv_policy::reference);
                props_o[key_o] = value;
            } catch (...) {
                Throw(
                    "[%s] could not assign an object of type '%s' to key '%s', "
                    "as the value was not convertible any of the supported "
                    "property types (e.g. int/float/color/vector/...). It also "
                    "could be stored as an property of type 'any', as the "
                    "underlying type was not exposed using the nanobind "
                    "library. Did you mean to cast this value into a Dr.Jit "
                    "array before assigning it?",
                    path, nb::inst_name(value).c_str(), key);
            }
        }
    }
}

static ParserState parse_dict(const ParserConfig &, const nb::dict &d) {
    ParserState state;
    state.resolver = new FileResolver(*file_resolver());

    // Create root node
    SceneNode root;

    // Check if the root dict has a type
    std::string_view plugin_name =
        d.contains("type") ? nb::cast<std::string_view>(d["type"]) : "scene";
    root.props.set_plugin_name(plugin_name);

    // Handle dictionary with type="rgb" or type="spectrum" at the root level specially
    if (plugin_name == "rgb" || plugin_name == "spectrum") {
        root.type = ObjectType::Texture;
        state.nodes.push_back(std::move(root));
        state.node_paths.push_back("root");
        parse_color_spectrum(state, 0, "value", d, plugin_name, "root");
        return state;
    } else {
        root.type = PluginManager::instance()->plugin_type(root.props.plugin_name());
    }

    if (d.contains("id")) {
        std::string id = nb::cast<std::string>(d["id"]);
        root.props.set_id(id);
        state.id_to_index[id] = 0;
    }

    state.nodes.push_back(std::move(root));
    state.node_paths.push_back("root");

    // Parse dictionary entries recursively, but first validate all top-level keys
    for (auto [key_o, value] : d) {
        if (!nb::isinstance<nb::str>(key_o))
            Throw("Dictionary keys must be strings");

        std::string_view key = nb::cast<std::string_view>(key_o);

        // Skip keys that are already processed
        if (key == "type" || key == "id")
            continue;

        // '.' characters are reserved for path delimiters
        if (key.find('.') != std::string_view::npos)
            Throw("The object key '%s' contains a '.' character, which is "
                  "reserved as a delimiter in object paths. Please use '_' instead.", key);
    }

    parse_dict_impl(state, d, 0, "root");

    return state;
}

// Helper function to convert kwargs to ParameterList
static ParameterList convert_param_list(const nb::kwargs &kwargs) {
    ParameterList params;
    params.reserve(kwargs.size());
    for (auto [k, v] : kwargs)
        params.emplace_back(nb::cast<std::string>(k),
                            nb::cast<std::string>(nb::str(v)));
    return params;
}

/// Depending on whether or not the input vector has size 1, returns the first
/// and only element of the vector or the entire vector as a list
static nb::object single_object_or_list(std::vector<ref<Object>> &objects) {
    if (objects.size() == 1)
        return cast_object(objects[0]);

    nb::list l;
    for (ref<Object> &obj : objects)
        l.append(cast_object(obj));
    return l;
}

/// Get the current variant
static nb::str get_variant_str() {
    nb::object variant = nb::module_::import_("mitsuba").attr("variant")();
    if (variant.is_none())
        nb::raise("No variant was set!");
    return nb::borrow<nb::str>(std::move(variant));
}

MI_PY_EXPORT(parser) {
    // Create parser submodule
    auto parser = m.def_submodule("parser", "Scene parsing infrastructure");

    // Export ParserConfig
    nb::class_<ParserConfig>(parser, "ParserConfig", D(parser, ParserConfig))
        .def(nb::init<std::string_view>(), "variant"_a,
             D(parser, ParserConfig, ParserConfig))
        .def_rw("unused_parameters", &ParserConfig::unused_parameters,
                D(parser, ParserConfig, unused_parameters))
        .def_rw("unused_properties", &ParserConfig::unused_properties,
                D(parser, ParserConfig, unused_properties))
        .def_rw("max_include_depth", &ParserConfig::max_include_depth,
                D(parser, ParserConfig, max_include_depth))
        .def_rw("variant", &ParserConfig::variant,
                D(parser, ParserConfig, variant))
        .def_rw("parallel", &ParserConfig::parallel,
                D(parser, ParserConfig, parallel))
        .def_rw("merge_equivalent", &ParserConfig::merge_equivalent,
                D(parser, ParserConfig, merge_equivalent))
        .def_rw("merge_meshes", &ParserConfig::merge_meshes,
                D(parser, ParserConfig, merge_meshes));

    // Export SceneNode
    nb::class_<SceneNode>(parser, "SceneNode", D(parser, SceneNode))
        .def(nb::init<>())
        .def(nb::init<const SceneNode &>(), "Copy constructor")
        .def_rw("type", &SceneNode::type, D(parser, SceneNode, type))
        .def_rw("file_index", &SceneNode::file_index, D(parser, SceneNode, file_index))
        .def_rw("offset", &SceneNode::offset, D(parser, SceneNode, offset))
        .def_rw("props", &SceneNode::props, D(parser, SceneNode, props))
        .def("__eq__", &SceneNode::operator==)
        .def("__ne__", &SceneNode::operator!=);

    // Bind std::vector<SceneNode> as a Python list-like type
    nb::bind_vector<std::vector<SceneNode>, nb::rv_policy::reference_internal>(
        parser, "SceneNodeList");

    // Export ParserState
    nb::class_<ParserState>(parser, "ParserState", D(parser, ParserState))
        .def(nb::init<>())
        .def(nb::init<const ParserState &>(), "Copy constructor")
        .def_rw("nodes", &ParserState::nodes, D(parser, ParserState, nodes))
        .def_rw("node_paths", &ParserState::node_paths, D(parser, ParserState, node_paths))
        .def_rw("files", &ParserState::files, D(parser, ParserState, files))
        .def_rw("resolver", &ParserState::resolver, D(parser, ParserState, resolver))
        .def_prop_rw("id_to_index",
            [](const ParserState &s) {
                nb::dict result;
                for (const auto &[key, value] : s.id_to_index)
                    result[nb::str(key.c_str())] = value;
                return result;
            },
            [](ParserState &s, const nb::dict &d) {
                s.id_to_index.clear();
                for (auto [key, value] : d) {
                    s.id_to_index[nb::cast<std::string>(key)] = nb::cast<size_t>(value);
                }
            },
            D(parser, ParserState, id_to_index))
        .def_rw("versions", &ParserState::versions, D(parser, ParserState, versions))
        .def_prop_ro("root",
            [](ParserState &s) -> SceneNode & {
                if (s.nodes.empty())
                    nb::raise("ParserState: there is no root node!");
                return s.root();
            },
            nb::rv_policy::reference_internal,
            D(parser, ParserState, root))
        .def("__eq__", &ParserState::operator==)
        .def("__ne__", &ParserState::operator!=);

    // Export functions
    parser.def("parse_file",
          [](const ParserConfig &config, std::string_view filename, nb::kwargs kwargs) {
              return parse_file(config, fs::path(filename), convert_param_list(kwargs));
          },
          "config"_a, "filename"_a, "kwargs"_a,
          D(parser, parse_file));

    parser.def("parse_string",
          [](const ParserConfig &config, std::string_view string, nb::kwargs kwargs) {
              return parse_string(config, string, convert_param_list(kwargs));
          },
          "config"_a, "string"_a, "kwargs"_a,
          D(parser, parse_string));

    parser.def("parse_dict", &parse_dict,
          "config"_a, "dict"_a,
          "Parse a scene from a Python dictionary");

    parser.def("transform_upgrade", &transform_upgrade,
          "config"_a, "state"_a,
          D(parser, transform_upgrade));

    parser.def("transform_resolve_references", &transform_resolve,
          "config"_a, "state"_a,
          D(parser, transform_resolve));

    parser.def("transform_resolve", &transform_resolve,
          "config"_a, "state"_a,
          D(parser, transform_resolve));

    parser.def("transform_merge_equivalent", &transform_merge_equivalent,
          "config"_a, "state"_a,
          D(parser, transform_merge_equivalent));

    parser.def("transform_merge_meshes", &transform_merge_meshes,
          "config"_a, "state"_a,
          D(parser, transform_merge_meshes));

    parser.def("transform_reorder", &transform_reorder,
          "config"_a, "state"_a,
          D(parser, transform_reorder));

    parser.def("transform_relocate", &transform_relocate,
          "config"_a, "state"_a, "output_directory"_a,
          D(parser, transform_relocate));

    parser.def("transform_all", &transform_all,
          "config"_a, "state"_a,
          D(parser, transform_all));

    parser.def("file_location", &file_location,
          "state"_a, "node"_a,
          D(parser, file_location));

    parser.def("instantiate",
          [](const ParserConfig &config, ParserState &state) -> nb::object {
              std::vector<ref<Object>> objects;
              {
                  nb::gil_scoped_release release;
                  objects = instantiate(config, state);
              }
              return single_object_or_list(objects);
          },
          "config"_a, "state"_a,
          D(parser, instantiate));

    parser.def("write_file", &write_file,
          "state"_a, "filename"_a, "add_section_headers"_a = false,
          D(parser, write_file));

    parser.def("write_string", &write_string,
          "state"_a, "add_section_headers"_a = false,
          D(parser, write_string));

    // ======================== Main interface ========================

    m.def(
        "load_file",
        [](const std::string &name, bool parallel, bool optimize, nb::kwargs kwargs) {
            parser::ParameterList params = convert_param_list(kwargs);
            nb::str variant_str = get_variant_str();
            parser::ParserConfig config(variant_str.c_str());

            config.parallel = parallel;
            config.merge_equivalent = optimize;
            config.merge_meshes = optimize;

            std::vector<ref<Object>> objects;
            {
                nb::gil_scoped_release release;
                parser::ParserState state = parser::parse_file(config, name, params);
                parser::transform_all(config, state);
                objects = parser::instantiate(config, state);
            }

            return single_object_or_list(objects);
        },
        "path"_a, "parallel"_a = true, "optimize"_a = true, "kwargs"_a,
        R"doc(Load a Mitsuba scene or object from an XML file

Parameter ``name``:
    The XML scene description's filename

Parameter ``parallel``:
    Whether the loading should be executed on multiple threads in parallel

Parameter ``optimize``:
    Whether to enable optimizations like merging identical objects (default: True)

Parameter ``kwargs``:
    A dictionary of key value pairs that will replace any default parameters declared in the XML.

)doc");


    m.def(
        "load_string",
        [](const std::string &value, bool parallel, bool optimize, nb::kwargs kwargs) {
            parser::ParameterList params = convert_param_list(kwargs);
            nb::str variant_str = get_variant_str();
            parser::ParserConfig config(variant_str.c_str());
            config.parallel = parallel;
            config.merge_equivalent = optimize;
            config.merge_meshes = optimize;

            std::vector<ref<Object>> objects;
            {
                nb::gil_scoped_release release;
                parser::ParserState state = parser::parse_string(config, value, params);
                parser::transform_all(config, state);
                objects = parser::instantiate(config, state);
            }

            return single_object_or_list(objects);
        },
        "value"_a, "parallel"_a = true, "optimize"_a = true, "kwargs"_a,
        R"doc(Load a Mitsuba scene or object from an XML string

Parameter ``value``:
    The XML scene description as a string

Parameter ``parallel``:
    Whether the loading should be executed on multiple threads in parallel

Parameter ``optimize``:
    Whether to enable optimizations like merging identical objects (default: True)

Parameter ``kwargs``:
    A dictionary of key value pairs that will replace any default parameters declared in the XML.

)doc");

    m.def(
        "load_dict",
        [](const nb::dict &dict, bool parallel, bool optimize) {
            nb::str variant_str = get_variant_str();
            parser::ParserConfig config(variant_str.c_str());
            config.parallel = parallel;
            config.merge_equivalent = optimize;
            config.merge_meshes = optimize;

            // Parse, transform, and instantiate
            parser::ParserState state = parse_dict(config, dict);

            std::vector<ref<Object>> objects;
            {
                nb::gil_scoped_release release;
                parser::transform_all(config, state);
                objects = parser::instantiate(config, state);
            }

            return single_object_or_list(objects);
        },
        "dict"_a, "parallel"_a=true, "optimize"_a=true,
        R"doc(Load a Mitsuba scene or object from an Python dictionary

Parameter ``dict``:
    Python dictionary containing the object description

Parameter ``parallel``:
    Whether the loading should be executed on multiple threads in parallel

Parameter ``optimize``:
    Whether to enable optimizations like merging identical objects (default: True)

)doc");
}
