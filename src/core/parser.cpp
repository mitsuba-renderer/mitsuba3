#include <mitsuba/core/parser.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/formatter.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/frame.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/mitsuba.h>
#include <pugixml.hpp>
#include <algorithm>
#include <string_view>
#include <charconv>
#include <mutex>
#include <tuple>
#include <nanothread/nanothread.h>

using namespace std::string_view_literals;

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(parser)

// Parsing is variant-independent and must therefore store properties
// using the highest possibly needed precision (i.e., double precision)
using ScalarVector3d    = Vector<double, 3>;
using ScalarColor3d     = Color<double, 3>;
using ScalarPoint3d     = Point<double, 3>;
using ScalarPoint4d     = Point<double, 4>;
using ScalarMatrix4d    = dr::Matrix<double, 4>;
using ScalarAffineTransform4d = AffineTransform<ScalarPoint4d>;

/// A list of all tag types that can be encountered in an XML file (excluding the various object type tags)
enum class TagType {
    Boolean, Integer, Float, String, Point, Vector, Spectrum, RGB,
    Transform, Translate, Matrix, Rotate, Scale, LookAt, Object,
    NamedReference, Include, Alias, Default, Resource, Invalid
};

/**
 * \brief Structure to track parameter substitutions during XML parsing
 *
 * This structure holds information about a single parameter that can be
 * substituted in XML attributes using the $parameter_name syntax.
 */
struct SortedParameter {
    std::string key;         ///< The search key including dollar prefix (e.g., "$myParam")
    std::string value;       ///< The replacement value (must be string, not string_view, for defaults)
    bool used = false;       ///< Track whether this parameter was used
};

/// List of parameters sorted by search key length (longest first) for correct substitution
using SortedParameters = std::vector<SortedParameter>;

// Forward declaration
template <typename... Args>
[[noreturn]] static void fail(const ParserState &state, const SceneNode &node,
                              const char *, Args&&... args);

static ParserState parse_file_impl(const ParserConfig &config, const fs::path &filename,
                                   SortedParameters &params, int depth);

// Helper function to check that all and only the specified attributes are present
// Required attributes are prefixed with '!', optional ones without
static void check_attributes(const ParserState &state, const SceneNode &node,
                             pugi::xml_node xml_node,
                             std::initializer_list<std::string_view> args) {
    // Check all attributes are recognized
    for (auto attr : xml_node.attributes()) {
        bool found = false;
        for (auto arg : args) {
            if ((arg[0] == '!' ? arg.substr(1) : arg) == attr.name()) {
                found = true;
                break;
            }
        }

        if (!found)
            fail(state, node, "unexpected attribute \"%s\" in element <%s>",
                 attr.name(), xml_node.name());
    }

    // Check required attributes are present
    for (auto arg : args) {
        if (arg[0] != '!')
            continue;

        std::string_view name = arg.substr(1);
        if (!xml_node.attribute(name))
            fail(state, node, "missing required attribute \"%s\" in <%s>",
                 std::string(name), xml_node.name());
    }
}

// Helper function to check for unused parameters and report them
static void check_unused_parameters(const ParserConfig &config,
                                    const SortedParameters &params) {
    std::string message;

    for (const auto &param : params) {
        if (param.used)
            continue;
        message += tfm::format("  - %s=%s\n", param.key, param.value);
    }

    if (!message.empty()) {
        message.pop_back(); // Remove trailing newline
        Log(config.unused_parameters, "Found unused parameters:\n%s", message);
    }
}

/**
 * \brief Interprets an XML tag name and returns its type classification
 *
 * This function takes an XML tag name and returns a pair containing:
 * - For property tags (e.g., "float", "rgb", "transform"):
 *   returns (appropriate TagType, ObjectType::Unknown)
 * - For object tags (e.g., "scene", "bsdf", "shape"):
 *   returns (TagType::Object, appropriate ObjectType)
 *
 * \param str The XML tag name to interpret
 * \return A pair of (TagType, ObjectType) indicating the tag classification
 */
static std::pair<TagType, ObjectType> interpret_tag(std::string_view str) {
    if (str.empty())
        return {TagType::Invalid, ObjectType::Unknown};

    // Switch on first character to reduce the number of necessary comparisons
    switch (str[0]) {
        case 'a':
            if (str == "alias") return {TagType::Alias, ObjectType::Unknown};
            break;
        case 'b':
            if (str == "boolean") return {TagType::Boolean, ObjectType::Unknown};
            if (str == "bsdf") return {TagType::Object, ObjectType::BSDF};
            break;
        case 'd':
            if (str == "default") return {TagType::Default, ObjectType::Unknown};
            break;
        case 'e':
            if (str == "emitter") return {TagType::Object, ObjectType::Emitter};
            break;
        case 'f':
            if (str == "float") return {TagType::Float, ObjectType::Unknown};
            if (str == "film") return {TagType::Object, ObjectType::Film};
            break;
        case 'i':
            if (str == "integer") return {TagType::Integer, ObjectType::Unknown};
            if (str == "include") return {TagType::Include, ObjectType::Unknown};
            if (str == "integrator") return {TagType::Object, ObjectType::Integrator};
            break;
        case 'l':
            if (str == "lookat") return {TagType::LookAt, ObjectType::Unknown};
            break;
        case 'm':
            if (str == "matrix") return {TagType::Matrix, ObjectType::Unknown};
            if (str == "medium") return {TagType::Object, ObjectType::Medium};
            break;
        case 'p':
            if (str == "point") return {TagType::Point, ObjectType::Unknown};
            if (str == "path") return {TagType::Resource, ObjectType::Unknown};
            if (str == "phase") return {TagType::Object, ObjectType::PhaseFunction};
            break;
        case 'r':
            if (str == "rgb") return {TagType::RGB, ObjectType::Unknown};
            if (str == "rotate") return {TagType::Rotate, ObjectType::Unknown};
            if (str == "ref") return {TagType::NamedReference, ObjectType::Unknown};
            if (str == "rfilter") return {TagType::Object, ObjectType::ReconstructionFilter};
            break;
        case 's':
            if (str == "string") return {TagType::String, ObjectType::Unknown};
            if (str == "spectrum") return {TagType::Spectrum, ObjectType::Unknown};
            if (str == "scale") return {TagType::Scale, ObjectType::Unknown};
            if (str == "scene") return {TagType::Object, ObjectType::Scene};
            if (str == "sensor") return {TagType::Object, ObjectType::Sensor};
            if (str == "sampler") return {TagType::Object, ObjectType::Sampler};
            if (str == "shape") return {TagType::Object, ObjectType::Shape};
            break;
        case 't':
            if (str == "transform") return {TagType::Transform, ObjectType::Unknown};
            if (str == "translate") return {TagType::Translate, ObjectType::Unknown};
            if (str == "texture") return {TagType::Object, ObjectType::Texture};
            break;
        case 'v':
            if (str == "vector") return {TagType::Vector, ObjectType::Unknown};
            if (str == "volume") return {TagType::Object, ObjectType::Volume};
            break;
    }

    return {TagType::Invalid, ObjectType::Unknown};
}

/**
 * \brief Perform parameter substitution on an XML attribute in-place
 *
 * This function modifies the attribute value directly, replacing any
 * occurrences of $parameter_name with the corresponding parameter value.
 *
 * \param xml_node The XML node containing the attribute (for error location)
 * \param attr The XML attribute to modify
 * \param params List of parameters sorted by search key length
 * \param state Parser state for error reporting
 * \param file_index File index for error location
 */
static void substitute_parameters(pugi::xml_node xml_node,
                                  pugi::xml_attribute &attr,
                                  SortedParameters &params,
                                  const ParserState &state,
                                  uint32_t file_index) {
    std::string value = attr.value();

    // Perform substitutions using pre-sorted parameters
    for (auto &param : params) {
        size_t pos = 0;
        while ((pos = value.find(param.key, pos)) != std::string::npos) {
            // Check if the dollar sign is escaped with a backslash
            if (pos > 0 && value[pos - 1] == '\\') {
                pos += param.key.length();
            } else {
                value.replace(pos, param.key.length(), param.value);
                param.used = true;
                pos += param.value.length();
            }
        }
    }

    // Check if there are still unresolved parameters
    size_t pos = 0;
    while ((pos = value.find('$', pos)) != std::string::npos) {
        // Check if the dollar sign is escaped
        if (pos > 0 && value[pos - 1] == '\\') {
            pos++;
            continue;
        }

        // Find the parameter name after '$'
        size_t end_pos = pos + 1;
        while (end_pos < value.size() &&
               (std::isalnum(value[end_pos]) || value[end_pos] == '_')) {
            ++end_pos;
        }
        std::string param_name = value.substr(pos + 1, end_pos - pos - 1);

        // Create temporary node for error reporting
        SceneNode temp_node;
        temp_node.file_index = file_index;
        temp_node.offset = xml_node.offset_debug();
        fail(state, temp_node, "undefined parameter: $%s", param_name);
    }

    // Remove escape characters from escaped dollar signs
    pos = 0;
    while ((pos = value.find("\\$", pos)) != std::string::npos) {
        value.erase(pos, 1); // Remove the backslash
        pos++; // Move past the dollar sign
    }

    // Update the attribute with the substituted value
    attr.set_value(value);
}

/// Helper function to format line:column information from a buffer
static std::string line_col_from_string(std::string_view buffer, size_t offset) {
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

// Helper function to compute line:column from a file
static std::string line_col_from_file(const fs::path& filepath, size_t offset) {
    FILE* file = fopen(filepath.string().c_str(), "rb");
    if (!file)
        return tfm::format("byte offset %zu", offset);

    // Get file size
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read only what we need (up to offset + some buffer)
    size_t read_size = std::min(file_size, offset + 1024);
    std::string buffer(read_size, '\0');
    size_t bytes_read = fread(buffer.data(), 1, read_size, file);
    fclose(file);

    if (bytes_read < offset)
        return tfm::format("byte offset %zu", offset);

    buffer.resize(bytes_read);
    return line_col_from_string(buffer, offset);
}

// Create parameter list sorted from longest to shortest to handle cases where
// one parameter name is a prefix of another
static SortedParameters make_sorted_parameters(const ParameterList &p) {
    SortedParameters sp;

    for (const auto &[key, value] : p) {
        SortedParameter param;
        param.key = "$" + key;
        param.value = value;
        param.used = false;
        sp.push_back(std::move(param));
    }

    std::sort(sp.begin(), sp.end(),
              [](const auto &a, const auto &b) {
                  return a.key.size() > b.key.size();
              });

    return sp;
}

// Helper function to expand the 'value' attribute into x/y/z components
static void expand_value_to_xyz(pugi::xml_node &node) {
    auto value_attr = node.attribute("value");
    if (value_attr) {
        if (node.attribute("x") || node.attribute("y") || node.attribute("z"))
            Throw("Cannot mix \"value\" and \"x\"/\"y\"/\"z\" attributes");

        const char *value = value_attr.value();
        auto tokens = string::tokenize(value);

        if (tokens.size() == 1) {
            // Single value - replicate to all components
            node.append_attribute("x") = tokens[0];
            node.append_attribute("y") = tokens[0];
            node.append_attribute("z") = tokens[0];
        } else if (tokens.size() == 3) {
            // Three values - one for each component
            node.append_attribute("x") = tokens[0];
            node.append_attribute("y") = tokens[1];
            node.append_attribute("z") = tokens[2];
        } else {
            Throw("\"value\" attribute must have exactly 1 or 3 elements");
        }

        node.remove_attribute("value");
    }
}

// Helper function to parse a vector from x/y/z attributes
static ScalarVector3d parse_vector(const ParserState &state, const SceneNode &node,
                                   pugi::xml_node xml_node, double default_val = 0.0) {
    double x = default_val, y = default_val, z = default_val;

    pugi::xml_attribute xa = xml_node.attribute("x"),
                        ya = xml_node.attribute("y"),
                        za = xml_node.attribute("z");

    std::string_view cur;
    try {
        if (xa) { cur = xa.value(); x = string::stof<double>(cur); }
        if (ya) { cur = ya.value(); y = string::stof<double>(cur); }
        if (za) { cur = za.value(); z = string::stof<double>(cur); }
    } catch (...) {
        fail(state, node, "could not parse floating point value \"%s\"", cur);
    }

    return ScalarVector3d(x, y, z);
}

// Helper function to parse a vector from a space-separated string
static ScalarVector3d parse_vector_from_string(const ParserState &state,
                                               const SceneNode &node,
                                               std::string_view str) {
    auto tokens = string::tokenize(str);
    if (tokens.size() != 3)
        fail(state, node, "expected 3 values in vector, got %zu", tokens.size());

    std::string_view cur;
    double x, y, z;
    try {
        cur = tokens[0]; x = string::stof<double>(cur);
        cur = tokens[1]; y = string::stof<double>(cur);
        cur = tokens[2]; z = string::stof<double>(cur);
    } catch (...) {
        fail(state, node, "could not parse floating point value \"%s\"", cur);
    }

    return ScalarVector3d(x, y, z);
}

/**
 * \brief Generate a human-readable file location string for error reporting
 *
 * Returns a string describing where a scene node was defined in the source file.
 * For XML files, this includes the filename and line:column information.
 * For dictionary-parsed nodes (which don't have location info), returns the node's ID
 * or a generic description.
 *
 * \param state Parser state containing file information
 * \param node Scene node to locate
 * \return Human-readable location string (e.g., "scene.xml (line 10, col 5)")
 */
std::string file_location(const ParserState &state, const SceneNode &node) {
    // First check if we have a path from dictionary parsing
    if (!state.nodes.empty()) {
        size_t node_idx = &node - &state.nodes[0];  // Get index of this node
        if (node_idx < state.node_paths.size() && !state.node_paths[node_idx].empty())
            return tfm::format("dictionary node \"%s\"", state.node_paths[node_idx]);
    }

    // Fall back to XML file location logic
    if (node.file_index >= state.files.size()) {
        // Otherwise return a generic description
        if (!node.props.id().empty())
            return tfm::format("node \"%s\"", node.props.id());
        else
            return "unknown location";
    }

    const fs::path& filepath = state.files[node.file_index];

    // If we have the content in memory (from parse_string), use it
    if (!state.content.empty()) {
        std::string line_col = line_col_from_string(state.content, node.offset);
        return tfm::format("string (%s)", line_col);
    }

    // Use the shared helper function
    std::string line_col = line_col_from_file(filepath, node.offset);
    return tfm::format("%s (%s)", filepath.string(), line_col);
}

// Helper function to log a message with file location information
template <typename... Args>
static void log_with_node(LogLevel level, const ParserState &state, const SceneNode &node,
                          const char *msg, Args&&... args) {
    Log(level, "While loading %s: %s", file_location(state, node), tfm::format(msg, std::forward<Args>(args)...));
}

// Helper function to throw an error with file location information
template <typename... Args>
[[noreturn]] static void fail(const ParserState &state, const SceneNode &node,
                              const char *msg, Args&&... args) {
    Throw("Error while loading %s: %s", file_location(state, node), tfm::format(msg, std::forward<Args>(args)...));
}

// Helper function to parse transform nodes
static void parse_transform_node(const ParserState &state, pugi::xml_node node,
                                 const SceneNode &scene_node,
                                 ScalarAffineTransform4d &transform,
                                 SortedParameters &params) {
    // Apply parameter substitution to all attributes
    for (pugi::xml_attribute attr : node.attributes()) {
        std::string_view value = attr.value();
        if (value.find('$') != std::string::npos)
            substitute_parameters(node, attr, params, state, scene_node.file_index);
    }

    std::string_view node_name(node.name());
    auto [tag_type, object_type] = interpret_tag(node_name);

    switch (tag_type) {
        case TagType::Translate: {
            // First expand value attribute if present
            expand_value_to_xyz(node);
            check_attributes(state, scene_node, node, {"x"sv, "y"sv, "z"sv});

            ScalarVector3d vec = parse_vector(state, scene_node, node);
            transform = ScalarAffineTransform4d::translate(vec) * transform;
            break;
        }

        case TagType::Rotate: {
            // First expand value attribute if present
            expand_value_to_xyz(node);
            check_attributes(state, scene_node, node, {"!angle"sv, "x"sv, "y"sv, "z"sv});

            // Default to 0 for missing components
            ScalarVector3d axis = parse_vector(state, scene_node, node, 0.0);
            std::string_view angle_str = node.attribute("angle").value();

            // Fail when the axis is zero
            if (dr::all(axis == 0))
                fail(state, scene_node, "rotation axis cannot be zero");

            try {
                double angle = string::stof<double>(angle_str);
                transform = ScalarAffineTransform4d::rotate(axis, angle) * transform;
            } catch (...) {
                fail(state, scene_node, "could not parse angle value \"%s\"", angle_str);
            }
            break;
        }

        case TagType::Scale: {
            // First expand value attribute if present
            expand_value_to_xyz(node);
            check_attributes(state, scene_node, node, {"x"sv, "y"sv, "z"sv});

            ScalarVector3d vec = parse_vector(state, scene_node, node, 1.0);
            transform = ScalarAffineTransform4d::scale(vec) * transform;
            break;
        }

        case TagType::LookAt: {
            check_attributes(state, scene_node, node, {"!origin"sv, "!target"sv, "up"sv});

            std::string_view origin_str = node.attribute("origin").value();
            std::string_view target_str = node.attribute("target").value();
            std::string_view up_str = node.attribute("up").value();

            ScalarVector3d origin = parse_vector_from_string(state, scene_node, origin_str),
                     target = parse_vector_from_string(state, scene_node, target_str),
                     up     = ScalarVector3d(0);

            if (!up_str.empty())
                up = parse_vector_from_string(state, scene_node, up_str);

            // If up vector is zero, generate one
            if (dr::all(up == 0)) {
                ScalarVector3d dir = dr::normalize(target - origin);
                std::tie(up, std::ignore) = coordinate_system(dir);
            }

            transform = ScalarAffineTransform4d::look_at(origin, target, up) * transform;
            if (dr::any_nested(dr::isnan(transform.matrix)))
                fail(state, scene_node, "invalid lookat transformation");

            break;
        }

        case TagType::Matrix: {
            check_attributes(state, scene_node, node, {"!value"sv});

            std::string_view value_str = node.attribute("value").value();
            auto tokens = string::tokenize(value_str);

            if (tokens.size() != 16 && tokens.size() != 9)
                fail(state, scene_node, "matrix must have 9 or 16 values, got %zu", tokens.size());

            ScalarMatrix4d m = dr::identity<ScalarMatrix4d>();
            int n = tokens.size() == 16 ? 4 : 3;
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    try {
                        m(i, j) = string::stof<double>(tokens[i * n + j]);
                    } catch (...) {
                        fail(state, scene_node, "could not parse matrix value \"%s\"",
                             tokens[i * n + j]);
                    }
                }
            }

            transform = ScalarAffineTransform4d(m) * transform;
            break;
        }

        default:
            fail(state, scene_node, "unexpected <%s> element inside <transform>", node_name);
    }
}

// Helper function to parse XML nodes recursively
static void parse_xml_node(const ParserConfig &config, ParserState &state,
                          pugi::xml_node node, size_t parent_idx,
                          SortedParameters &params, size_t &arg_counter,
                          uint32_t file_index) {
    // Skip comments and other non-element nodes
    if (node.type() != pugi::node_element)
        return;

    // Check if this will be the root node
    bool is_root = parent_idx == 0 && state.empty();

    // Apply parameter substitution and check for undefined parameters
    for (pugi::xml_attribute attr : node.attributes()) {
        std::string_view value = attr.value();
        if (value.find('$') != std::string::npos)
            substitute_parameters(node, attr, params, state, file_index);
    }

    std::string_view element_name = node.name();
    auto [tag_type, object_type] = interpret_tag(element_name);

    // Special case: <spectrum> with 'type' attribute should be treated as an object
    if (tag_type == TagType::Spectrum && node.attribute("type")) {
        tag_type = TagType::Object;
        object_type = ObjectType::Texture;  // Spectrum plugins are textures
    }

    // Create a new scene node
    SceneNode scene_node;
    scene_node.type = object_type;
    scene_node.file_index = file_index;
    scene_node.offset = node.offset_debug();

    if (tag_type == TagType::Invalid)
        fail(state, scene_node, "encountered an unsupported XML element: <%s>", element_name);

    std::string_view name = node.attribute("name").value();

    // Handle different tag types
    switch (tag_type) {
        case TagType::Object: {
            // Check attributes: version is required for root elements, type for others
            check_attributes(state, scene_node, node,
                is_root ? std::initializer_list<std::string_view>{"!version"sv, "type"sv, "id"sv, "name"sv}
                        : std::initializer_list<std::string_view>{"!type"sv, "id"sv, "name"sv});

            // Set plugin name from type attribute, or "scene" for scene elements
            pugi::xml_attribute type_attr = node.attribute("type");
            scene_node.props.set_plugin_name(type_attr ? type_attr.value() : "scene");

            // Set ID if present
            if (pugi::xml_attribute id = node.attribute("id"))
                scene_node.props.set_id(id.value());

            break;
        }

        case TagType::Float: {
            check_attributes(state, scene_node, node, {"!name"sv, "!value"sv});

            std::string_view value_str = node.attribute("value").value();

            try {
                state[parent_idx].props.set(name, string::stof<double>(value_str));
            } catch (...) {
                fail(state, scene_node, "could not parse floating point value \"%s\"",
                    value_str);
            }
            break;
        }

        case TagType::Integer: {
            check_attributes(state, scene_node, node, {"!name"sv, "!value"sv});

            std::string_view value_str = node.attribute("value").value();

            // Skip leading whitespace
            const char *start = value_str.data();
            const char *end = value_str.data() + value_str.size();
            while (start < end && std::isspace(*start))
                ++start;

            int64_t value = 0;
            auto [ptr, err] = std::from_chars(start, end, value);

            if (err != std::errc{})
                fail(state, scene_node, "could not parse integer value \"%s\"",
                    value_str);

            // Check for trailing non-whitespace characters
            for (const char *c = ptr; c < end; ++c) {
                if (!std::isspace(*c))
                    fail(state, scene_node, "could not parse integer value \"%s\"", value_str);
            }

            state[parent_idx].props.set(name, value);
            break;
        }

        case TagType::Boolean: {
            check_attributes(state, scene_node, node, {"!name"sv, "!value"sv});

            std::string_view value_str = node.attribute("value").value();

            // Parse boolean value
            bool value;
            if (string::iequals(value_str, "true"))
                value = true;
            else if (string::iequals(value_str, "false"))
                value = false;
            else
                fail(state, scene_node,
                     "could not parse boolean value \"%s\" -- must be \"true\" "
                     "or \"false\"", value_str);

            state[parent_idx].props.set(name, value);
            break;
        }

        case TagType::String: {
            check_attributes(state, scene_node, node, {"!name"sv, "!value"sv});
            state[parent_idx].props.set(name, node.attribute("value").value());
            break;
        }

        case TagType::Vector:
        case TagType::Point: {
            // First expand value attribute if present (removes 'value' and adds x/y/z)
            expand_value_to_xyz(node);

            // Now check for required attributes (always x/y/z after expansion)
            check_attributes(state, scene_node, node, {"!name"sv, "!x"sv, "!y"sv, "!z"sv});

            state[parent_idx].props.set(name, parse_vector(state, scene_node, node));
            break;
        }

        case TagType::RGB: {
            check_attributes(state, scene_node, node, {"!name"sv, "!value"sv});

            std::string_view value = node.attribute("value").value();

            // Parse RGB values (1 or 3 components)
            auto tokens = string::tokenize(value);

            ScalarColor3d color;
            try {
                if (tokens.size() == 1) {
                    double v = string::stof<double>(tokens[0]);
                    color = ScalarColor3d(v, v, v);
                } else if (tokens.size() == 3) {
                    color = ScalarColor3d(string::stof<double>(tokens[0]),
                                    string::stof<double>(tokens[1]),
                                    string::stof<double>(tokens[2]));
                } else {
                    fail(state, scene_node,
                         "<rgb> tag requires one or three values (got %zu)",
                         tokens.size());
                }
            } catch (...) {
                fail(state, scene_node, "could not parse RGB value \"%s\"", value);
            }

            state[parent_idx].props.set(name, color);
            break;
        }

        case TagType::Spectrum: {
            check_attributes(state, scene_node, node, {"!name"sv, "value"sv, "filename"sv});

            std::string_view value_attr = node.attribute("value").value();
            std::string_view filename_attr = node.attribute("filename").value();

            if (value_attr.empty() == filename_attr.empty())
                fail(state, scene_node,
                     "<spectrum> element requires either \"value\" or \"filename\" attribute "
                     "(but not both)");

            Properties::Spectrum spectrum;
            if (!value_attr.empty()) {
                try {
                    spectrum = Properties::Spectrum(value_attr);
                } catch (const std::exception &e) {
                    fail(state, scene_node, "%s", e.what());
                }
            } else {
                ref<FileResolver> fs = mitsuba::file_resolver();
                fs::path file_path = fs->resolve(filename_attr);
                try {
                    spectrum = Properties::Spectrum(file_path);
                } catch (const std::exception &e) {
                    fail(state, scene_node, "failed to load spectrum from file \"%s\": %s",
                         filename_attr, e.what());
                }
            }

            state[parent_idx].props.set(name, spectrum);
            break;
        }

        case TagType::Include: {
            check_attributes(state, scene_node, node, {"!filename"sv, "name"sv});

            // Clone the FileResolver and add the current file's parent directory
            // This allows relative paths in includes to be resolved correctly
            ref<FileResolver> fs_backup = mitsuba::file_resolver();
            ref<FileResolver> fs = new FileResolver(*fs_backup);

            // Add the parent directory of the current file being parsed
            if (scene_node.file_index < state.files.size()) {
                fs::path parent_dir = state.files[scene_node.file_index].parent_path();
                if (!parent_dir.empty())
                    fs->prepend(parent_dir);
            }
            set_file_resolver(fs.get());

            // Parse the included file recursively with increased depth
            ParserState inc_state;
            fs::path filename;
            try {
                filename = fs->resolve(node.attribute("filename").value());
                if (!fs::exists(filename))
                    fail(state, scene_node, "included file \"%s\" not found", filename);

                Log(Info, "Loading included XML file \"%s\" ..", filename.string());

                inc_state = parse_file_impl(config, filename, params, state.depth + 1);
            } catch (const std::exception &e) {
                set_file_resolver(fs_backup.get());
                // Add context about which file included this one
                fail(state, scene_node, "while processing <include>:\n%s", e.what());
            }

            set_file_resolver(fs_backup.get());

            // Merge the included nodes into our state
            if (!inc_state.empty()) {
                const SceneNode& inc_root = inc_state.root();

                // Omit the root node when the included file is a scene
                size_t skip = inc_root.props.plugin_name() == "scene" ? 1 : 0;

                if (inc_state.size() <= skip)
                    break; // Empty scene

                size_t node_offset = state.size();
                uint32_t file_offset = (uint32_t) state.files.size();

                // Merge files and versions
                state.files.insert(state.files.end(),
                    std::make_move_iterator(inc_state.files.begin()),
                    std::make_move_iterator(inc_state.files.end()));
                state.versions.insert(state.versions.end(),
                    std::make_move_iterator(inc_state.versions.begin()),
                    std::make_move_iterator(inc_state.versions.end()));
                state.nodes.insert(state.nodes.end(),
                    std::make_move_iterator(inc_state.nodes.begin() + skip),
                    std::make_move_iterator(inc_state.nodes.end()));

                // Update indices in moved nodes
                for (size_t i = node_offset; i < state.size(); ++i) {
                    state[i].file_index += file_offset;

                    // Update any ResolvedReference properties to adjust their indices
                    for (const auto &key : state[i].props) {
                        if (key.type() == Properties::Type::ResolvedReference) {
                            auto ref = key.get<Properties::ResolvedReference>();
                            state[i].props.set(key.name(),
                                Properties::ResolvedReference(ref.index() + node_offset - skip), false);
                        }
                    }
                }

                // Link to parent
                if (skip) {
                    // Copy all properties from the root node to the parent, but reassign argument names
                    for (const auto &key : inc_root.props) {
                        if (key.type() == Properties::Type::ResolvedReference) {
                            auto ref = key.get<Properties::ResolvedReference>();

                            // Potentially regenerate argument name using parent's argument counter
                            std::string new_prop_name;
                            if (string::starts_with(key.name(), "_arg_"))
                                new_prop_name = tfm::format("_arg_%zu", arg_counter++);
                            else
                                new_prop_name = std::string(key.name());

                            state[parent_idx].props.set(new_prop_name,
                                Properties::ResolvedReference(ref.index() + node_offset - skip), false);
                        }
                    }
                } else {
                    // For non-scene includes, we need to get the property name
                    // from the original node's name attribute
                    std::string prop_name = std::string(name);
                    if (prop_name.empty())
                        prop_name = tfm::format("_arg_%zu", arg_counter++);

                    state[parent_idx].props.set(
                        prop_name, Properties::ResolvedReference(node_offset),
                        false);
                }

                // Update ID mappings
                for (const auto &[id, idx] : inc_state.id_to_index) {
                    if (skip == 0 || idx > 0) {
                        size_t new_idx = idx + node_offset - skip;
                        if (state.id_to_index.find(id) != state.id_to_index.end())
                            fail(state, state[new_idx],
                                 "duplicate ID in included file \"%s\": \"%s\"",
                                 filename.string(), id);
                        state.id_to_index[id] = new_idx;
                    }
                }
            }
            break;
        }

        case TagType::Transform: {
            check_attributes(state, scene_node, node, {"!name"sv});

            // Create a fresh transform for this scope
            ScalarAffineTransform4d transform;

            // Process child transform operations
            for (pugi::xml_node child : node.children()) {
                if (child.type() == pugi::node_element)
                    parse_transform_node(state, child, scene_node, transform, params);
            }

            // Store the accumulated transform
            state[parent_idx].props.set(name, transform);

            return; // Don't process children again
        }

        case TagType::NamedReference: {
            check_attributes(state, scene_node, node, {"!id"sv, "name"sv});

            std::string prop_name;
            if (!name.empty())
                prop_name = std::string(name);
            else
                prop_name = tfm::format("_arg_%zu", arg_counter++);

            state[parent_idx].props.set(
                prop_name, Properties::Reference(node.attribute("id").value()));

            break;
        }

        case TagType::Alias: {
            check_attributes(state, scene_node, node, {"!id"sv, "!as"sv});

            std::string_view alias_src = node.attribute("id").value();
            std::string_view alias_dst = node.attribute("as").value();

            // Check if the destination ID already exists
            if (state.id_to_index.find(alias_dst) != state.id_to_index.end())
                fail(state, scene_node, "alias destination ID \"%s\" already exists", alias_dst);

            // Check if the source ID exists
            auto it_src = state.id_to_index.find(alias_src);
            if (it_src == state.id_to_index.end())
                fail(state, scene_node, "alias source ID \"%s\" not found", alias_src);

            // Add the alias mapping to id_to_index
            state.id_to_index[std::string(alias_dst)] = it_src->second;

            break;
        }

        case TagType::Default: {
            check_attributes(state, scene_node, node, {"!name"sv, "!value"sv});

            std::string_view param_name = node.attribute("name").value();
            std::string_view param_value = node.attribute("value").value();

            if (param_name.empty())
                fail(state, scene_node, "<default>: name must be non-empty");

            // Check if this parameter already exists in params
            bool found = false;
            for (const auto &param : params) {
                if (param.key.substr(1) == param_name) {  // Remove '$' prefix
                    found = true;
                    break;
                }
            }

            // If not found, add it as a new parameter
            if (!found) {
                SortedParameter new_param;
                new_param.key = "$" + std::string(param_name);
                new_param.value = param_value;
                new_param.used = true; // Don't report unused default parameters as errors/warnings

                // Insert in sorted order (longest key first)
                auto insert_pos = std::find_if(params.begin(), params.end(),
                    [&new_param](const SortedParameter &p) {
                        return new_param.key.size() > p.key.size();
                    });
                params.insert(insert_pos, std::move(new_param));
            }

            break;
        }

        case TagType::Resource: {
            check_attributes(state, scene_node, node, {"!value"sv});

            // <path> tags must be direct children of the root node
            if (parent_idx != 0)
                fail(state, scene_node, "<path>: path can only be child of root");

            ref<FileResolver> fs = mitsuba::file_resolver();
            fs::path resource_path(node.attribute("value").value());

            if (!resource_path.is_absolute()) {
                // First try to resolve it starting in the XML file directory
                if (scene_node.file_index < state.files.size()) {
                    resource_path = state.files[scene_node.file_index].parent_path() / resource_path;
                }
                // Otherwise try to resolve it with the FileResolver
                if (!fs::exists(resource_path))
                    resource_path = fs->resolve(node.attribute("value").value());
            }

            if (!fs::exists(resource_path))
                fail(state, scene_node, "<path>: folder \"%s\" not found", resource_path);

            fs->prepend(resource_path);
            break;
        }

        case TagType::Translate:
        case TagType::Rotate:
        case TagType::Scale:
        case TagType::LookAt:
        case TagType::Matrix:
            // Transform operations are only allowed inside <transform> tags
            fail(state, scene_node, "transform operation <%s> can only appear inside a <transform> tag",
                 element_name);

        default:
            fail(state, scene_node, "unknown or unsupported tag type <%s>", node.name());
            break;
    }

    // Only add object nodes to the state (property nodes like float/int/string are handled above)
    if (tag_type == TagType::Object) {
        // Get property name from name attribute or auto-generate
        std::string property_name = std::string(name);
        if (property_name.empty())
            property_name = tfm::format("_arg_%zu", arg_counter++);

        // Add the node to the state
        size_t node_idx = state.size();

        // Register the node's ID in the id_to_index map if it has one
        if (!scene_node.props.id().empty()) {
            auto [it, inserted] = state.id_to_index.insert({std::string(scene_node.props.id()), node_idx});
            if (!inserted) {
                const SceneNode &prev_node = state.nodes[it->second];
                fail(state, scene_node, "duplicate ID: \"%s\" (previous was at %s)",
                     scene_node.props.id(), file_location(state, prev_node));
            }
        }

        // If we have a valid parent index and this is not the root, add it as a child
        if (!is_root)
            state[parent_idx].props.set(
                property_name, Properties::ResolvedReference(node_idx), false);

        // Recursively parse child nodes
        state.nodes.push_back(std::move(scene_node));
        size_t child_arg_counter = 0;
        for (pugi::xml_node child : node.children())
            parse_xml_node(config, state, child, node_idx, params, child_arg_counter, file_index);
    } else if (tag_type != TagType::Transform) {
        // For properties (other than transforms), check for unexpected children
        for (pugi::xml_node child : node.children()) {
            if (child.type() == pugi::node_element)
                fail(state, scene_node, "<%s> element cannot occur as child of a property", child.name());
        }
    }
}

ParserState parse_file(const ParserConfig &config, const fs::path &filename,
                       const ParameterList &param_list) {
    SortedParameters params = make_sorted_parameters(param_list);
    ParserState state = parse_file_impl(config, filename, params, 0);
    check_unused_parameters(config, params);
    return state;
}

static ParserState parse_file_impl(const ParserConfig &config,
                                   const fs::path &filename,
                                   SortedParameters &params, int depth) {

    // Check recursion depth
    if (depth >= config.max_include_depth)
        Throw("Exceeded maximum include recursion depth of %d", config.max_include_depth);

    // Check if file exists
    if (!fs::exists(filename))
        Throw("File not found: %s", filename.string());

    Log(Info, "Loading XML file \"%s\" ..", filename);

    // Parse XML using pugixml
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filename.native().c_str());

    if (!result) {
        std::string location = line_col_from_file(filename, result.offset);
        Throw("Parsing of XML file \"%s\" failed: %s (%s)",
              filename.string(), result.description(), location);
    }

    // Get the root element
    pugi::xml_node root_node = doc.document_element();

    // Check for version attribute
    if (!root_node.attribute("version"))
        Throw("Error while loading XML file \"%s\": missing version attribute in root element <%s>",
              filename.string(), root_node.name());

    ParserState state;
    state.depth = depth;
    state.files.push_back(filename);

    std::string_view version_str = root_node.attribute("version").value();
    try {
        state.versions.push_back(util::Version(version_str));
    } catch (...) {
        Throw("Error while loading XML file \"%s\": invalid version number \"%s\"",
              filename.string(), version_str);
    }

    if (root_node) {
        size_t arg_counter = 0;
        parse_xml_node(config, state, root_node, 0, params, arg_counter, 0);
    }

    return state;
}

ParserState parse_string(const ParserConfig &config, std::string_view string, const ParameterList &param_list) {
    SortedParameters params = make_sorted_parameters(param_list);

    // Parse XML using pugixml
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer(string.data(), string.size());

    if (!result) {
        std::string location = line_col_from_string(string, result.offset);
        Throw("XML parsing failed: %s (%s)", result.description(), location);
    }

    // Get the root element
    pugi::xml_node root_node = doc.document_element();

    // Check for version attribute
    if (!root_node.attribute("version"))
        Throw("Missing version attribute in root element <%s>", root_node.name());

    ParserState state;
    state.depth = 0;
    state.content = string;
    state.files.push_back("<string>");

    std::string_view version_str = root_node.attribute("version").value();
    try {
        state.versions.push_back(util::Version(version_str));
    } catch (...) {
        Throw("Invalid version number: %s", version_str);
    }

    if (root_node) {
        size_t arg_counter = 0;
        parse_xml_node(config, state, root_node, 0, params, arg_counter, 0);
    }

    // Check for unused parameters
    check_unused_parameters(config, params);

    return state;
}

// ===========================================================================
//   Scene transformation passes
// ===========================================================================

// Helper function to convert camelCase to underscore_case
static std::string camel_to_underscore_case(std::string_view input) {
    std::string result;
    result.reserve(input.size() + 4); // Reserve extra space for underscores

    for (size_t i = 0; i < input.length(); ++i) {
        char c = input[i];

        if (i > 0 && std::islower(input[i-1]) && std::isupper(c)) {
            result += '_';
            result += (char) std::tolower(c);

            // Convert any subsequent uppercase letters to lowercase
            while (i + 1 < input.length() && std::isupper(input[i + 1])) {
                i++;
                result += (char) std::tolower(input[i]);
            }
        } else {
            result += c;
        }
    }

    return result;
}

// Helper function to upgrade nodes from version < 2.0
static void upgrade_from_v1(Properties &props) {
    // Convert camelCase to underscore_case for property names
    for (const auto &prop : props) {
        std::string_view name = prop.name();
        std::string new_name = camel_to_underscore_case(name);

        if (new_name != name)
            props.rename_property(name, new_name);
    }

    // Upgrade 'diffuse_reflectance' to 'reflectance' for diffuse BSDFs
    if (props.plugin_name() == "diffuse" && props.has_property("diffuse_reflectance"))
        props.rename_property("diffuse_reflectance", "reflectance");

    // Convert uoffset/voffset/uscale/vscale to transform block
    bool has_uoffset = props.has_property("uoffset");
    bool has_voffset = props.has_property("voffset");
    bool has_uscale = props.has_property("uscale");
    bool has_vscale = props.has_property("vscale");

    if (has_uoffset || has_voffset || has_uscale || has_vscale) {
        ScalarVector3d offset(0.0, 0.0, 0.0);
        ScalarVector3d scale(1.0, 1.0, 1.0);

        // Extract values and remove properties
        if (has_uoffset) {
            offset.x() = props.get<double>("uoffset");
            props.remove_property("uoffset");
        }
        if (has_voffset) {
            offset.y() = props.get<double>("voffset");
            props.remove_property("voffset");
        }
        if (has_uscale) {
            scale.x() = props.get<double>("uscale");
            props.remove_property("uscale");
        }
        if (has_vscale) {
            scale.y() = props.get<double>("vscale");
            props.remove_property("vscale");
        }

        props.set("to_uv", ScalarAffineTransform4d::scale(scale) *
                           ScalarAffineTransform4d::translate(offset));
    }
}

void transform_upgrade(const ParserConfig &/*config*/, ParserState &state) {
    if (state.versions.empty())
        return;

    // Process each node and apply upgrades based on its file version
    for (auto &node : state.nodes) {
        const util::Version &version = state.versions[node.file_index];

        // Process upgrades for version < 2.0
        if (version < util::Version(2, 0, 0))
            upgrade_from_v1(node.props);
    }
}

// Helper function to detect circular references using DFS
static void check_cycles(const ParserState &state, size_t node_idx,
                         std::vector<size_t> &path) {
    // Check if current node is already in the path (cycle detected)
    auto it = std::find(path.begin(), path.end(), node_idx);
    if (it != path.end()) {
        // Build cycle string starting from the repeated node
        std::string cycle;
        for (auto i = it; i != path.end(); ++i) {
            if (i != it) cycle += " -> ";
            std::string_view id = state[*i].props.id();
            cycle += id.empty() ? tfm::format("[node %d]", *i) : std::string(id);
        }
        std::string_view last_id = state[node_idx].props.id();
        cycle += " -> ";
        cycle += last_id.empty() ? tfm::format("[node %d]", node_idx) : std::string(last_id);

        fail(state, state[node_idx], "circular reference detected: %s", cycle);
    }

    // Add current node to path and recurse
    path.push_back(node_idx);
    for (const auto &prop : state[node_idx].props.filter(Properties::Type::ResolvedReference))
        check_cycles(state, prop.get<Properties::ResolvedReference>().index(), path);
    path.pop_back();
}

void transform_resolve(const ParserConfig &/*config*/, ParserState &state) {
    // First pass: resolve all references
    for (auto &node : state.nodes) {
        for (const auto &key : node.props.filter(Properties::Type::Reference)) {
            Properties::Reference ref = key.get<Properties::Reference>();
            std::string_view ref_id = ref.id(), prop_name = key.name();

            auto it = state.id_to_index.find(ref_id);
            if (it == state.id_to_index.end()) {
                fail(state, node,
                     "unresolved reference to \"%s\" in property \"%s\". Make "
                     "sure the object with id=\"%s\" exists.",
                     ref_id, prop_name, ref_id);
            }

            // Replace with resolved reference
            node.props.set(prop_name, Properties::ResolvedReference(it->second), false);
        }
    }

    // Second pass: detect circular references
    std::vector<size_t> path;
    for (size_t i = 0; i < state.size(); ++i)
        check_cycles(state, i, path);
}

void transform_merge_equivalent(const ParserConfig &/*config*/, ParserState &state) {
    // Build a hash table mapping Properties to node indices
    struct NodeHasher {
        const ParserState *state;
        NodeHasher(const ParserState *s) : state(s) {}

        size_t operator()(size_t node_idx) const {
            return state->nodes[node_idx].props.hash();
        }
    };

    struct NodeEqual {
        const ParserState *state;
        NodeEqual(const ParserState *s) : state(s) {}

        bool operator()(size_t a, size_t b) const {
            return state->nodes[a].props == state->nodes[b].props;
        }
    };

    // Helper data structure for clustering, rebuilt in each phase
    tsl::robin_map<size_t, size_t, NodeHasher, NodeEqual>
        dedup_map(16, NodeHasher(&state), NodeEqual(&state));

    // Map from node index to the index of its canonical representative
    std::vector<size_t> canonical(state.size());
    for (size_t i = 0; i < state.size(); ++i)
        canonical[i] = i;

    size_t total_merged_count = 0;

    // Iterate until no more changes occur
    bool changed = true;
    while (changed) {
        changed = false;
        dedup_map.clear();

        // First pass: merge equivalent nodes
        for (size_t i = 0; i < state.size(); ++i) {
            size_t repr = canonical[i];

            // Skip merging for emitters and shapes
            if (state[repr].type == ObjectType::Emitter ||
                state[repr].type == ObjectType::Shape)
                continue;

            // Try to find an equivalent node
            auto [it, inserted] = dedup_map.insert({repr, repr});
            if (!inserted && repr != it->second) {
                canonical[i] = canonical[repr] = it->second;
                total_merged_count++;
                changed = true;

                Log(Debug, "Merging duplicate %s node (index %zu -> %zu)",
                    plugin_type_name(state[i].type), i, it->second);
            }
        }

        // Second pass: update all ResolvedReference properties to point to canonical nodes
        for (auto &node : state.nodes) {
            for (const auto &prop : node.props.filter(Properties::Type::ResolvedReference)) {
                size_t ref_idx = prop.get<Properties::ResolvedReference>().index();
                size_t canonical_idx = canonical[ref_idx];

                if (ref_idx != canonical_idx) {
                    node.props.set(prop.name(),
                                 Properties::ResolvedReference(canonical_idx),
                                 false);
                    changed = true;
                }
            }
        }
    }

    if (total_merged_count > 0)
        Log(Info, "Merged %zu duplicate nodes", total_merged_count);
}

// Section names corresponding to order indices (see node_order_id)
static const char* section_names[] = {
    "Camera and Rendering Parameters", // 0: Integrator
    "Camera and Rendering Parameters", // 1: Sensor
    "Materials",                       // 2: Texture
    "Materials",                       // 3: BSDF
    "Emitters",                        // 4: Emitter/Shape with area light
    "Shapes",                          // 5: Shape
    "Volumes",                         // 6: Volume
    "Volumes",                         // 7: Medium
    "Other"                            // 8: Other/Unknown
};

// Helper function that assigns a logical section number to a node. Used in
// transform_reporder() and write_node_to_xml()
static int node_order_id(const ParserState &state, size_t node_idx) {
    const SceneNode &node = state[node_idx];

    // Check if a shape contains an area light
    auto has_area_light = [&state](size_t idx) -> bool {
        const SceneNode &node = state[idx];
        if (node.type != ObjectType::Shape)
            return false;
        for (const auto &prop : node.props.filter(Properties::Type::ResolvedReference)) {
            const SceneNode &child = state[prop.get<Properties::ResolvedReference>().index()];
            if (child.type == ObjectType::Emitter && child.props.plugin_name() == "area")
                return true;
        }
        return false;
    };

    // Determine order based on node type
    switch (node.type) {
        case ObjectType::Integrator: return 0;
        case ObjectType::Sensor:     return 1;
        case ObjectType::Texture:    return 2;
        case ObjectType::BSDF:       return 3;
        case ObjectType::Emitter:    return 4;
        case ObjectType::Shape:      return has_area_light(node_idx) ? 4 : 5;
        case ObjectType::Volume:     return 6;
        case ObjectType::Medium:     return 7;
        default:                     return 8;
    }
}

void transform_reorder(const ParserConfig &/*config*/, ParserState &state) {
    if (state.empty() || state.root().type != ObjectType::Scene)
        return;

    Properties &props = state.root().props;

    // Build a vector of (priority, insertion_order, property_name) tuples
    std::vector<std::tuple<size_t, size_t, std::string>> refs;
    size_t insertion_order = 0;

    for (const auto &prop : props.filter(Properties::Type::ResolvedReference)) {
        size_t idx = prop.get<Properties::ResolvedReference>().index();
        int order = node_order_id(state, idx);
        refs.push_back({order, insertion_order++, std::string(prop.name())});
    }

    // Sort by (priority, insertion_order, property_name) - tuple comparison does this naturally
    std::sort(refs.begin(), refs.end());

    // Remove all reference properties and add them back in sorted order
    for (const auto &[priority, ins_order, name] : refs) {
        auto ref = props.get<Properties::ResolvedReference>(name);
        props.remove_property(name);
        props.set(name, ref, false);
    }
}

void transform_merge_meshes(const ParserConfig &/*config*/, ParserState &state) {
    if (state.empty() || state.root().type != ObjectType::Scene)
        return;

    Properties &root_props = state.root().props;
    std::vector<std::pair<std::string, size_t>> children;
    for (const auto &prop : root_props.filter(Properties::Type::ResolvedReference))
        children.emplace_back(std::string(prop.name()),
                                prop.get<Properties::ResolvedReference>().index());

    // If there are no references to move, we're done
    if (children.empty())
        return;

    // Create a new merge shape node
    SceneNode merge_node;
    merge_node.type = ObjectType::Shape;
    merge_node.props.set_plugin_name("merge");

    for (const auto &[name, ref_idx] : children) {
        merge_node.props.set(name, Properties::ResolvedReference(ref_idx), false);
        root_props.remove_property(name);
    }
    // Use auto-generated argument name to avoid property validation issues
    root_props.set("_arg_0", Properties::ResolvedReference(state.size()), false);

    state.nodes.push_back(std::move(merge_node));
}

void transform_relocate(const ParserConfig &/*config*/, ParserState &state,
                        const fs::path &output_directory) {
    if (state.empty())
        return;

    // Helper function to determine subfolder based on object type
    auto get_subfolder = [](ObjectType type,
                            std::string_view plugin_name) -> std::string_view {
        switch (type) {
            case ObjectType::Texture:
                if (plugin_name == "irregular" || plugin_name == "regular")
                    return "spectra";
                [[fallthrough]];
            case ObjectType::Emitter:
                return "textures";
            case ObjectType::Shape:
                return "meshes";
            default:
                return "assets";
        }
    };

    // Track copied files: source_path -> relative target path
    tsl::robin_map<std::string, std::string, std::hash<std::string_view>,
                   std::equal_to<>> file_mapping;

    // Track duplicate counts by base filename
    tsl::robin_map<std::string, int, std::hash<std::string_view>,
                   std::equal_to<>> copy_count;

    // Helper function to copy a file and handle duplicates
    auto copy_file = [&](const fs::path &src_path,
                         const fs::path &subfolder) -> std::string {
        // Check if already copied
        std::string src_str = src_path.string();
        if (auto it = file_mapping.find(src_str); it != file_mapping.end())
            return it->second;

        // Create target directory
        fs::path target_dir = output_directory / subfolder;
        if (!fs::exists(target_dir))
            fs::create_directory(target_dir);

        // Handle duplicates using copy_count mapping (matches Python behavior)
        std::string base_name = src_path.filename().string();
        if (auto it = copy_count.find(base_name); it != copy_count.end()) {
            // Add number suffix for duplicate
            std::string stem = src_path.filename().replace_extension().string(),
                        ext  = src_path.extension().string();
            base_name = stem + " (" + std::to_string(it.value()) + ")" + ext;
            it.value() += 1;
        } else {
            copy_count[base_name] = 1;
        }

        fs::path target_path = target_dir / base_name;

        if (!fs::copy_file(src_path, target_path))
            Throw("Failed to copy file from \"%s\" to \"%s\"",
                  src_path.string(), target_path.string());

        std::string rel_path = (subfolder / base_name).string();

#if defined(_WIN32)
        for (size_t i = 0; i < rel_path.size(); ++i) {
            char &c = rel_path[i];
            if (c == '\\')
                c = '/';
        }
#endif

        file_mapping[src_str] = rel_path;
        return rel_path;
    };


    // Process all nodes
    for (auto &node : state.nodes) {
        std::string_view subfolder = get_subfolder(node.type, node.props.plugin_name());

        // Process only string properties using filter
        for (const auto &prop : node.props.filter(Properties::Type::String)) {
            std::string_view prop_name = prop.name();
            std::string_view value = prop.get<std::string_view>();
            if (!value.empty() && fs::exists(fs::path(value))) {
                std::string rel_path = copy_file(fs::path(value), subfolder);
                node.props.set(prop_name, rel_path, false);
            }
        }
    }

    if (!file_mapping.empty())
        Log(Info, "Relocated %zu files.", file_mapping.size());
}

void transform_all(const ParserConfig &config, ParserState &state) {
    transform_upgrade(config, state);
    transform_resolve(config, state);
    if (config.merge_equivalent)
        transform_merge_equivalent(config, state);
    if (config.merge_meshes)
        transform_merge_meshes(config, state);
}

// ===========================================================================
//   Scene instantiation
// ===========================================================================


/// Helper structure to track instantiation state for parallel loading
struct Scratch {
    std::mutex mutex;
    std::vector<ref<Object>> objects;
    Task* task = nullptr;
};

// Set JIT scopes while instantating nodes
struct ScopedSetJITScope {
    ScopedSetJITScope(uint32_t backend, uint32_t scope) : backend(backend), backup(0) {
#if defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_CUDA)
        if (backend) {
            backup = jit_scope((JitBackend) backend);
            jit_set_scope((JitBackend) backend, scope);
        }
#endif
    }

    ~ScopedSetJITScope() {
#if defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_CUDA)
        if (backend)
            jit_set_scope((JitBackend) backend, backup);
#endif
    }

    uint32_t backend, backup;
};


static Task* instantiate_node(const ParserConfig &config,
                              ParserState &state,
                              std::vector<Scratch> &scratch,
                              size_t index) {
    Scratch &s = scratch[index];

    // Check if already being processed or completed
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (s.task || !s.objects.empty())
            return s.task;
    }

    const SceneNode &node = state[index];

    // Gather dependency tasks
    std::vector<Task*> deps;
    deps.reserve(node.props.size());

    // Process all ResolvedReference properties to find dependencies
    for (auto &key : node.props.filter(Properties::Type::ResolvedReference)) {
        size_t child_idx = key.get<Properties::ResolvedReference>().index();

        // Recursively instantiate the dependency
        Task* dep_task = instantiate_node(config, state, scratch, child_idx);
        if (dep_task)
            deps.push_back(dep_task);
    }

    // Determine backend and scope for JIT compilation
    uint32_t backend = 0, scope = 0;

    if (config.parallel) {
#if defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_CUDA)
        if (string::starts_with(config.variant, "cuda_"))
            backend = (uint32_t) JitBackend::CUDA;
        else if (string::starts_with(config.variant, "llvm_"))
            backend = (uint32_t) JitBackend::LLVM;

        if (backend) {
            jit_new_scope((JitBackend) backend);
            scope = jit_scope((JitBackend) backend);
        }
#endif
    }

    // Lambda function to instantiate a node once its children are ready
    auto instantiate = [&config, &state, &scratch, index, backend, scope]() {
        ScopedSetJITScope set_scope(config.parallel ? backend : 0u, scope);

        Scratch &s = scratch[index];
        SceneNode &node = state[index];
        Properties &props = node.props;

        // Replace ResolvedReference properties with actual objects
        for (auto &key : props.filter(Properties::Type::ResolvedReference)) {
            size_t child_idx = key.get<Properties::ResolvedReference>().index();

            // Get the instantiated child objects
            const std::vector<ref<Object>> &children = scratch[child_idx].objects;
            if (children.empty())
                Throw("Child object at index %zu has not been instantiated yet", child_idx);

            // Handle object expansion
            if (children.size() == 1) {
                props.set(key.name(), children[0], false);
            } else {
                // Multiple children - append indices
                for (size_t i = 0; i < children.size(); ++i)
                    props.set(tfm::format("%s_%zu", key.name(), i), children[i], false);
                props.remove_property(key.name());
            }
        }

        // Create the object
        ref<Object> obj;
        try {
            // Special handling for rgb/spectrum dictionaries
            if (props.plugin_name() == "rgb" || props.plugin_name() == "spectrum") {
                // These are special texture types that need to be created via get_texture_impl
                obj = props.get_texture_impl("value", config.variant, false, false);
            } else {
                obj = PluginManager::instance()->create_object(
                    props, config.variant, node.type);
            }
        } catch (const std::exception &e) {
            Throw("At %s: failed to instantiate %s plugin of type \"%s\": %s",
                  file_location(state, node),
                  plugin_type_name(node.type), props.plugin_name(), e.what());
        }

        // Expand the object once and store the results
        s.objects = obj->expand();
        if (s.objects.empty())
            s.objects.push_back(obj);

        // Check for unqueried properties by iterating through all properties
        std::string unqueried_details;
        size_t unqueried_count = 0;
        for (const auto &prop : props) {
            if (!prop.queried()) {
                unqueried_details += tfm::format("  - %s \"%s\": %s\n",
                    property_type_name(prop.type()),
                    prop.name(),
                    props.as_string(prop.name()));
                unqueried_count++;
            }
        }

        if (unqueried_count) {
            unqueried_details.pop_back(); // Remove trailing newline

            log_with_node(config.unused_properties, state, node,
                          "Found %zu unreferenced %s in %s plugin of type \"%s\":\n%s",
                          unqueried_count,
                          unqueried_count > 1 ? "properties" : "property",
                          plugin_type_name(node.type), props.plugin_name(),
                          unqueried_details);
        }
    };

    // Instantiate the node
    if (index == 0) {
        // Root node is always instantiated on the main thread
        // First wait for all dependencies
        std::exception_ptr eptr;
        for (Task* task : deps) {
            try {
                if (task)
                    task_wait(task);
            } catch (...) {
                if (!eptr)
                    eptr = std::current_exception();
            }
        }

        // Release all tasks
        for (size_t i = 0; i < scratch.size(); ++i) {
            if (scratch[i].task) {
                task_release(scratch[i].task);
                scratch[i].task = nullptr;
            }
        }

        // Rethrow first exception if any
        if (eptr)
            std::rethrow_exception(eptr);

        // Instantiate the root
        instantiate();

#if defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_CUDA)
        if (backend && config.parallel)
            jit_new_scope((JitBackend) backend);
#endif

        return nullptr;
    } else {
        // Non-root nodes
        if (config.parallel && !deps.empty()) {
            // Schedule asynchronous instantiation with dependencies
            s.task = dr::do_async(instantiate, deps.data(), deps.size());
            return s.task;
        } else {
            // Instantiate synchronously
            instantiate();
            return nullptr;
        }
    }
}

std::vector<ref<Object>> instantiate(const ParserConfig &config, ParserState &state) {
    if (state.empty())
        Throw("No nodes to instantiate");

    std::vector<Scratch> scratch(state.size());
    instantiate_node(config, state, scratch, 0);

    // Return the expanded root objects
    return scratch[0].objects;
}

// ===========================================================================
//   XML writing support
// ===========================================================================

/**
 * \brief Helper function to decompose a transform matrix into a canonical
 * format to improve editability of the resulting XML.

 * The function tries to decompose the transformation into
 * - A scale (uniform or per-axis)
 * - A single rotation
 * - A translation
 * (omitting
 * 6. For sensors without scale: <lookat> (when applicable)
 * Falls back to <matrix> if decomposition fails or results in multiple rotations
 */
static bool decompose_transform(const ScalarAffineTransform4d &transform,
                                pugi::xml_node &prop_node,
                                bool is_sensor = false) {
    const ScalarMatrix4d &m = transform.matrix;
    const double eps = 1e-6; // Fixed threshold for comparisons

    /// Helper function to add an individual attribute
    auto add_attr = [eps](pugi::xml_node &node, std::string_view key, double value, double identity) -> bool {
        if (std::abs(value - identity) < eps)
            return false;
        node.append_attribute(key).set_value(tfm::format("%.17g", value));
        return true;
    };

    /// Helper function to add an vectorial attribute
    auto add_vec = [&add_attr, eps](pugi::xml_node &parent,
                                    std::string_view key,
                                    const ScalarVector3d &value,
                                    bool supports_uniform,
                                    double identity) -> bool {
        pugi::xml_node node = parent.append_child(key);
        bool success = false;

        if (supports_uniform &&
            std::abs(value.x() - value.y()) < eps &&
            std::abs(value.y() - value.z()) < eps) {
            success |= add_attr(node, "value", value.x(), identity);
        } else {
            success |= add_attr(node, "x", value.x(), identity);
            success |= add_attr(node, "y", value.y(), identity);
            success |= add_attr(node, "z", value.z(), identity);
        }

        if (!success)
            parent.remove_child(node);

        return success;
    };

    // Decompose into scale+rotation+translation
    auto [scale_mat, quat, trans] = dr::transform_decompose(m);
    if (dr::any_nested(dr::isnan(scale_mat)) ||
        dr::any_nested(dr::isnan(quat)) ||
        dr::any_nested(dr::isnan(trans)))
        return false;

    // Transformations with a non-diagonal scale cannot be decomposed
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (i != j && std::abs(scale_mat(i, j)) > eps)
                return false;
        }
    }

    // Extract scale values from diagonal
    ScalarVector3d scale(scale_mat(0, 0), scale_mat(1, 1), scale_mat(2, 2));
    bool has_scale = dr::any(dr::abs(scale - 1.0) > eps);

    // Convert quaternion to Euler angles
    ScalarVector3d euler = dr::quat_to_euler(quat) * (180.0 / dr::Pi<double>);

    // Reject operations that require too many rotations
    int rotation_count = (int) dr::count(dr::abs(euler) > eps);
    if (rotation_count > 1) {
        // For sensors without scale, try to use a lookat representation
        if (is_sensor && !has_scale) {
            ScalarVector3d origin = transform.translation();
            ScalarVector3d forward(m(0, 2), m(1, 2), m(2, 2));
            ScalarVector3d up(m(0, 1), m(1, 1), m(2, 1));
            ScalarVector3d target = origin + forward;

            pugi::xml_node lookat_node = prop_node.append_child("lookat");
            lookat_node.append_attribute("origin").set_value(
                tfm::format("%.17g, %.17g, %.17g", origin.x(), origin.y(), origin.z()));
            lookat_node.append_attribute("target").set_value(
                tfm::format("%.17g, %.17g, %.17g", target.x(), target.y(), target.z()));
            lookat_node.append_attribute("up").set_value(
                tfm::format("%.17g, %.17g, %.17g", up.x(), up.y(), up.z()));

            return true;
        }
        return false;
    }

    bool has_transform = false;
    has_transform |= add_vec(prop_node, "scale", scale, true, 1.0);

    // Add rotations
    if (std::abs(euler[0]) >= eps) {
        pugi::xml_node rotate_node = prop_node.append_child("rotate");
        rotate_node.append_attribute("x").set_value("1");
        add_attr(rotate_node, "angle", euler[0], 0.0);
        has_transform = true;
    }

    if (std::abs(euler[1]) >= eps) {
        pugi::xml_node rotate_node = prop_node.append_child("rotate");
        rotate_node.append_attribute("y").set_value("1");
        add_attr(rotate_node, "angle", euler[1], 0.0);
        has_transform = true;
    }

    if (std::abs(euler[2]) >= eps) {
        pugi::xml_node rotate_node = prop_node.append_child("rotate");
        rotate_node.append_attribute("z").set_value("1");
        add_attr(rotate_node, "angle", euler[2], 0.0);
        has_transform = true;
    }

    has_transform |= add_vec(prop_node, "translate", trans, false, 0.0);

    return has_transform;
}

// Structure to track state during XML writing
struct WriterState {
    // Track how many times each node is referenced
    std::vector<size_t> refcount;

    // Default parameter tracking
    struct DefaultParam {
        std::string_view property_name;  // e.g. "sample_count"
        std::string_view default_name;   // e.g. "spp"
        std::string value;               // First encountered value
        bool used = false;               // Whether this default was used at least once
    };

    // List of parameters we want to extract as defaults
    std::vector<DefaultParam> defaults = {
        {"sample_count", "spp", "", false},
        {"width", "resx", "", false},
        {"height", "resy", "", false}
    };

    WriterState(size_t node_count)
        : refcount(node_count, 0) { }
};

// Helper function to write a single node to XML
static void write_node_to_xml(WriterState &writer_state,
                              const ParserState &state,
                              size_t node_idx,
                              pugi::xml_node &xml_parent,
                              std::vector<pugi::xml_node> &xml_nodes,
                              std::string_view property_name) {
    if (node_idx >= state.size())
        return;

    const SceneNode &node = state[node_idx];

    // Check if this is a reference to an already-written node
    if (xml_nodes[node_idx]) {
        // This node was already written, so write a reference instead
        pugi::xml_node ref_node = xml_parent.append_child("ref");

        // Check if the original node has an ID attribute
        pugi::xml_attribute id_attr = xml_nodes[node_idx].attribute("id");
        if (!id_attr) {
            // Add an auto-generated ID to the original node
            std::string generated_id = tfm::format("_unnamed_%u", node_idx);
            xml_nodes[node_idx].append_attribute("id").set_value(generated_id);
            id_attr = xml_nodes[node_idx].attribute("id");
        }

        // Use the ID for this reference
        ref_node.append_attribute("id").set_value(id_attr.value());

        // Add name attribute if this node has a property name
        if (!property_name.empty() && !string::starts_with(property_name, "_arg_"))
            ref_node.append_attribute("name").set_value(property_name);

        return;
    }

    // Determine XML tag name based on object type and plugin name
    std::string_view tag_name;
    if (node.props.plugin_name() == "ref")
        tag_name = "ref";
    else if (node.props.plugin_name() == "scene")
        tag_name = "scene";
    else if (node.type != ObjectType::Unknown)
        tag_name = plugin_type_name(node.type);
    else
        Throw("Cannot determine XML tag name for node with unknown object type and plugin name \"%s\"",
              node.props.plugin_name());

    // Create XML element
    pugi::xml_node xml_node = xml_parent.append_child(tag_name);

    // Store the XML node for potential future references
    xml_nodes[node_idx] = xml_node;

    // Add version attribute for root element
    if (node_idx == 0) {
        util::Version version =
            !state.versions.empty()
                ? state.versions[0]
                : util::Version(MI_VERSION_MAJOR, MI_VERSION_MINOR,
                                MI_VERSION_PATCH);
        xml_node.append_attribute("version").set_value(version.to_string());
    }

    // Handle attributes based on tag type
    if (tag_name == "ref") {
        // For ref tags, id is the reference target and name is the property name
        xml_node.append_attribute("id").set_value(node.props.id());
        xml_node.append_attribute("name").set_value(property_name);
        return; // Skip properties and children for ref nodes
    }

    // Add type attribute (except for scene root)
    if (node.props.plugin_name() != "scene")
        xml_node.append_attribute("type").set_value(node.props.plugin_name());

    // Add ID only if this node is referenced more than once
    std::string_view id = node.props.id();
    if (!id.empty() && writer_state.refcount[node_idx] > 1)
        xml_node.append_attribute("id").set_value(id);

    // Add name attribute if not auto-generated
    if (!property_name.empty() && !string::starts_with(property_name, "_arg_"))
        xml_node.append_attribute("name").set_value(property_name);

    // Write properties
    for (const auto &prop : node.props) {
        std::string_view key = prop.name();
        // Skip internal properties
        if (key == "type" || key == "id")
            continue;
        Properties::Type prop_type = prop.type();

        // Handle resolved references (child nodes) separately as they don't create a property node
        if (prop_type == Properties::Type::ResolvedReference) {
            size_t child_idx = prop.get<Properties::ResolvedReference>().index();
            write_node_to_xml(writer_state, state, child_idx, xml_node, xml_nodes, key);
            continue;
        }

        pugi::xml_node prop_node;

        // Special handling for transforms - check if identity before creating node
        if (prop_type == Properties::Type::Transform) {
            ScalarMatrix4d mat = prop.get<ScalarAffineTransform4d>().matrix;
            using Array = dr::array_t<ScalarMatrix4d>;

            // Skip identity transforms entirely
            const double eps = 1e-6;
            if (!dr::any_nested(dr::abs(Array(mat - dr::identity<ScalarMatrix4d>())) > eps))
                continue;
        }

        // Get the tag name from property_type_name
        std::string_view type_name = property_type_name(prop_type);

        // Create the property node and add name attribute first
        prop_node = xml_node.append_child(type_name);
        prop_node.append_attribute("name").set_value(key);

        // Now add the value-specific attributes
        std::string value_str;
        bool has_value_attr = false;

        switch (prop_type) {
            case Properties::Type::Bool:
                value_str = prop.get<bool>() ? "true" : "false";
                has_value_attr = true;
                break;

            case Properties::Type::Integer:
                value_str = std::to_string(prop.get<int64_t>());
                has_value_attr = true;
                break;

            case Properties::Type::Float:
                value_str = tfm::format("%.17g", prop.get<double>());
                has_value_attr = true;
                break;

            case Properties::Type::String:
                value_str = prop.get<std::string_view>();
                has_value_attr = true;
                break;

            case Properties::Type::Vector: {
                ScalarVector3d vec = prop.get<ScalarVector3d>();
                prop_node.append_attribute("x").set_value(tfm::format("%.17g", vec.x()));
                prop_node.append_attribute("y").set_value(tfm::format("%.17g", vec.y()));
                prop_node.append_attribute("z").set_value(tfm::format("%.17g", vec.z()));
                break;
            }

            case Properties::Type::Color: {
                ScalarColor3d color = prop.get<ScalarColor3d>();
                prop_node.append_attribute("value").set_value(tfm::format("%.17g %.17g %.17g",
                                                              color[0], color[1], color[2]));
                break;
            }

            case Properties::Type::Spectrum: {
                const Properties::Spectrum &spec = prop.get<Properties::Spectrum>();

                if (spec.wavelengths.empty()) {
                    // Uniform spectrum
                    if (!spec.values.empty())
                        prop_node.append_attribute("value").set_value(tfm::format("%.17g", spec.values[0]));
                } else {
                    // Wavelength-value pairs
                    for (size_t i = 0; i < spec.wavelengths.size(); ++i) {
                        if (i > 0)
                            value_str += ", ";
                        value_str += tfm::format("%.17g:%.17g", spec.wavelengths[i], spec.values[i]);
                    }
                    prop_node.append_attribute("value").set_value(value_str);
                }
                break;
            }

            case Properties::Type::Transform: {
                ScalarAffineTransform4d transform = prop.get<ScalarAffineTransform4d>();

                // Try to decompose transform into one of several canonical formats
                if (decompose_transform(transform, prop_node,
                                        node.type == ObjectType::Sensor))
                    break;

                // Fall back to matrix representation
                ScalarMatrix4d m = transform.matrix;
                pugi::xml_node matrix_node = prop_node.append_child("matrix");

                // Format the matrix values
                std::string matrix_str;
                for (int i = 0; i < 4; ++i) {
                    for (int j = 0; j < 4; ++j) {
                        if (i > 0 || j > 0)
                            matrix_str += " ";
                        matrix_str += tfm::format("%.17g", m(i, j));
                    }
                }

                matrix_node.append_attribute("value").set_value(matrix_str);
                break;
            }

            case Properties::Type::Reference:
                prop_node.append_attribute("id") = prop.get<Properties::Reference>().id();
                break;

            default:
                Log(Error, "Encountered property \"%s\" with unsupported type \"%s\"", key, type_name);
        }

        // For properties with name/value attributes, check if this should be a default
        if (has_value_attr) {
            for (auto &param : writer_state.defaults) {
                if (param.property_name == key) {
                    if (!param.used) {
                        param.value = value_str;
                        param.used = true;
                    }
                    if (param.value == value_str)
                        value_str = tfm::format("$%s", param.default_name);
                    break;
                }
            }

            prop_node.append_attribute("value").set_value(value_str);
        }
    }
}

// Helper function to recursively add newlines before elements with children
static void add_xml_newlines(pugi::xml_node parent, int indent = 1) {
    bool is_first = true;
    for (pugi::xml_node child = parent.first_child(); child; child = child.next_sibling()) {
        if (child.type() != pugi::node_element)
            continue;
        if (!is_first && child.first_child())
            parent.insert_child_before(pugi::node_pcdata, child).set_value("\n\n" + std::string(indent * 4, ' '));
        add_xml_newlines(child, indent + 1);
        is_first = false;
    }
}

// Helper function to generate XML document from ParserState
static pugi::xml_document generate_xml_document(const ParserState &state, bool add_section_headers) {
    pugi::xml_document doc;

    if (state.empty())
        return doc;

    // First pass: count all references to each node
    WriterState writer_state(state.size());
    for (const auto &node : state.nodes) {
        for (const auto &prop : node.props) {
            if (prop.type() == Properties::Type::Reference) {
                if (auto it = state.id_to_index.find(prop.get<Properties::Reference>().id());
                    it != state.id_to_index.end()) {
                    writer_state.refcount[it->second]++;
                }
            } else if (prop.type() == Properties::Type::ResolvedReference) {
                size_t index = prop.get<Properties::ResolvedReference>().index();
                writer_state.refcount[index]++;
            }
        }
    }

    // Second pass: turn the scene into XML
    std::vector<pugi::xml_node> xml_nodes(state.size());
    write_node_to_xml(writer_state, state, 0, doc, xml_nodes, "");

    // Insert default tags at the beginning of the root node
    pugi::xml_node root = xml_nodes[0], node;
    for (const auto &param : writer_state.defaults) {
        if (!param.used)
            continue;
        node = node ?
            root.insert_child_after("default", node) :
            root.prepend_child("default");
        node.append_attribute("name").set_value(param.default_name);
        node.append_attribute("value").set_value(param.value);
    }

    // Add newlines to improve readability of the document
    add_xml_newlines(doc, 0);

    // Add section headers if requested
    if (add_section_headers && !state.empty() && state.root().type == ObjectType::Scene) {
        std::string_view last_section_name;

        // Iterate through immediate children and add section headers
        for (const auto &prop : state.root().props.filter(Properties::Type::ResolvedReference)) {
            size_t idx = prop.get<Properties::ResolvedReference>().index();
            int order_id = node_order_id(state, idx);

            constexpr size_t section_names_size = sizeof(section_names) / sizeof(section_names[0]);
            if (order_id < (int) section_names_size && section_names[order_id][0] != '\0') {
                std::string_view section_name = section_names[order_id];

                if (section_name != last_section_name) {
                    // Prepend section header comment to this node
                    pugi::xml_node comment = root.insert_child_before(pugi::node_comment, xml_nodes[idx]);
                    comment.set_value((" " + std::string(section_name) + " ").c_str());

                    last_section_name = section_name;
                }
            }
        }
    }

    return doc;
}


void write_file(const ParserState &state, const fs::path &filename, bool add_section_headers) {
    pugi::xml_document doc = generate_xml_document(state, add_section_headers);

    if (!doc.save_file(filename.string().c_str(), "    ",
             pugi::format_no_declaration | pugi::format_indent,
             pugi::encoding_utf8))
        Throw("Failed to write XML to file: %s", filename.string());
}

std::string write_string(const ParserState &state, bool add_section_headers) {
    struct string_writer : pugi::xml_writer {
        std::string result;

        virtual void write(const void* data, size_t size) {
            result.append((const char *) data, size);
        }
    };

    pugi::xml_document doc = generate_xml_document(state, add_section_headers);
    string_writer writer;
    doc.save(writer, "    ",
             pugi::format_no_declaration | pugi::format_indent,
             pugi::encoding_utf8);

    return writer.result;
}

bool SceneNode::operator==(const SceneNode &other) const {
    // Only compare type and props - file_index and offset are metadata
    // used for error reporting and vary between parsing methods
    return type == other.type && props == other.props;
}

bool ParserState::operator==(const ParserState &other) const {
    // Compare all the members that affect the scene structure
    if (nodes.size() != other.nodes.size())
        return false;

    // Compare nodes
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i] != other.nodes[i])
            return false;
    }

    // Note: We don't compare the remaining fields as they store parsing-related
    // metadata, not the actual scene structure.

    return true;
}

NAMESPACE_END(parser)
NAMESPACE_END(mitsuba)
