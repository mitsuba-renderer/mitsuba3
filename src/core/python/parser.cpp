#include <mitsuba/core/parser.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/python/python.h>
#include "properties.h"
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/pair.h>

// Import core types
using namespace mitsuba::parser;
using Vector3f = mitsuba::Vector<double, 3>;
using Color3f = mitsuba::Color<double, 3>;
using Transform4f = mitsuba::Transform<mitsuba::Point<double, 4>>;

// Forward declaration
static void parse_dict_impl(ParserState &state, nb::dict d, uint32_t parent_idx, std::string_view path = "");

ParserState parse_dict(nb::dict d) {
    ParserState state;

    // Create root node
    SceneNode root;

    // Check if the root dict has a type
    if (!d.contains("type"))
        root.props.set_plugin_name("scene");
    else
        root.props.set_plugin_name(nb::cast<std::string_view>(d["type"]));

    // Set object type for root
    root.type = PluginManager::instance()->plugin_type(root.props.plugin_name());

    if (d.contains("id")) {
        std::string id = nb::cast<std::string>(d["id"]);
        root.props.set_id(id);
        state.id_to_index[id] = 0;
    }
    state.nodes.push_back(root);

    // Parse dictionary entries recursively, but first validate all top-level keys
    for (auto [key_o, value] : d) {
        if (!key_o.type().is(&PyUnicode_Type))
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

    parse_dict_impl(state, d, 0, "scene");

    return state;
}

// Helper function to parse dictionary entries recursively
static void parse_dict_impl(ParserState &state, nb::dict d, uint32_t parent_idx, std::string_view path_) {
    for (auto [key_o, value] : d) {
        if (!key_o.type().is(&PyUnicode_Type))
            Throw("[%s]: dictionary keys must be strings", path_);

        std::string_view key = nb::cast<std::string_view>(key_o);

        // Skip the "type" and "id" keys as they're already processed
        if (key == "type" || key == "id")
            continue;

        // Validate key name - dots are reserved for path delimiters
        if (key.find('.') != std::string_view::npos)
            Throw("[%s] The object key '%s' contains a '.' character, which is "
                  "reserved as a delimiter in object paths. Please use '_' instead.",
                  path_, key);

        // Build the current property path
        std::string path = path_.empty()
                               ? std::string(key)
                               : std::string(path_) + "." + std::string(key);

        nb::handle tp = value.type();

        // Check if value is a nested dictionary (child object)
        if (tp.is(&PyDict_Type)) {
            nb::dict child_dict = nb::borrow<nb::dict>(value);

            if (!child_dict.contains("type"))
                Throw("[%s] Child object is missing 'type' attribute", path);

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
            }

            // Create child node
            SceneNode child;
            child.props.set_plugin_name(type);

            // Determine object type from plugin name
            auto plugin_mgr = PluginManager::instance();
            child.type = plugin_mgr->plugin_type(type);
            uint32_t child_idx = (uint32_t) state.nodes.size();

            // Set ID if present
            if (child_dict.contains("id")) {
                std::string id = nb::cast<std::string>(child_dict["id"]);
                child.props.set_id(id);
                state.id_to_index[id] = child_idx;
            }

            // Add as child of parent
            state.nodes.push_back(child);
            state.nodes[parent_idx].children.push_back({std::string(key), child_idx});

            // Recursively parse the child's properties
            parse_dict_impl(state, child_dict, child_idx, path);
        } else if (tp.is(&PyBool_Type)) {
            state.nodes[parent_idx].props.set(key, nb::cast<bool>(value));
        } else if (tp.is(&PyFloat_Type)) {
            state.nodes[parent_idx].props.set(key, nb::cast<double>(value));
        } else if (tp.is(&PyLong_Type)) {
            state.nodes[parent_idx].props.set(key, nb::cast<int64_t>(value));
        } else if (tp.is(&PyUnicode_Type)) {
            state.nodes[parent_idx].props.set(key, nb::cast<std::string>(value));
        } else if (tp.is(&PyList_Type) || tp.is(&PyTuple_Type)) {
            size_t len = nb::len(value);
            if (len == 3) {
                // Assume it's a vector/point/color
                Vector3f vec(nb::cast<double>(value[0]),
                            nb::cast<double>(value[1]),
                            nb::cast<double>(value[2]));
                state.nodes[parent_idx].props.set(key, vec);
            } else {
                Log(Warn, "[%s] Unsupported list size %zu", path, len);
            }
        } else {
            {
                Transform4f transform;
                if (nb::try_cast<Transform4f>(value, transform)) {
                    state.nodes[parent_idx].props.set(key, transform);
                    continue;
                }
            }

            {
                Color3f color;
                if (nb::try_cast<Color3f>(value, color)) {
                    state.nodes[parent_idx].props.set(key, color);
                    continue;
                }
            }

            // Try to cast to Object first (for Mitsuba objects like textures)
            {
                Object* obj_ptr = nullptr;
                if (nb::try_cast<Object*>(value, obj_ptr) && obj_ptr) {
                    state.nodes[parent_idx].props.set(key, ref<Object>(obj_ptr));
                    continue;
                }
            }

            // Try to store as an arbitrary Python object using the Any type
            try {
                state.nodes[parent_idx].props.set(key, any_wrap(value));
            } catch (const std::exception &e) {
                Log(Warn, "[%s] Failed to store value of type %s: %s",
                    path, nb::str(value.type()).c_str(), e.what());
            }
        }
    }
}


// Helper function to convert kwargs to ParameterList
static mitsuba::parser::ParameterList convert_param_list(const nb::kwargs &kwargs) {
    mitsuba::parser::ParameterList param;
    for (auto [k, v] : kwargs) {
        param.emplace_back(
            nb::str(k).c_str(),
            nb::str(v).c_str()
        );
    }
    return param;
}


MI_PY_EXPORT(parser) {
    using namespace mitsuba::parser;

    // Create parser submodule
    auto parser = m.def_submodule("parser", "Scene parsing infrastructure");

    // Export enums
    nb::enum_<ColorMode>(parser, "ColorMode")
        .value("Monochromatic", ColorMode::Monochromatic, "Convert all colors to grayscale")
        .value("RGB", ColorMode::RGB, "Keep colors as RGB triplets")
        .value("Spectral", ColorMode::Spectral, "Use full spectral representation");

    // Export ParserConfig
    nb::class_<ParserConfig>(parser, "ParserConfig")
        .def(nb::init<>())
        .def_rw("unused_parameters", &ParserConfig::unused_parameters,
                "How to handle unused parameters: Error (default), Warn, or Debug")
        .def_rw("max_include_depth", &ParserConfig::max_include_depth,
                "Maximum include depth to prevent infinite recursion (default: 15)");

    // Export SceneNode
    nb::class_<SceneNode>(parser, "SceneNode")
        .def(nb::init<>())
        .def_rw("type", &SceneNode::type, "Object type")
        .def_rw("file_index", &SceneNode::file_index, "File index in ParserState::files")
        .def_rw("offset", &SceneNode::offset, "Byte offset in file")
        .def_rw("props", &SceneNode::props, "Properties of this node")
        .def_rw("children", &SceneNode::children, "Indices of child nodes");

    // Export ParserState
    nb::class_<ParserState>(parser, "ParserState")
        .def(nb::init<>())
        .def_rw("nodes", &ParserState::nodes, "List of all scene nodes")
        .def_rw("files", &ParserState::files, "List of parsed files")
        .def_rw("id_to_index", &ParserState::id_to_index, "Map from IDs to node indices")
        .def_rw("versions", &ParserState::versions, "Version number for each file")
        .def_prop_ro("root",
            [](ParserState &s) -> SceneNode& { return s.root(); },
            nb::rv_policy::reference_internal,
            "Access the root node")
        .def("__getitem__",
            [](ParserState &s, uint32_t i) -> SceneNode& { return s[i]; },
            nb::rv_policy::reference_internal,
            "Return a node by index")
        .def("__len__",
            [](const ParserState &s) { return s.nodes.size(); },
            "Number of nodes");

    // Export functions
    parser.def("parse_file",
          [](std::string_view filename, const ParserConfig &config, nb::kwargs kwargs) {
              return parse_file(fs::path(filename), convert_param_list(kwargs), config);
          },
          "filename"_a, "config"_a = ParserConfig{}, "kwargs"_a,
          "Parse a scene from an XML file");

    parser.def("parse_string",
          [](std::string_view string, const ParserConfig &config, nb::kwargs kwargs) {
              return parse_string(string, convert_param_list(kwargs), config);
          },
          "string"_a, "config"_a = ParserConfig{}, "kwargs"_a,
          "Parse a scene from an XML string");

    parser.def("parse_dict", &parse_dict,
          "dict"_a,
          "Parse a scene from a Python dictionary");

    parser.def("transform_upgrade", &transform_upgrade,
          "ctx"_a,
          "Upgrade scene data to latest version");

    parser.def("transform_colors", &transform_colors,
          "ctx"_a, "variant"_a,
          "Apply color-related transformations");

    parser.def("file_location", &file_location,
          "ctx"_a, "node"_a,
          "Get human-readable file location for a node");

    parser.def("instantiate", &instantiate,
          "ctx"_a, "variant"_a, "parallel"_a = true,
          "Instantiate the scene graph", nb::rv_policy::move);

    parser.def("write_file", &write_file,
          "ctx"_a, "filename"_a,
          "Write scene data to an XML file");

    parser.def("write_string", &write_string,
          "ctx"_a,
          "Convert scene data to an XML string");
}
