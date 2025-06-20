#include <mitsuba/core/parser.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/formatter.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/frame.h>
#include <mitsuba/mitsuba.h>
#include <pugixml.hpp>
#include <algorithm>
#include <string_view>
#include <charconv>


using namespace std::string_view_literals;

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(parser)

// Import core types
using Vector3f = Vector<double, 3>;
using Color3f = Color<double, 3>;
using Point3f = Point<double, 3>;
using Point4f = Point<double, 4>;
using Matrix4f = dr::Matrix<double, 4>;
using Transform4f = Transform<Point4f>;


/**
 * \brief Structure to track parameter substitutions in XML parsing
 *
 * This structure holds information about a single parameter that can be
 * substituted in XML attributes using the $parameter_name syntax.
 */
struct SortedParameter {
    std::string key;         ///< The search key including dollar prefix (e.g., "$myParam")
    std::string_view value;  ///< The replacement value
    bool used = false;       ///< Track whether this parameter was used
};

/// List of parameters sorted by search key length (longest first) for correct substitution
using SortedParameters = std::vector<SortedParameter>;

// Forward declaration
template <typename... Args>
[[noreturn]] static void fail(const ParserState &state, const SceneNode &node,
                              std::string_view msg, Args&&... args);

// Forward declarations
static void parse_xml_node(ParserState &state, const ParserConfig &config,
                           pugi::xml_node node, uint32_t parent_idx,
                           SortedParameters &params, size_t &arg_counter,
                           uint32_t file_index);

static void parse_transform_node(const ParserState &state, pugi::xml_node node,
                                 const SceneNode &scene_node, Transform4f &transform,
                                 SortedParameters &params);

static ParserState parse_file_impl(const fs::path &filename, SortedParameters &params,
                                   const ParserConfig &config, int depth);

static void check_attributes(const ParserState &state, const SceneNode &node,
                             pugi::xml_node xml_node,
                             std::initializer_list<std::string_view> args);



/// A list of all tag types that can be encountered in an XML file (excluding the various object type tags)
enum class TagType {
    Boolean, Integer, Float, String, Point, Vector, Spectrum, RGB,
    Transform, Translate, Matrix, Rotate, Scale, LookAt, Object,
    NamedReference, Include, Alias, Default, Resource, Invalid
};

/**
 * \brief Interprets an XML tag name and returns its type classification
 *
 * This function takes an XML tag name and returns a pair containing:
 * - For property tags (e.g., "float", "rgb", "transform"):
 *   Returns (appropriate TagType, ObjectType::Unknown)
 * - For object tags (e.g., "scene", "bsdf", "shape"):
 *   Returns (TagType::Object, appropriate ObjectType)
 *
 * \param str The XML tag name to interpret
 * \return A pair of (TagType, ObjectType) indicating the tag classification
 */
static std::pair<TagType, ObjectType> interpret_tag(std::string_view str) {
    if (str.empty())
        return {TagType::Invalid, ObjectType::Unknown};

    // Use switch on first character to reduce comparisons
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
 * \param attr The XML attribute to modify
 * \param params List of parameters sorted by search key length
 * \param state Parser state for error reporting
 * \param node Scene node for error location tracking
 */
static void substitute_parameters(pugi::xml_attribute &attr,
                                 SortedParameters &params,
                                 const ParserState &state, const SceneNode &node) {
    std::string value = attr.value();

    // Perform substitutions using pre-sorted parameters
    for (auto &param : params) {
        size_t pos = 0;
        while ((pos = value.find(param.key, pos)) != std::string::npos) {
            value.replace(pos, param.key.length(), param.value);
            param.used = true;
            pos += param.value.length();
        }
    }

    // Check if there are still unresolved parameters
    size_t pos = value.find('$');
    if (pos != std::string::npos) {
        // Find the parameter name after '$'
        size_t end_pos = pos + 1;
        while (end_pos < value.size() &&
               (std::isalnum(value[end_pos]) || value[end_pos] == '_')) {
            ++end_pos;
        }
        std::string param_name = value.substr(pos + 1, end_pos - pos - 1);
        fail(state, node, "Undefined parameter: $%s", param_name);
    }

    // Update the attribute with the substituted value
    attr.set_value(value.c_str());
}

ParserState parse_file(const fs::path &filename, const ParameterList &param_list, const ParserConfig &config) {
    Log(Debug, "parse_file called with filename: %s", filename.string());

    // Build sorted parameter list
    SortedParameters params;

    for (const auto &[key, value] : param_list) {
        SortedParameter param;
        param.key = "$" + key;
        param.value = value;
        param.used = false;
        params.push_back(std::move(param));
    }

    // Sort parameters by key length (longest first) to handle cases where
    // one parameter name is a prefix of another
    std::sort(params.begin(), params.end(),
              [](const auto &a, const auto &b) {
                  return a.key.size() > b.key.size();
              });

    return parse_file_impl(filename, params, config, 0);
}

static ParserState parse_file_impl(const fs::path &filename, SortedParameters &params,
                                  const ParserConfig &config, int depth) {
    Log(Info, "Loading XML file \"%s\" ..", filename);

    // Check recursion depth
    if (depth >= config.max_include_depth)
        Throw("Exceeded maximum include recursion depth of %d", config.max_include_depth);

    // Check if file exists
    if (!fs::exists(filename))
        Throw("File not found: %s", filename.string());

    ParserState state;
    state.depth = depth;

    // The root node will be created based on the actual root element in XML
    // (It's not pre-created as scene, this will be determined by the XML content)

    // Store filename
    state.files.push_back(filename);

    // Parse XML using pugixml
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filename.native().c_str());

    if (!result) {
        Throw("XML parsing failed: %s (at offset %td)",
              result.description(), result.offset);
    }

    // Get the root element
    pugi::xml_node root_node = doc.document_element();

    // Check for version attribute
    if (!root_node.attribute("version"))
        Throw("Missing version attribute in root element <%s>", root_node.name());

    std::string_view version_str = root_node.attribute("version").value();
    try {
        state.versions.push_back(util::Version(version_str));
    } catch (...) {
        Throw("Invalid version number: %s", version_str);
    }

    // Parse root node if it's not empty
    if (root_node) {
        // Parse the root element itself as the first node
        size_t arg_counter = 0;
        parse_xml_node(state, config, root_node, 0, params, arg_counter, 0);
    }

    // Check for unused parameters only at the top level
    if (depth == 0) {
        for (const auto &param : params) {
            if (!param.used) {
                // Log at the specified level (this might also throw an exception if level is Error)
                Log(config.unused_parameters, "Unused parameter: '%s' = '%s'",
                    param.key.substr(1), param.value);
            }
        }
    }

    return state;
}

ParserState parse_string(std::string_view string, const ParameterList &param_list, const ParserConfig &config) {
    Log(Debug, "parse_string called with %zu characters", string.size());

    // Build sorted parameter list
    SortedParameters params;

    for (const auto &[key, value] : param_list) {
        SortedParameter param;
        param.key = "$" + key;
        param.value = value;
        param.used = false;
        params.push_back(std::move(param));
    }

    // Sort parameters by key length (longest first) to handle cases where
    // one parameter name is a prefix of another
    std::sort(params.begin(), params.end(),
              [](const auto &a, const auto &b) {
                  return a.key.size() > b.key.size();
              });

    ParserState state;
    state.depth = 0;
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
    if (!root_node.attribute("version"))
        Throw("Missing version attribute in root element <%s>", root_node.name());

    std::string_view version_str = root_node.attribute("version").value();
    try {
        state.versions.push_back(util::Version(version_str));
    } catch (...) {
        Throw("Invalid version number: %s", version_str);
    }

    // Parse root node if it's not empty
    if (root_node) {
        // Parse the root element itself as the first node
        size_t arg_counter = 0;
        parse_xml_node(state, config, root_node, 0, params, arg_counter, 0);
    }

    // Check for unused parameters
    for (const auto &param : params) {
        if (!param.used) {
            // Log at the specified level (this might also throw an exception if level is Error)
            Log(config.unused_parameters, "Unused parameter: '%s' = '%s'",
                param.key.substr(1), std::string(param.value));
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

void transform_colors(ParserState &ctx, std::string_view variant) {
    Log(Info, "transform_colors called on %zu nodes with variant %s",
        ctx.nodes.size(), variant);

    // TODO: Implement color transformations
    // - RGB to monochromatic conversion
    // - Spectrum construction from RGB
    // - Insert upsampling/downsampling plugins
}



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
            node.append_attribute("x") = tokens[0];
            node.append_attribute("y") = tokens[0];
            node.append_attribute("z") = tokens[0];
        } else if (tokens.size() == 3) {
            // Three values - one for each component
            node.append_attribute("x") = tokens[0];
            node.append_attribute("y") = tokens[1];
            node.append_attribute("z") = tokens[2];
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

// Helper function to parse a vector from a space-separated string
static Vector3f parse_vector_from_string(const ParserState &state, const SceneNode &node,
                                        std::string_view str) {
    auto tokens = string::tokenize(str);
    if (tokens.size() != 3)
        fail(state, node, "Expected 3 values in vector, got %zu", tokens.size());

    try {
        return Vector3f(string::stof<double>(tokens[0]),
                        string::stof<double>(tokens[1]),
                        string::stof<double>(tokens[2]));
    } catch (...) {
        fail(state, node, "Could not parse vector value '%s'", str);
    }
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

ref<Object> instantiate(ParserState &ctx, std::string_view variant, bool parallel) {
    Log(Info, "instantiate called with %zu nodes, parallel=%s, variant=%s",
        ctx.nodes.size(), parallel ? "true" : "false", variant);

    if (ctx.nodes.empty())
        Throw("No nodes to instantiate");

    // TODO: Implement actual instantiation
    // For now, just return a dummy object
    auto plugin_mgr = PluginManager::instance();

    // Create a simple scene object as placeholder
    Properties props("scene");
    props.set_id("dummy_scene");

    return plugin_mgr->create_object(props, variant, ObjectType::Scene);
}

// Helper function to parse transform nodes
static void parse_transform_node(const ParserState &state, pugi::xml_node node,
                                 const SceneNode &scene_node,
                                 Transform4f &transform,
                                 SortedParameters &params) {
    // Apply parameter substitution to all attributes
    for (auto attr : node.attributes()) {
        std::string value = attr.value();
        if (value.find('$') != std::string::npos)
            substitute_parameters(attr, params, state, scene_node);
    }

    std::string_view node_name(node.name());
    auto [tag_type, object_type] = interpret_tag(node_name);

    switch (tag_type) {
        case TagType::Translate: {
            // First expand value attribute if present
            expand_value_to_xyz(node);
            check_attributes(state, scene_node, node, {"x"sv, "y"sv, "z"sv});

            Vector3f vec = parse_vector(state, scene_node, node);
            transform = Transform4f::translate(vec) * transform;
            break;
        }

        case TagType::Rotate: {
            // First expand value attribute if present
            expand_value_to_xyz(node);
            check_attributes(state, scene_node, node, {"!angle"sv, "x"sv, "y"sv, "z"sv});

            Vector3f axis = parse_vector(state, scene_node, node, 0.0);  // Default to 0 for missing components
            const char *angle_str = node.attribute("angle").value();

            // Fail when the axis is zero
            if (dr::all(axis == 0))
                fail(state, scene_node, "Rotation axis cannot be zero");

            try {
                double angle = string::stof<double>(angle_str);
                transform = Transform4f::rotate(axis, angle) * transform;
            } catch (...) {
                fail(state, scene_node, "Could not parse angle value '%s'", angle_str);
            }
            break;
        }

        case TagType::Scale: {
            // First expand value attribute if present
            expand_value_to_xyz(node);
            check_attributes(state, scene_node, node, {"x"sv, "y"sv, "z"sv});

            Vector3f vec = parse_vector(state, scene_node, node, 1.0);
            transform = Transform4f::scale(vec) * transform;
            break;
        }

        case TagType::LookAt: {
            check_attributes(state, scene_node, node, {"!origin"sv, "!target"sv, "up"sv});

            std::string_view origin_str = node.attribute("origin").value();
            std::string_view target_str = node.attribute("target").value();
            std::string_view up_str = node.attribute("up").value();

            Vector3f origin = parse_vector_from_string(state, scene_node, origin_str),
                     target = parse_vector_from_string(state, scene_node, target_str),
                     up     = Vector3f(0);

            if (!up_str.empty())
                up = parse_vector_from_string(state, scene_node, up_str);

            // If up vector is zero, generate one
            if (dr::all(up == 0)) {
                Vector3f dir = dr::normalize(target - origin);
                std::tie(up, std::ignore) = coordinate_system(dir);
            }

            transform = Transform4f::look_at(origin, target, up) * transform;
            if (dr::any_nested(dr::isnan(transform.matrix)))
                fail(state, scene_node, "Invalid lookat transformation");

            break;
        }

        case TagType::Matrix: {
            check_attributes(state, scene_node, node, {"!value"sv});

            const char *value_str = node.attribute("value").value();
            auto tokens = string::tokenize(value_str);

            if (tokens.size() != 16 && tokens.size() != 9)
                fail(state, scene_node, "Matrix must have 9 or 16 values, got %zu", tokens.size());

            int n = tokens.size() == 16 ? 4 : 3;

            Matrix4f m = dr::identity<Matrix4f>();
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    try {
                        m(i, j) = string::stof<double>(tokens[i * n + j]);
                    } catch (...) {
                        fail(state, scene_node, "Could not parse matrix value '%s'",
                             tokens[i * n + j]);
                    }
                }
            }

            transform = Transform4f(m) * transform;
            break;
        }

        default:
            fail(state, scene_node, "Unexpected tag '%s' inside <transform>", node_name.data());
    }
}

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
            fail(state, node, "Unexpected attribute '%s' in element <%s>",
                 attr.name(), xml_node.name());
    }

    // Check required attributes are present
    for (auto arg : args) {
        if (arg[0] != '!')
            continue;

        std::string_view name = arg.substr(1);
        if (!xml_node.attribute(name))
            fail(state, node, "Missing required attribute '%s' in <%s>",
                 std::string(name), xml_node.name());
    }
}

// Helper function to parse XML nodes recursively
static void parse_xml_node(ParserState &state, const ParserConfig &config,
                          pugi::xml_node node, uint32_t parent_idx,
                          SortedParameters &params, size_t &arg_counter,
                          uint32_t file_index) {
    // Skip comments and other non-element nodes
    if (node.type() != pugi::node_element)
        return;

    // Check if this will be the root node
    bool is_root = parent_idx == 0 && state.nodes.empty();

    // Apply parameter substitution to ALL attributes first (or check for undefined parameters)
    // Create a temporary scene node for error reporting
    SceneNode temp_node;
    temp_node.file_index = file_index;
    temp_node.offset = node.offset_debug();

    for (auto attr : node.attributes()) {
        std::string value = attr.value();
        if (value.find('$') != std::string::npos)
            substitute_parameters(attr, params, state, temp_node);
    }

    std::string_view node_name = node.name();
    auto [tag_type, object_type] = interpret_tag(node_name);

    if (tag_type == TagType::Invalid)
        Throw("Unknown XML tag: %s", node_name);

    // Create a new scene node
    SceneNode scene_node;
    scene_node.type = object_type;
    scene_node.file_index = file_index;
    scene_node.offset = node.offset_debug();

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
            // Float tags need name and value attributes
            check_attributes(state, scene_node, node, {"!name"sv, "!value"sv});

            const char *name      = node.attribute("name").value(),
                       *value_str = node.attribute("value").value();

            try {
                double value = string::stof<double>(value_str);

                // Store in parent's properties
                state.nodes[parent_idx].props.set(name, value);
            } catch (...) {
                fail(state, scene_node, "Could not parse floating point value '%s'",
                    value_str);
            }
            break;
        }

        case TagType::Integer: {
            // Integer tags need name and value attributes
            check_attributes(state, scene_node, node, {"!name"sv, "!value"sv});

            std::string_view name      = node.attribute("name").value(),
                             value_str = node.attribute("value").value();

            int64_t value = 0;
            auto [ptr, err] = std::from_chars(
                value_str.data(), value_str.data() + value_str.size(), value);

            if (err != std::errc{})
                fail(state, scene_node, "Could not parse integer point value '%s'",
                    value_str);

            for (const char *c = ptr; c < value_str.data() + value_str.size(); ++c) {
                if (!std::isspace(*c))
                    Throw("Invalid trailing characters in integer \"%s\"", value_str);
            }

            state.nodes[parent_idx].props.set(name, value);
            break;
        }

        case TagType::Boolean: {
            // Boolean tags need name and value attributes
            check_attributes(state, scene_node, node, {"!name"sv, "!value"sv});

            const char *name      = node.attribute("name").value(),
                       *value_str = node.attribute("value").value();

            // Parse boolean value
            bool value;
            if (string::iequals(value_str, "true"))
                value = true;
            else if (string::iequals(value_str, "false"))
                value = false;
            else
                fail(state, scene_node, "Could not parse boolean value '%s' - must be 'true' or 'false'",
                    value_str);

            // Store in parent's properties
            state.nodes[parent_idx].props.set(name, value);
            break;
        }

        case TagType::String: {
            // String tags need name and value attributes
            check_attributes(state, scene_node, node, {"!name"sv, "!value"sv});

            std::string_view name  = node.attribute("name").value(),
                             value = node.attribute("value").value();

            // Store in parent's properties
            state.nodes[parent_idx].props.set(name, std::string(value));
            break;
        }

        case TagType::Vector:
        case TagType::Point: {
            // First expand value attribute if present (removes 'value' and adds x/y/z)
            expand_value_to_xyz(node);

            // Now check for required attributes (always x/y/z after expansion)
            check_attributes(state, scene_node, node, {"!name"sv, "!x"sv, "!y"sv, "!z"sv});

            std::string_view name = node.attribute("name").value();
            Vector3f vec = parse_vector(state, scene_node, node);

            // Store in parent's properties
            state.nodes[parent_idx].props.set(name, vec);
            break;
        }

        case TagType::RGB: {
            // RGB tags need name and value attributes
            check_attributes(state, scene_node, node, {"!name"sv, "!value"sv});

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
            state.nodes[parent_idx].props.set(name, color);
            break;
        }

        case TagType::Include: {
            // Include tags need filename attribute
            check_attributes(state, scene_node, node, {"!filename"sv, "name"sv});

            const char *filename_str = node.attribute("filename").value();

            // Resolve the file path
            fs::path include_path(filename_str);

            // Make relative paths relative to the current file's directory
            if (!include_path.is_absolute() && file_index < state.files.size())
                include_path = state.files[file_index].parent_path() / include_path;

            // Check if file exists
            if (!fs::exists(include_path))
                fail(state, scene_node, "Included file \"%s\" not found", include_path.string());

            Log(Info, "Loading included XML file \"%s\" ..", include_path.string());

            // Parse the included file recursively with increased depth
            ParserState inc_state;
            try {
                inc_state = parse_file_impl(include_path, params, config, state.depth + 1);
            } catch (const std::exception &e) {
                // Add context about which file included this one
                fail(state, scene_node, "while processing <include>:\n%s", e.what());
            }

            // Merge the included nodes into our state
            if (!inc_state.nodes.empty()) {
                const SceneNode& inc_root = inc_state.root();

                // Omit the root node when the included file is a scene
                size_t skip = inc_root.props.plugin_name() == "scene" ? 1 : 0;

                if (inc_state.nodes.size() <= skip)
                    break; // Empty scene

                uint32_t node_offset = (uint32_t) state.nodes.size();
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
                for (size_t i = node_offset; i < state.nodes.size(); ++i) {
                    state.nodes[i].file_index += file_offset;
                    for (auto &[name, child] : state.nodes[i].children)
                        child += node_offset - skip;
                }

                // Link to parent
                if (skip) {
                    for (const auto &[name, child] : inc_root.children)
                        state.nodes[parent_idx].children.push_back({name, child + node_offset - skip});
                } else {
                    // For non-scene includes, we need to get the property name
                    // from the original node's name attribute
                    std::string prop_name = node.attribute("name").value();
                    if (prop_name.empty())
                        prop_name = tfm::format("_arg_%zu", arg_counter++);
                    state.nodes[parent_idx].children.push_back({prop_name, node_offset});
                }

                // Update ID mappings
                for (const auto &[id, idx] : inc_state.id_to_index) {
                    if (skip == 0 || idx > 0) {
                        uint32_t new_idx = idx + node_offset - skip;
                        if (state.id_to_index.count(id))
                            fail(state, state.nodes[new_idx], "Duplicate ID: '%s'", id.c_str());
                        state.id_to_index[id] = new_idx;
                    }
                }
            }
            break;
        }

        case TagType::Transform: {
            // Transform tags need a name attribute
            check_attributes(state, scene_node, node, {"!name"sv});

            const char *name = node.attribute("name").value();

            // Create a fresh transform for this scope
            Transform4f transform;

            // Process child transform operations
            for (pugi::xml_node child : node.children()) {
                if (child.type() == pugi::node_element)
                    parse_transform_node(state, child, scene_node, transform, params);
            }

            // Store the accumulated transform
            state.nodes[parent_idx].props.set(name, transform);

            return; // Don't process children again
        }

        case TagType::Translate:
        case TagType::Rotate:
        case TagType::Scale:
        case TagType::LookAt:
        case TagType::Matrix:
            // Transform operations are only allowed inside <transform> tags
            fail(state, scene_node, "Transform operation <%s> can only appear inside a <transform> tag",
                 node.name());

        case TagType::NamedReference: {
            // Reference tags need id and name attributes
            check_attributes(state, scene_node, node, {"!id"sv, "!name"sv});

            const char *ref_id = node.attribute("id").value();
            const char *ref_name = node.attribute("name").value();

            // Add reference directly to parent's properties
            state.nodes[parent_idx].props.set(ref_name, Properties::Reference(ref_id));

            break;
        }

        default:
            // For now, ignore other tag types
            Log(Debug, "Ignoring tag type %d", (int)tag_type);
            break;
    }

    // Only add object nodes to the state (property nodes like float/int/string are handled above)
    if (tag_type == TagType::Object) {
        // Get property name from name attribute or auto-generate
        std::string property_name = node.attribute("name").value();
        if (property_name.empty())
            property_name = tfm::format("_arg_%zu", arg_counter++);

        // Add the node to the state
        uint32_t node_idx = (uint32_t)state.nodes.size();
        state.nodes.push_back(scene_node);

        // If we have a valid parent index and this is not the root, add it as a child
        if (!is_root)
            state.nodes[parent_idx].children.push_back({property_name, node_idx});

        // Recursively parse child nodes
        size_t child_arg_counter = 0;
        for (pugi::xml_node child : node.children())
            parse_xml_node(state, config, child, node_idx, params, child_arg_counter, file_index);
    }
}


// Forward declarations for XML writing
static void write_node_to_xml(const ParserState &state, uint32_t node_idx,
                              pugi::xml_node &xml_parent,
                              tsl::robin_map<uint32_t, bool> &written_nodes,
                              const std::string &property_name = "");
static pugi::xml_document generate_xml_document(const ParserState &ctx);

void write_file(const ParserState &ctx, const fs::path &filename) {
    Log(Debug, "write_file called with filename: %s", filename.string());

    // Generate XML document
    pugi::xml_document doc = generate_xml_document(ctx);

    // Write directly to file using pugixml
    if (!doc.save_file(filename.string().c_str(), "    "))
        Throw("Failed to write XML to file: %s", filename.string());
}

std::string write_string(const ParserState &ctx) {
    Log(Debug, "write_string called with %zu nodes", ctx.nodes.size());

    // Generate XML document
    pugi::xml_document doc = generate_xml_document(ctx);

    // Custom writer that builds a string
    struct string_writer : pugi::xml_writer {
        std::string result;

        virtual void write(const void* data, size_t size) {
            result.append(static_cast<const char*>(data), size);
        }
    };

    string_writer writer;
    doc.save(writer, "    ");

    return writer.result;
}

// Helper function to generate XML document from ParserState
static pugi::xml_document generate_xml_document(const ParserState &ctx) {
    pugi::xml_document doc;

    if (ctx.nodes.empty())
        return doc;

    // Track which nodes have been written (to handle references)
    tsl::robin_map<uint32_t, bool> written_nodes;

    // Write the root node (should be at index 0)
    write_node_to_xml(ctx, 0, doc, written_nodes);

    return doc;
}

// Helper function to write a single node to XML
static void write_node_to_xml(const ParserState &state, uint32_t node_idx,
                              pugi::xml_node &xml_parent,
                              tsl::robin_map<uint32_t, bool> &written_nodes,
                              const std::string &property_name) {
    if (node_idx >= state.nodes.size())
        return;

    const SceneNode &node = state.nodes[node_idx];

    // Check if this is a reference to an already-written node
    if (written_nodes.find(node_idx) != written_nodes.end()) {
        // This node was already written, so write a reference instead
        std::string_view id = node.props.id();

        // Write a reference instead of the full node
        pugi::xml_node ref_node = xml_parent.append_child("ref");

        if (!id.empty())
            ref_node.append_attribute("id").set_value(id);
        else // Generate an ID for nodes that don't have one
            ref_node.append_attribute("id").set_value(tfm::format("_unnamed_%u", node_idx));

        // Add name attribute if this node has a property name
        if (!property_name.empty() && !string::starts_with(property_name, "_arg_"))
            ref_node.append_attribute("name").set_value(property_name);

        return;
    }

    // Mark this node as written
    written_nodes[node_idx] = true;

    // Determine XML tag name based on object type and plugin name
    std::string_view tag_name;
    if (node.props.plugin_name() == "ref")
        tag_name = "ref";
    else if (node.props.plugin_name() == "scene")
        tag_name = "scene";
    else if (node.type != ObjectType::Unknown)
        tag_name = plugin_type_name(node.type);
    else
        Throw("Cannot determine XML tag name for node with unknown object type and plugin name '%s'",
              node.props.plugin_name());

    // Create XML element
    pugi::xml_node xml_node = xml_parent.append_child(tag_name);

    // Add version attribute for root scene element
    if (node_idx == 0 && tag_name == "scene") {
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

    // Add ID if present or generate one if this node is referenced elsewhere
    std::string_view id = node.props.id();
    if (!id.empty()) {
        xml_node.append_attribute("id").set_value(id);
    } else {
        // Check if this node is referenced elsewhere (appears multiple times in children lists)
        int ref_count = 0;
        for (const auto& other_node : state.nodes) {
            for (const auto& [child_name, child_idx] : other_node.children) {
                if (child_idx == node_idx)
                    ref_count++;
            }
        }

        // Generate an ID if this node is referenced multiple times
        if (ref_count > 1)
            xml_node.append_attribute("id").set_value(tfm::format("_unnamed_%u", node_idx));
    }

    // Add name attribute if not auto-generated
    if (!property_name.empty() && !string::starts_with(property_name, "_arg_"))
        xml_node.append_attribute("name").set_value(property_name);

    // Write properties
    for (const std::string &key : node.props.property_names()) {
        // Skip internal properties
        if (key == "type" || key == "id")
            continue;
        Properties::Type prop_type = node.props.type(key);

        pugi::xml_node prop;

        // Preserve references
        if (prop_type == Properties::Type::Reference) {
            pugi::xml_node ref_node = xml_node.append_child("ref");
            ref_node.append_attribute("id") =
                (const std::string &) node.props.get<Properties::Reference>(key);
            ref_node.append_attribute("name") = key;
            continue;
        }

        // Skip unsupported property types
        if (prop_type == Properties::Type::Object ||
            prop_type == Properties::Type::Any) {
            Log(Warn, "Skipping property '%s' with unsupported type", key.c_str());
            continue;
        }

        // Get the tag name from property_type_name
        std::string_view tag_name = property_type_name(prop_type);

        // Create the property node and add name attribute first
        prop = xml_node.append_child(tag_name);
        prop.append_attribute("name").set_value(key);

        // Now add the value-specific attributes
        switch (prop_type) {
            case Properties::Type::Bool:
                prop.append_attribute("value").set_value(node.props.get<bool>(key) ? "true" : "false");
                break;

            case Properties::Type::Integer:
                prop.append_attribute("value").set_value(std::to_string(node.props.get<int64_t>(key)));
                break;

            case Properties::Type::Float:
                prop.append_attribute("value").set_value(tfm::format("%.17g", node.props.get<double>(key)));
                break;

            case Properties::Type::String:
                prop.append_attribute("value").set_value(node.props.get<std::string_view>(key));
                break;

            case Properties::Type::Vector: {
                Vector3f vec = node.props.get<Vector3f>(key);
                prop.append_attribute("x").set_value(tfm::format("%.17g", vec.x()));
                prop.append_attribute("y").set_value(tfm::format("%.17g", vec.y()));
                prop.append_attribute("z").set_value(tfm::format("%.17g", vec.z()));
                break;
            }

            case Properties::Type::Color: {
                Color3f color = node.props.get<Color3f>(key);
                prop.append_attribute("value").set_value(tfm::format("%.17g %.17g %.17g",
                                                             color[0], color[1], color[2]));
                break;
            }

            case Properties::Type::Transform: {
                Matrix4f m = node.props.get<Transform4f>(key).matrix;
                pugi::xml_node matrix_node = prop.append_child("matrix");

                // Format the matrix values
                std::string matrix_str;
                for (int i = 0; i < 4; ++i) {
                    for (int j = 0; j < 4; ++j) {
                        if (i > 0 || j > 0) matrix_str += " ";
                        matrix_str += tfm::format("%.17g", m(i, j));
                    }
                }
                matrix_node.append_attribute("value").set_value(matrix_str);
                break;
            }

            default:
                break; // Already handled above
        }
    }

    // Write child nodes
    for (const auto &[name, child_idx] : node.children)
        write_node_to_xml(state, child_idx, xml_node, written_nodes, name);
}

NAMESPACE_END(parser)
NAMESPACE_END(mitsuba)
