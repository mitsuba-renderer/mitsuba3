#include <mitsuba/core/parser.h>
#include <mitsuba/core/object.h>
#include <mitsuba/python/python.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

// Helper function to find "type" in dictionary
static std::string get_dict_type(nb::dict d) {
    if (d.contains("type")) {
        return nb::str(d["type"]).c_str();
    }
    return "";
}

// Implementation of parse_dict
namespace mitsuba::parser {
    ParserState parse_dict(nb::dict d) {
        Log(Debug, "parse_dict called with dictionary");

        ParserState state;

        // Create root node
        SceneNode root;

        // Check if the root dict has a type
        std::string root_type = get_dict_type(d);
        if (root_type.empty()) {
            root.props.set_plugin_name("scene");
        } else {
            root.props.set_plugin_name(root_type);
        }

        root.props.set_id("root");
        state.nodes.push_back(root);

        // TODO: Parse nested objects
        // For now, just iterate through the dictionary
        for (auto [key, value] : d) {
            std::string key_str = nb::str(key).c_str();

            // Skip the "type" key as it's already processed
            if (key_str == "type")
                continue;

            // Check if value is a nested dictionary (child object)
            if (nb::isinstance<nb::dict>(value)) {
                nb::dict child_dict = nb::cast<nb::dict>(value);
                std::string child_type = get_dict_type(child_dict);

                if (child_type.empty()) {
                    Throw("Child object '%s' is missing 'type' attribute", key_str.c_str());
                }

                // TODO: Create child node and add to parent
            }
            // TODO: Handle other property types
        }

        return state;
    }
}

// Helper function to convert kwargs to ParameterList
static mitsuba::parser::ParameterList convert_param_list(const nb::kwargs &kwargs) {
    mitsuba::parser::ParameterList param;
    if (kwargs) {
        for (auto [k, v] : kwargs) {
            param.emplace_back(
                nb::str(k).c_str(),
                nb::str(v).c_str()
            );
        }
    }
    return param;
}

MI_PY_EXPORT(parser) {
    using namespace mitsuba::parser;

    // Export enums
    nb::enum_<ColorMode>(m, "ColorMode")
        .value("Monochromatic", ColorMode::Monochromatic, "Convert all colors to grayscale")
        .value("RGB", ColorMode::RGB, "Keep colors as RGB triplets")
        .value("Spectral", ColorMode::Spectral, "Use full spectral representation");

    // Export SceneNode
    nb::class_<SceneNode>(m, "SceneNode")
        .def(nb::init<>())
        .def_rw("type", &SceneNode::type, "Object type")
        .def_rw("file_index", &SceneNode::file_index, "File index in ParserState::files")
        .def_rw("offset", &SceneNode::offset, "Byte offset in file")
        .def_rw("props", &SceneNode::props, "Properties of this node")
        .def_rw("children", &SceneNode::children, "Indices of child nodes")
        .def_rw("property_name", &SceneNode::property_name, "Property name for parent attachment");

    // Export ParserState
    nb::class_<ParserState>(m, "ParserState")
        .def(nb::init<>())
        .def_rw("nodes", &ParserState::nodes, "List of all scene nodes")
        .def_rw("files", &ParserState::files, "List of parsed files")
        .def_rw("id_to_index", &ParserState::id_to_index, "Map from IDs to node indices")
        .def_rw("color_mode", &ParserState::color_mode, "Color processing mode")
        .def_rw("backend", &ParserState::backend, "JIT backend")
        .def_rw("variant", &ParserState::variant, "Variant name")
        .def_rw("version", &ParserState::version, "File version")
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
    m.def("parse_file",
          [](std::string_view filename, nb::kwargs kwargs) {
              return parse_file(fs::path(filename), convert_param_list(kwargs));
          },
          "filename"_a, "kwargs"_a,
          "Parse a scene from an XML file");

    m.def("parse_string",
          [](std::string_view string, nb::kwargs kwargs) {
              return parse_string(string, convert_param_list(kwargs));
          },
          "string"_a, "kwargs"_a,
          "Parse a scene from an XML string");

    m.def("parse_dict", &parse_dict,
          "d"_a,
          "Parse a scene from a Python dictionary");

    m.def("transform_upgrade", &transform_upgrade,
          "ctx"_a,
          "Upgrade scene data to the latest version");

    m.def("transform_colors", &transform_colors,
          "ctx"_a,
          "Apply color-related transformations");

    m.def("file_location", &file_location,
          "ctx"_a, "node"_a,
          "Get human-readable file location for a node");

    m.def("instantiate", &instantiate,
          "ctx"_a, "parallel"_a = true,
          "Instantiate the scene graph", nb::rv_policy::move);
}
