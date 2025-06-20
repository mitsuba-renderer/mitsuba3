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
 * 1. Parsing: \ref parse_file(), \ref parse_string(), \ref parse_dict()
 *    - Converts XML/dict input into an intermediate representation (SceneNode)
 *    - Handles parameter substitution and file includes
 *    - Validates structure and captures metadata
 *
 * 2. Transformations: \ref transform_upgrade(), \ref transform_colors()
 *    - Upgrades old scene formats to the latest version
 *    - Performs color space conversions based on variant
 *    - Applies other semantic transformations
 *
 * 3. Instantiation: \ref instantiate()
 *    - Creates actual Mitsuba objects from the intermediate representation
 *    - Handles object references and dependencies
 *    - Supports parallel instantiation for performance
 */

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(parser)

/// List of key=value pairs used to substitute scene parameters during parsing
using ParameterList = std::vector<std::pair<std::string, std::string>>;

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
    /// The indices refer to positions in ParserState::nodes vector
    std::vector<uint32_t> children;

    /// Property name to use when adding this node to its parent
    /// (from the "name" attribute in XML or the key in a dictionary)
    /// For example: "material" for a BSDF child of a shape
    std::string property_name;
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

    /// List of files that were parsed while loading
    /// Used for error reporting and preventing circular includes
    /// (unused in the dictionary parser)
    std::vector<fs::path> files;

    /// Maps named nodes with an ``id`` attribute to their index in ``nodes``
    /// Allows efficient lookup of objects for reference resolution
    tsl::robin_map<std::string, uint32_t> id_to_index;

    /// Mode for processing color data based on the current variant
    ColorMode color_mode = ColorMode::RGB;

    /// CUDA/LLVM backend if applicable
    /// Determines which variant-specific code paths to use
    JitBackend backend = JitBackend::None;

    /// Variant name, e.g., "cuda_ad_rgb"
    /// Used for variant-specific transformations and validation
    std::string variant;

    /// Stores the version number of the file, if available
    /// Used by transform_upgrade() to apply appropriate upgrades
    util::Version version;

    /// When parsing a file via parse_string(), this references the string
    /// contents. Used to compute line information for error messages.
    std::string_view content;

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
 * representation. It handles:
 * - File includes via <include> tags
 * - Parameter substitution using the provided parameter list
 * - Basic structural validation
 * - Source location tracking for error reporting
 *
 * \param filename Path to the XML file to load
 * \param params List of parameter substitutions to apply
 * \return Parser state containing the scene graph
 */
extern MI_EXPORT_LIB ParserState parse_file(
    const fs::path &filename,
    const ParameterList &params = {}
);

/**
 * \brief Parse a scene from an XML string and return the resulting parser state
 *
 * Similar to parse_file() but takes the XML content as a string.
 * Useful for programmatically generated scenes or testing.
 *
 * \param string XML content to parse
 * \param params List of parameter substitutions to apply
 * \return Parser state containing the scene graph
 */
extern MI_EXPORT_LIB ParserState parse_string(
    std::string_view string,
    const ParameterList &params = {}
);

/**
 * \brief Upgrade scene data to the latest version
 *
 * This transformation updates older scene formats to be compatible
 * with the current version of Mitsuba. Current transformations include:
 *
 * - Converting property names from camelCase to underscore_case (version < 3.0)
 * - Updating deprecated plugin names to their modern equivalents
 * - Migrating old parameter structures to new formats
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
 */
extern MI_EXPORT_LIB void transform_colors(ParserState &ctx);

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
extern MI_EXPORT_LIB std::string file_location(const ParserState &ctx, const SceneNode &node);

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
extern MI_EXPORT_LIB ref<Object> instantiate(ParserState &ctx, bool parallel = true);

NAMESPACE_END(parser)
NAMESPACE_END(mitsuba)
