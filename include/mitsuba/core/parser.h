#pragma once

#include <mitsuba/core/properties.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/filesystem.h>
#include <tsl/robin_map.h>

/**
 * \brief Parser infrastructure for Mitsuba scene loading
 *
 * This file implements a unified parser for loading scenes from XML files
 * or Python dictionaries. The parsing is split into 3 stages:
 *
 * 1. Parsing: \ref parse_file(), \ref parse_string(), \ref parse_dict() (in a separate file, `src/core/python/parser.cpp`)
 *    - These functions convert XML/dict input into an intermediate representation (SceneNode).
 *    - They handle parameter substitution and file includes.
 *    - They validate structure and capture metadata.
 *    - This stage is variant-independent.
 *
 * 2. Transformations: \ref transform_upgrade(), \ref transform_colors()
 *    - The upgrade transformation updates old scene formats to the latest version (variant-independent).
 *    - The color transformation performs color space conversions based on the selected variant (variant-dependent).
 *    - Additional semantic transformations may be applied as needed.
 *
 * 3. Instantiation: \ref instantiate()
 *    - This function creates actual Mitsuba objects from the intermediate representation.
 *    - It handles object references and dependencies.
 *    - It supports parallel instantiation for performance.
 *    - This stage is variant-dependent as it instantiates variant-specific plugin implementations.
 */

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(parser)

/// List of key=value pairs used to substitute scene parameters during parsing
using ParameterList = std::vector<std::pair<std::string, std::string>>;

/**
 * \brief Configuration options for the parser
 *
 * This structure contains various options that control parser behavior,
 * such as how to handle unused parameters and other validation settings.
 */
struct ParserConfig {
    /// How to handle unused parameters: Error (default), Warn, or Debug
    LogLevel unused_parameters = LogLevel::Error;

    /// Maximum include depth to prevent infinite recursion
    int max_include_depth = 15;
};

/**
 * \brief Intermediate scene object representation
 *
 * This class stores information needed to instantiate a Mitsuba object at some
 * future point in time, including its plugin name and any parameters to be supplied.
 * The \ref parse_string and \ref parse_file functions below turn an XML file or string
 * into a sequence of \ref SceneNode instances that may undergo further transformation
 * before finally being instantiated.
 *
 * Each node represents a scene object (shape, bsdf, emitter, etc.) with its properties.
 *
 * The node hierarchy is stored as indices into the ParserState::nodes vector,
 * allowing efficient manipulation and traversal.
 */
struct SceneNode {
    /// Object type of this node (if known)
    /// Used for validation and type-specific transformations
    /// (unused in the dict parser)
    ObjectType type = ObjectType::Unknown;

    /// File index, identifies an entry of ParserState::files
    /// Used for error reporting to show which file contains this node
    /// (unused in the dict parser)
    uint32_t file_index = 0;

    /// Byte offset in the file where this node is found in the XML file
    /// Used for precise error reporting with line/column information
    /// (unused in the dict parser)
    size_t offset = 0;

    /// Stores the plugin attributes, sub-objects, ID, and plugin name.
    /// References are also stored here initially before they are resolved
    /// into objects during instantiation
    Properties props;

    /// Child objects. This identifies entries of ParserState::nodes data
    /// structure that will eventually be instantiated and added to `props`.
    /// Each pair contains the property name and the index of the child node
    /// in the ParserState::nodes vector.
    std::vector<std::pair<std::string, uint32_t>> children;
};

/// Color processing mode to use when loading the scene data
enum class ColorMode {
    /// Convert all color data to grayscale
    Monochromatic,
    /// Keep colors as RGB triplets
    RGB,
    /// Use full spectral representation
    Spectral
};

/// Keeps track of common state while parsing an XML file or dictionary
struct ParserState {
    /// The list of all scene nodes. The root node is at position 0.
    /// Nodes are added during parsing and never removed, only modified
    std::vector<SceneNode> nodes;

    /// List of files that were parsed while loading for error reporting.
    /// Indexed by \ref SceneNode.file_index, unused in the dictionary parser
    std::vector<fs::path> files;

    /// Version number of each parsed file in \ref files. Used by
    /// transform_upgrade() to apply appropriate upgrades per file
    std::vector<util::Version> versions;

    /// Maps named nodes with an ``id`` attribute to their index in ``nodes``
    /// Allows efficient lookup of objects for reference resolution
    tsl::robin_map<std::string, uint32_t> id_to_index;

    /// When parsing a file via parse_string(), this references the string
    /// contents. Used to compute line information for error messages.
    std::string_view content;

    /// Current include depth (for preventing infinite recursion)
    int depth = 0;

    /// Convenience operator for accessing nodes (const version)
    const SceneNode &operator[](uint32_t index) const {
        #if !defined(NDEBUG)
                return nodes.at(index);
        #else
                return nodes[index];
        #endif
    }

    /// Convenience operator for accessing nodes
    SceneNode &operator[](uint32_t index) {
        #if !defined(NDEBUG)
                return nodes.at(index);
        #else
                return nodes[index];
        #endif
    }

    /// Return the root node
    SceneNode &root() { return operator[](0); };
    const SceneNode &root() const { return operator[](0); };
};

/**
 * \brief Parse a scene from an XML file and return the resulting parser state
 *
 * This function loads an XML file and converts it to the intermediate
 * representation. It handles
 *
 * - File includes via <include> tags
 * - Parameter substitution using the provided parameter list
 * - Basic structural validation
 * - Source location tracking for error reporting
 *
 * It does no further interpretation/instantiation.
 *
 * \param filename Path to the XML file to load
 * \param params List of parameter substitutions to apply
 * \param config Parser configuration options
 * \return Parser state containing the scene graph
 */
extern MI_EXPORT_LIB ParserState parse_file(
    const fs::path &filename,
    const ParameterList &params = {},
    const ParserConfig &config = {}
);

/**
 * \brief Parse a scene from an XML string and return the resulting parser state
 *
 * Similar to parse_file() but takes the XML content as a string.
 * Useful for programmatically generated scenes or testing.
 *
 * \param string XML content to parse
 * \param params List of parameter substitutions to apply
 * \param config Parser configuration options
 * \return Parser state containing the scene graph
 */
extern MI_EXPORT_LIB ParserState parse_string(
    std::string_view string,
    const ParameterList &params = {},
    const ParserConfig &config = {}
);

/**
 * \brief Upgrade scene data to the latest version
 *
 * This transformation updates older scene formats for compatibility with the
 * current Mitsuba version. It performs the following steps:
 *
 * - Converting property names from camelCase to underscore_case (version < 2.0)
 * - Upgrading deprecated plugin names/parameters to newer equivalents
 *
 * \param ctx Parser state to modify in-place
 */
extern MI_EXPORT_LIB void transform_upgrade(ParserState &ctx);

/**
 * \brief Apply color-related transformations based on the current variant
 *
 * This transformation handles color space conversions and spectrum
 * construction based on the active variant. It performs:
 *
 * - RGB to monochromatic conversion for grayscale variants
 * - Spectrum construction from RGB values for spectral variants
 * - Insertion of spectral upsampling/downsampling plugins where needed
 * - Validation of color data for the target variant
 *
 * \param ctx Parser state to modify in-place
 * \param variant Name of the target variant (e.g., "scalar_rgb", "cuda_spectral")
 */
extern MI_EXPORT_LIB void transform_colors(ParserState &ctx, std::string_view variant);

/**
 * \brief Generate a human-readable file location string for error reporting
 *
 * Returns a string in the format "filename.xml:line:col" for XML nodes,
 * or just the node description for dictionary-parsed nodes.
 *
 * \param ctx Parser state containing file information
 * \param node Scene node to locate
 * \return Human-readable location string
 */
extern MI_EXPORT_LIB std::string file_location(const ParserState &ctx,
                                               const SceneNode &node);

/**
 * \brief Instantiate the scene graph into actual Mitsuba objects
 *
 * This final stage creates the actual scene objects from the intermediate
 * representation. It handles:
 *
 * - Plugin instantiation via the PluginManager
 * - Reference resolution between objects
 * - Dependency ordering for correct instantiation order
 * - Parallel instantiation of independent objects (if enabled)
 * - Property validation and type checking
 *
 * \param ctx Parser state containing the scene graph
 * \param parallel Enable parallel instantiation for better performance
 * \return The root object of the instantiated scene
 */
extern MI_EXPORT_LIB ref<Object> instantiate(ParserState &ctx,
                                             std::string_view variant,
                                             bool parallel = true);

/**
 * \brief Write scene data back to an XML file
 *
 * This function converts the intermediate representation back to XML format
 * and writes it to disk. Useful for:
 * - Converting between scene formats (dict to XML)
 * - Saving programmatically generated scenes
 * - Debugging the parser's intermediate representation
 *
 * \param ctx Parser state containing the scene graph
 * \param filename Path where the XML file should be written
 */
extern MI_EXPORT_LIB void write_file(const ParserState &ctx,
                                     const fs::path &filename);

/**
 * \brief Convert scene data to an XML string
 *
 * Similar to write_file() but returns the XML content as a string
 * instead of writing to disk.
 *
 * \param ctx Parser state containing the scene graph
 * \return XML representation of the scene
 */
extern MI_EXPORT_LIB std::string write_string(const ParserState &ctx);

NAMESPACE_END(parser)
NAMESPACE_END(mitsuba)
