#include <mitsuba/core/parser.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/vector.h>
#include <pugixml.hpp>
#include <cstdio>
#include <algorithm>
#include <string_view>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(parser)

using namespace std::string_view_literals;

// Import core types
using Vector3f = Vector<double, 3>;
using Color3f = Color<double, 3>;

namespace detail {
    // Helper function to parse integers and detact trailing characters
    static int64_t stoll(const std::string &s) {
        size_t offset = 0;
        int64_t result = std::stoll(s, &offset);

        for (size_t i = offset; i < s.size(); ++i) {
            if (!std::isspace(s[i]))
                Throw("Invalid trailing characters in integer \"%s\"", s);
        }

        return result;
    }
}


/// Tag types used in XML parsing to categorize different XML elements
enum class TagType {
    Boolean, Integer, Float, String, Point, Vector, Spectrum, RGB,
    Transform, Translate, Matrix, Rotate, Scale, LookAt, Object,
    NamedReference, Include, Alias, Default, Resource, Invalid
};

/// Interprets an XML tag name and returns its type classification
/// Returns a pair of (TagType, ObjectType) where TagType indicates the kind of XML element
/// and ObjectType specifies the plugin type for object elements
static std::pair<TagType, ObjectType> interpret_tag(std::string_view str) {
    // Property value tags
    if (str == "boolean") return {TagType::Boolean, ObjectType::Unknown};
    if (str == "integer") return {TagType::Integer, ObjectType::Unknown};
    if (str == "float") return {TagType::Float, ObjectType::Unknown};
    if (str == "string") return {TagType::String, ObjectType::Unknown};
    if (str == "point") return {TagType::Point, ObjectType::Unknown};
    if (str == "vector") return {TagType::Vector, ObjectType::Unknown};
    if (str == "spectrum") return {TagType::Spectrum, ObjectType::Unknown};
    if (str == "rgb") return {TagType::RGB, ObjectType::Unknown};
    if (str == "transform") return {TagType::Transform, ObjectType::Unknown};
    if (str == "translate") return {TagType::Translate, ObjectType::Unknown};
    if (str == "matrix") return {TagType::Matrix, ObjectType::Unknown};
    if (str == "rotate") return {TagType::Rotate, ObjectType::Unknown};
    if (str == "scale") return {TagType::Scale, ObjectType::Unknown};
    if (str == "lookat") return {TagType::LookAt, ObjectType::Unknown};
    if (str == "ref") return {TagType::NamedReference, ObjectType::Unknown};
    if (str == "include") return {TagType::Include, ObjectType::Unknown};
    if (str == "alias") return {TagType::Alias, ObjectType::Unknown};
    if (str == "default") return {TagType::Default, ObjectType::Unknown};
    if (str == "path") return {TagType::Resource, ObjectType::Unknown};

    // Object tags
    if (str == "scene") return {TagType::Object, ObjectType::Scene};
    if (str == "sensor") return {TagType::Object, ObjectType::Sensor};
    if (str == "film") return {TagType::Object, ObjectType::Film};
    if (str == "emitter") return {TagType::Object, ObjectType::Emitter};
    if (str == "sampler") return {TagType::Object, ObjectType::Sampler};
    if (str == "shape") return {TagType::Object, ObjectType::Shape};
    if (str == "texture") return {TagType::Object, ObjectType::Texture};
    if (str == "volume") return {TagType::Object, ObjectType::Volume};
    if (str == "medium") return {TagType::Object, ObjectType::Medium};
    if (str == "bsdf") return {TagType::Object, ObjectType::BSDF};
    if (str == "integrator") return {TagType::Object, ObjectType::Integrator};
    if (str == "phase") return {TagType::Object, ObjectType::PhaseFunction};
    if (str == "rfilter") return {TagType::Object, ObjectType::ReconstructionFilter};

    return {TagType::Invalid, ObjectType::Unknown};
}

// ParameterMap as described in TASK.md
using ParameterMap = tsl::robin_map<std::string_view, std::pair<std::string_view, bool>>;

// Sorted parameter list for efficient substitution
using SortedParameters = std::vector<std::pair<std::string, std::string_view>>;

// Forward declaration
static void parse_xml_node(ParserState &state, pugi::xml_node node,
                          uint32_t parent_idx, int depth,
                          ParameterMap *param_map = nullptr,
                          const SortedParameters *sorted_params = nullptr);

// Helper function to perform parameter substitution on a string
static std::string substitute_parameters(std::string_view input, ParameterMap &param_map,
                                        const SortedParameters &sorted_params,
                                        const ParserState &state, const SceneNode &node) {
    std::string result(input);
    
    // Check if string contains any '$' characters
    if (result.find('$') == std::string::npos)
        return result;
    
    // Perform substitutions using pre-sorted parameters
    for (const auto &[search_key, param_name] : sorted_params) {
        size_t pos = 0;
        while ((pos = result.find(search_key, pos)) != std::string::npos) {
            auto &[value, used] = param_map[param_name];
            result.replace(pos, search_key.length(), value);
            used = true;
            pos += value.length();
        }
    }
    
    // Check if there are still unresolved parameters
    size_t dollar_pos = result.find('$');
    if (dollar_pos != std::string::npos) {
        // Find the parameter name after '$'
        size_t end_pos = dollar_pos + 1;
        while (end_pos < result.size() && 
               (std::isalnum(result[end_pos]) || result[end_pos] == '_')) {
            ++end_pos;
        }
        std::string param_name = result.substr(dollar_pos + 1, end_pos - dollar_pos - 1);
        fail(state, node, "Undefined parameter: $%s", param_name);
    }
    
    return result;
}

ParserState parse_file(const fs::path &filename, const ParameterList &params) {
    Log(Debug, "parse_file called with filename: %s", filename.string());
    
    // Build parameter map and sorted parameter list
    ParameterMap param_map;
    SortedParameters sorted_params;
    
    for (const auto &[key, value] : params) {
        param_map[key] = {value, false};
        sorted_params.emplace_back("$" + key, key);
    }
    
    // Sort parameters by length (longest first) to handle cases where
    // one parameter name is a prefix of another
    std::sort(sorted_params.begin(), sorted_params.end(),
              [](const auto &a, const auto &b) {
                  return a.first.size() > b.first.size();
              });

    // Check if file exists
    if (!fs::exists(filename))
        Throw("File not found: %s", filename.string());

    ParserState state;

    // The root node will be created based on the actual root element in XML
    // (It's not pre-created as scene, this will be determined by the XML content)

    // Store filename
    state.files.push_back(filename);

    // Parse XML using pugixml's load_file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filename.native().c_str());

    if (!result) {
        Throw("XML parsing failed: %s (at offset %td)",
              result.description(), result.offset);
    }

    // Get the root element
    pugi::xml_node root_node = doc.document_element();

    // Check for version attribute
    if (root_node.attribute("version")) {
        std::string_view version_str = root_node.attribute("version").value();
        try {
            state.version = util::Version(std::string(version_str));
        } catch (...) {
            Throw("Invalid version string: %s", std::string(version_str).c_str());
        }
    } else {
        // Default to version 3.0.0 if not specified
        state.version = util::Version(3, 0, 0);
    }

    // Parse root node if it's not empty
    if (root_node) {
        // Parse the root element itself as the first node
        parse_xml_node(state, root_node, 0, 0, 
                      params.empty() ? nullptr : &param_map,
                      params.empty() ? nullptr : &sorted_params);
    }
    
    // Check for unused parameters and emit warnings
    for (const auto &[key, value_pair] : param_map) {
        if (!value_pair.second) {
            Log(Warn, "Unused parameter: '%s' = '%s'", std::string(key), std::string(value_pair.first));
        }
    }

    return state;
}

ParserState parse_string(std::string_view string, const ParameterList &params) {
    Log(Debug, "parse_string called with %zu characters", string.size());
    
    // Build parameter map and sorted parameter list
    ParameterMap param_map;
    SortedParameters sorted_params;
    
    for (const auto &[key, value] : params) {
        param_map[key] = {value, false};
        sorted_params.emplace_back("$" + key, key);
    }
    
    // Sort parameters by length (longest first) to handle cases where
    // one parameter name is a prefix of another
    std::sort(sorted_params.begin(), sorted_params.end(),
              [](const auto &a, const auto &b) {
                  return a.first.size() > b.first.size();
              });

    ParserState state;
    state.content = string;

    // Add a virtual filename for error reporting
    state.files.push_back("<string>");

    // Parse XML using pugixml
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(string.data(), string.size());

    if (!result) {
        Throw("XML parsing failed: %s (at offset %td)",
              result.description(), result.offset);
    }

    // Get the root element
    pugi::xml_node root_node = doc.document_element();

    // Check for version attribute
    if (root_node.attribute("version")) {
        std::string_view version_str = root_node.attribute("version").value();
        try {
            state.version = util::Version(std::string(version_str));
        } catch (...) {
            Throw("Invalid version string: %s", std::string(version_str).c_str());
        }
    } else {
        // Default to version 3.0.0 if not specified
        state.version = util::Version(3, 0, 0);
    }

    // Parse root node if it's not empty
    if (root_node) {
        // Parse the root element itself as the first node
        parse_xml_node(state, root_node, 0, 0, 
                      params.empty() ? nullptr : &param_map,
                      params.empty() ? nullptr : &sorted_params);
    }
    
    // Check for unused parameters and emit warnings
    for (const auto &[key, value_pair] : param_map) {
        if (!value_pair.second) {
            Log(Warn, "Unused parameter: '%s' = '%s'", std::string(key), std::string(value_pair.first));
        }
    }

    return state;
}

void transform_upgrade(ParserState &ctx) {
    Log(Info, "transform_upgrade called on %zu nodes", ctx.nodes.size());

    // TODO: Implement version upgrade transformations
    // - Convert camelCase to underscore_case for version < 2.0
    // - Update deprecated plugin names
    // - Migrate old parameter structures
}

void transform_colors(ParserState &ctx) {
    Log(Info, "transform_colors called with color mode: %s",
        ctx.color_mode == ColorMode::Monochromatic ? "monochromatic" :
        ctx.color_mode == ColorMode::RGB ? "rgb" : "spectral");

    // TODO: Implement color transformations
    // - RGB to monochromatic conversion
    // - Spectrum construction from RGB
    // - Insert upsampling/downsampling plugins
}


// Forward declaration
template <typename... Args>
[[noreturn]] static void fail(const ParserState &state, const SceneNode &node,
                             std::string_view msg, Args&&... args);

// Helper function to expand the 'value' attribute into x/y/z components
static void expand_value_to_xyz(pugi::xml_node &node) {
    auto value_attr = node.attribute("value");
    if (value_attr) {
        if (node.attribute("x") || node.attribute("y") || node.attribute("z"))
            Throw("Cannot mix 'value' and 'x'/'y'/'z' attributes");

        const char *value = value_attr.value();
        auto tokens = string::tokenize(value);

        if (tokens.size() == 1) {
            // Single value - replicate to all components
            node.append_attribute("x") = tokens[0].c_str();
            node.append_attribute("y") = tokens[0].c_str();
            node.append_attribute("z") = tokens[0].c_str();
        } else if (tokens.size() == 3) {
            // Three values - one for each component
            node.append_attribute("x") = tokens[0].c_str();
            node.append_attribute("y") = tokens[1].c_str();
            node.append_attribute("z") = tokens[2].c_str();
        } else {
            Throw("'value' attribute must have exactly 1 or 3 elements");
        }

        node.remove_attribute("value");
    }
}

// Helper function to parse a vector from x/y/z attributes
static Vector3f parse_vector(const ParserState &state, const SceneNode &node,
                           pugi::xml_node xml_node, double default_val = 0.0) {
    double x = default_val, y = default_val, z = default_val;

    pugi::xml_attribute xa = xml_node.attribute("x"),
                        ya = xml_node.attribute("y"),
                        za = xml_node.attribute("z");

    try {
        if (xa) x = string::stof<double>(xa.value());
        if (ya) y = string::stof<double>(ya.value());
        if (za) z = string::stof<double>(za.value());
    } catch (...) {
        fail(state, node, "Could not parse floating point value in vector");
    }

    return Vector3f(x, y, z);
}

// Helper function to convert a byte offset in a buffer to line:column information
static std::string compute_line_col(std::string_view buffer, size_t offset) {
    if (offset > buffer.size())
        return tfm::format("byte offset %zu", offset);

    size_t line = 1, line_start = 0;

    for (size_t i = 0; i < offset && i < buffer.size(); ++i) {
        if (buffer[i] == '\n') {
            ++line;
            line_start = i + 1;
        }
    }

    size_t col = offset - line_start + 1;
    return tfm::format("line %zu, col %zu", line, col);
}

/**
 * \brief Generate a human-readable file location string for error reporting
 *
 * Returns a string describing where a scene node was defined in the source file.
 * For XML files, this includes the filename and line:column information.
 * For dictionary-parsed nodes (which don't have location info), returns the node's ID
 * or a generic description.
 *
 * \param ctx Parser state containing file information
 * \param node Scene node to locate
 * \return Human-readable location string (e.g., "scene.xml (line 10, col 5)")
 */
std::string file_location(const ParserState &ctx, const SceneNode &node) {
    if (node.file_index >= ctx.files.size()) {
        // Otherwise return a generic description
        if (!node.props.id().empty())
            return tfm::format("node '%s'", node.props.id());
        else
            return "unknown location";
    }

    const fs::path& filepath = ctx.files[node.file_index];

    // If we have the content in memory (from parse_string), use it
    if (!ctx.content.empty()) {
        std::string line_col = compute_line_col(ctx.content, node.offset);
        return tfm::format("%s (%s)", filepath.string(), line_col);
    }

    // Read file content to compute line:column
    FILE* file = fopen(filepath.string().c_str(), "rb");
    if (file) {
        // Get file size
        fseek(file, 0, SEEK_END);
        size_t file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Read only what we need (up to offset + some buffer)
        size_t read_size = std::min(file_size, node.offset + 1024);
        std::string buffer(read_size, '\0');
        size_t bytes_read = fread(buffer.data(), 1, read_size, file);
        fclose(file);

        if (bytes_read >= node.offset) {
            buffer.resize(bytes_read);
            std::string line_col = compute_line_col(buffer, node.offset);
            return tfm::format("%s (%s)", filepath.string(), line_col);
        }
    }

    // Fallback to byte offset
    return tfm::format("%s (byte offset %zu)", filepath.string(), node.offset);
}

// Helper function to throw an error with file location information
template <typename... Args>
[[noreturn]] static void fail(const ParserState &state, const SceneNode &node,
                             std::string_view msg, Args&&... args) {
    std::string location = file_location(state, node);
    std::string formatted_msg = tfm::format(msg.data(), std::forward<Args>(args)...);
    Throw("Error while parsing %s: %s", location.c_str(), formatted_msg.c_str());
}

ref<Object> instantiate(ParserState &ctx, bool parallel) {
    Log(Info, "instantiate called with %zu nodes, parallel=%s",
        ctx.nodes.size(), parallel ? "true" : "false");

    if (ctx.nodes.empty())
        Throw("No nodes to instantiate");

    // TODO: Implement actual instantiation
    // For now, just return a dummy object
    auto plugin_mgr = PluginManager::instance();

    // Create a simple scene object as placeholder
    Properties props("scene");
    props.set_id("dummy_scene");

    // Use the variant from the parser state, default to scalar_rgb
    std::string variant = ctx.variant.empty() ? "scalar_rgb" : ctx.variant;
    return plugin_mgr->create_object(props, variant, ObjectType::Scene);
}

// Helper function to check if all required attributes are present
static void check_attributes(const ParserState &state, const SceneNode &node,
                             pugi::xml_node xml_node,
                             std::initializer_list<std::string_view> args) {
    bool *found = (bool *) alloca(sizeof(bool) * args.size());
    for (size_t i = 0; i < args.size(); ++i)
        found[i] = false;

    for (pugi::xml_attribute attr : xml_node.attributes()) {
        size_t i = 0;
        for (std::string_view arg : args) {
            if (arg == attr.name()) {
                found[i] = true;
                break;
            }
            ++i;
        }
    }

    size_t i = 0;
    for (std::string_view arg : args) {
        if (!found[i]) {
            fail(state, node, "Missing required attribute '%s' in <%s>",
                 std::string(arg), xml_node.name());
        }
        ++i;
    }
}

// Helper function to parse XML nodes recursively
static void parse_xml_node(ParserState &state, pugi::xml_node node,
                          uint32_t parent_idx, int depth,
                          ParameterMap *param_map,
                          const SortedParameters *sorted_params) {
    // Skip comments and other non-element nodes
    if (node.type() != pugi::node_element)
        return;

    // Apply parameter substitution to ALL attributes first
    if (param_map && sorted_params) {
        for (auto attr : node.attributes()) {
            std::string value = attr.value();
            
            // Skip attributes that don't contain '$'
            if (value.find('$') == std::string::npos)
                continue;
            
            // Create a temporary scene node for error reporting
            SceneNode temp_node;
            temp_node.file_index = 0;
            temp_node.offset = node.offset_debug();
            
            // Apply substitution
            value = substitute_parameters(value, *param_map, *sorted_params, state, temp_node);
            
            // Update the attribute with the substituted value
            attr.set_value(value.c_str());
        }
    }

    std::string_view node_name = node.name();
    auto [tag_type, object_type] = interpret_tag(node_name);

    if (tag_type == TagType::Invalid) {
        Throw("Unknown XML tag: %s", std::string(node_name).c_str());
    }

    // Create a new scene node
    SceneNode scene_node;
    scene_node.type = object_type;
    scene_node.file_index = 0;  // We only have one file for now
    scene_node.offset = node.offset_debug();

    // Handle different tag types
    switch (tag_type) {
        case TagType::Object: {
            // Objects need a type attribute, except for the root <scene> element
            auto type_attr = node.attribute("type");
            if (!type_attr && object_type != ObjectType::Scene) {
                fail(state, scene_node, "Missing 'type' attribute in <%s>",
                    std::string(node_name).c_str());
            }

            // For <scene> without type attribute, use "scene" as plugin name
            if (type_attr) {
                scene_node.props.set_plugin_name(type_attr.value());
            } else {
                scene_node.props.set_plugin_name("scene");
            }

            // Check for id attribute
            auto id_attr = node.attribute("id");
            if (id_attr) {
                scene_node.props.set_id(id_attr.value());
            }
            break;
        }

        case TagType::Float: {
            // Float tags need name and value attributes
            check_attributes(state, scene_node, node, {"name"sv, "value"sv});

            const char *name      = node.attribute("name").value(),
                       *value_str = node.attribute("value").value();

            try {
                double value = string::stof<double>(value_str);

                // For now, store in parent's properties
                if (parent_idx < state.nodes.size()) {
                    state.nodes[parent_idx].props.set(name, value);
                }
            } catch (...) {
                fail(state, scene_node, "Could not parse floating point value '%s'",
                    std::string(value_str).c_str());
            }

            // Float nodes don't need to be added to the node list
            return;
        }

        case TagType::Integer: {
            // Integer tags need name and value attributes
            check_attributes(state, scene_node, node, {"name"sv, "value"sv});

            const char *name      = node.attribute("name").value(),
                       *value_str = node.attribute("value").value();

            // Try to parse the integer value
            try {
                int64_t value = detail::stoll(value_str);

                // Store in parent's properties
                if (parent_idx < state.nodes.size()) {
                    state.nodes[parent_idx].props.set(name, value);
                }
            } catch (...) {
                fail(state, scene_node, "Could not parse integer value '%s'",
                    std::string(value_str).c_str());
            }

            // Integer nodes don't need to be added to the node list
            return;
        }

        case TagType::String: {
            // String tags need name and value attributes
            check_attributes(state, scene_node, node, {"name"sv, "value"sv});

            const char *name  = node.attribute("name").value(),
                       *value = node.attribute("value").value();

            // Store in parent's properties
            if (parent_idx < state.nodes.size()) {
                state.nodes[parent_idx].props.set(name, std::string(value));
            }

            // String nodes don't need to be added to the node list
            return;
        }

        case TagType::Vector:
        case TagType::Point: {
            // First expand value attribute if present
            expand_value_to_xyz(node);

            // Vector/Point tags need name and x/y/z attributes
            check_attributes(state, scene_node, node, {"name"sv, "x"sv, "y"sv, "z"sv});

            const char *name = node.attribute("name").value();
            Vector3f vec = parse_vector(state, scene_node, node);

            // Store in parent's properties
            if (parent_idx < state.nodes.size()) {
                state.nodes[parent_idx].props.set(name, vec);
            }

            // Vector/Point nodes don't need to be added to the node list
            return;
        }

        case TagType::RGB: {
            // RGB tags need name and value attributes
            check_attributes(state, scene_node, node, {"name"sv, "value"sv});

            const char *name  = node.attribute("name").value(),
                       *value = node.attribute("value").value();

            // Parse RGB values (1 or 3 components)
            auto tokens = string::tokenize(value);

            Color3f color;
            try {
                if (tokens.size() == 1) {
                    double v = string::stof<double>(tokens[0]);
                    color = Color3f(v, v, v);
                } else if (tokens.size() == 3) {
                    color = Color3f(string::stof<double>(tokens[0]),
                                    string::stof<double>(tokens[1]),
                                    string::stof<double>(tokens[2]));
                } else {
                    fail(state, scene_node, "'rgb' tag requires one or three values (got %zu)", tokens.size());
                }
            } catch (...) {
                fail(state, scene_node, "Could not parse RGB value '%s'", value);
            }

            // Store in parent's properties
            if (parent_idx < state.nodes.size()) {
                state.nodes[parent_idx].props.set(name, color);
            }

            // RGB nodes don't need to be added to the node list
            return;
        }

        default:
            // For now, ignore other tag types
            Log(Debug, "Ignoring tag type %d at depth %d", (int)tag_type, depth);
            return;
    }

    // Only add object nodes to the state (property nodes like float/int/string are handled above)
    if (tag_type == TagType::Object) {
        // Add the node to the state
        uint32_t node_idx = (uint32_t)state.nodes.size();
        state.nodes.push_back(scene_node);

        // If this is not the root element (depth > 0), add it as a child of the parent
        if (depth > 0 && parent_idx < state.nodes.size())
            state.nodes[parent_idx].children.push_back(node_idx);

        // Recursively parse child nodes
        for (pugi::xml_node child : node.children())
            parse_xml_node(state, child, node_idx, depth + 1, param_map, sorted_params);
    }
}

NAMESPACE_END(parser)
NAMESPACE_END(mitsuba)
