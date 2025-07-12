#pragma once

#include <mitsuba/core/properties.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/filesystem.h>
#include <tsl/robin_map.h>

/**
 * \brief Parser infrastructure for Mitsuba scene loading
 *
 * This namespace implements a unified parser for loading scenes from XML files
 * or Python dictionaries. The parsing is split into 3 stages:
 *
 * 1. Parsing: \ref parse_file(), \ref parse_string(), \ref parse_dict() (the
 *    latter is in ``src/core/python/parser.cpp``). These functions are
 *    variant-independent and
 *    - convert XML/dict input into an intermediate representation (SceneNode).
 *    - handle parameter substitution and file includes.
 *    - validate structure and capture metadata.
 *
 * 2. Transformations: \ref transform_upgrade(), \ref
 *    transform_resolve(), \ref transform_merge_equivalent(),
 *    \ref transform_merge_meshes()
 *
 *    - ``upgrade``: adapts old scene formats to the latest version
 *      (variant-independent).
 *
 *    - ``resolve_reference``: converts named references to node ID-based
 *      references (variant-independent).
 *
 *    - ``merge_equivalent``, ``merge_meshes``: merge equivalent/compatible
 *      plugin instantiations.
 *
 *    The convenience function \ref transform_all() applies the standard
 *    transformation pipeline in the correct order.
 *
 * 3. Instantiation: \ref instantiate(). This variant-specific function
 *    - creates actual Mitsuba objects from the intermediate representation.
 *    - handles object references and dependencies.
 *    - supports parallel instantiation for performance.
 *
 * The following additional functionality exists:
 *
 * 4. XML Export: \ref write_file(), \ref write_string() - convert the
 *    intermediate representation back to XML format for debugging, format
 *    conversion, or saving programmatically generated scenes.
 *
 * 5. Utility transformations: \ref transform_reorder(), \ref
 *    transform_relocate() - optional transformations for improving XML
 *    readability and organizing scene assets.
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
    /// How to handle unused "$key" -> "value" substitutions during parsing:
    /// Error (default), Warn, or Debug
    LogLevel unused_parameters = LogLevel::Error;

    /// How to handle unused properties during instantiation: Error (default),
    /// Warn, or Debug
    LogLevel unused_properties = LogLevel::Error;

    /// Maximum include depth to prevent infinite recursion
    int max_include_depth = 15;

    /// Target variant for instantiation (e.g., "scalar_rgb", "cuda_spectral")
    std::string variant;

    /// Enable parallel instantiation for better performance
    bool parallel = true;

    /// Enable merging of identical plugin instances (e.g., materials)
    bool merge_equivalent = true;

    /// Merge compatible meshes (same material) into single larger mesh
    bool merge_meshes = true;

    /// Constructor that takes variant name
    ParserConfig(std::string_view variant) : variant(variant) {}
};

/**
 * \brief Intermediate scene object representation
 *
 * This class stores information needed to instantiate a Mitsuba object at some
 * future point in time, including its plugin name and any parameters to be
 * supplied. The \ref parse_string and \ref parse_file functions below turn an
 * XML file or string into a sequence of \ref SceneNode instances that may
 * undergo further transformation before finally being instantiated.
 */
struct MI_EXPORT_LIB SceneNode {
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
    /// References are also kept here. They are initially unresolved and
    /// later resolved into concrete indices into ``ParserState.nodes``.
    Properties props;

    /// Equality comparison for SceneNode
    bool operator==(const SceneNode &other) const;

    bool operator!=(const SceneNode &other) const {
        return !operator==(other);
    }
};

/// Keeps track of common state while parsing an XML file or dictionary
struct MI_EXPORT_LIB ParserState {
    /// The list of all scene nodes. The root node is at position 0.
    /// Nodes are added during parsing and never removed, only modified
    std::vector<SceneNode> nodes;

    /// Node paths (e.g., "scene.myshape.mybsdf") parallel to ``nodes``.
    /// Only used in the dictionary parser, specifically for error reporting.
    std::vector<std::string> node_paths;

    /// List of files that were parsed while loading for error reporting.
    /// Indexed by \ref SceneNode.file_index, unused in the dictionary parser
    std::vector<fs::path> files;

    /// Version number of each parsed file in \ref files. Used by
    /// transform_upgrade() to apply appropriate upgrades per file
    std::vector<util::Version> versions;

    /// Maps named nodes with an ``id`` attribute to their index in ``nodes``
    /// Allows efficient lookup of objects for reference resolution
    tsl::robin_map<std::string, size_t, std::hash<std::string_view>,
                   std::equal_to<>> id_to_index;

    /// When parsing a file via parse_string(), this references the string
    /// contents. Used to compute line information for error messages.
    std::string_view content;

    /// Current include depth (for preventing infinite recursion)
    int depth = 0;

    /// Convenience operator for accessing nodes (const version)
    const SceneNode &operator[](size_t index) const {
        #if !defined(NDEBUG)
            return nodes.at(index);
        #else
            return nodes[index];
        #endif
    }

    /// Convenience operator for accessing nodes
    SceneNode &operator[](size_t index) {
        #if !defined(NDEBUG)
            return nodes.at(index);
        #else
            return nodes[index];
        #endif
    }

    /// Return the root node
    SceneNode &root() { return operator[](0); };
    const SceneNode &root() const { return operator[](0); };
    size_t size() const { return nodes.size(); }
    bool empty() const { return nodes.empty(); }

    /// Equality comparison for ParserState
    bool operator==(const ParserState &other) const;
    bool operator!=(const ParserState &other) const {
        return !operator==(other);
    }
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
 * This function is variant-independent.
 *
 * \param config Parser configuration options
 * \param filename Path to the XML file to load
 * \param params List of parameter substitutions to apply
 * \return Parser state containing the scene graph
 */
extern MI_EXPORT_LIB ParserState parse_file(
    const ParserConfig &config,
    const fs::path &filename,
    const ParameterList &params = {}
);

/**
 * \brief Parse a scene from an XML string and return the resulting parser state
 *
 * Similar to parse_file() but takes the XML content as a string. This function
 * is variant-independent.
 *
 * \param config Parser configuration options
 * \param string XML content to parse
 * \param params List of parameter substitutions to apply
 * \return Parser state containing the scene graph
 */
extern MI_EXPORT_LIB ParserState parse_string(
    const ParserConfig &config,
    std::string_view string,
    const ParameterList &params = {}
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
 * \param config Parser configuration
 * \param state Parser state to modify in-place
 */
extern MI_EXPORT_LIB void transform_upgrade(const ParserConfig &config,
                                            ParserState &state);

/**
 * \brief Resolve named references and raise an error when detecting broken
 * links
 *
 * This transformation converts all (named) \ref Properties::Reference objects
 * into index-based \ref Properties::ResolvedReference references.
 *
 * This transformation is variant-independent.
 *
 * \param config Parser configuration
 * \param state Parser state to modify in-place
 * \throws std::runtime_error if a reference cannot be resolved
 */
extern MI_EXPORT_LIB void transform_resolve(const ParserConfig &config,
                                            ParserState &state);

/**
 * \brief Merge equivalent nodes to reduce memory usage and instantiation time
 *
 * This transformation identifies nodes with identical properties and merges
 * them. All references to duplicate nodes are updated to point to a single
 * canonical instance.
 *
 * This optimization is particularly effective for scenes with many repeated
 * elements (e.g., identical materials or textures referenced by multiple
 * shapes).
 *
 * Note: Nodes containing Object or Any properties are never deduplicated as
 * their equality cannot be reliably determined. Additionally, emitter and shape
 * nodes are excluded from merging to preserve their distinct identities.
 *
 * \param config Parser configuration
 * \param state Parser state to optimize (modified in-place)
 */
extern MI_EXPORT_LIB void transform_merge_equivalent(const ParserConfig &config,
                                                     ParserState &state);

/**
 * \brief Adapt the scene description to merge geometry whenever possible
 *
 * This transformation moves all top-level geometry (i.e., occurring directly
 * within the <scene>) into a a shape plugin of type ``merge``.
 *
 * When instantiated, this ``merge`` shape
 * - Collects compatible groups of mesh objects (i.e., with identical BSDF,
 *   media, emitter, etc.)
 * - Merges them into single mesh instances to reduce memory usage
 * - Preserves non-mesh shapes and meshes with unique attributes
 *
 * \param config Parser configuration (currently unused)
 * \param state Parser state to modify in-place
 */
extern MI_EXPORT_LIB void transform_merge_meshes(const ParserConfig &config,
                                                 ParserState &state);

/**
 * \brief Reorder immediate children of scene nodes for better readability
 *
 * This transformation reorders the immediate children of scene nodes to follow
 * a logical grouping that improves XML readability. The ordering is:
 *
 * 1. Defaults
 * 2. Integrators
 * 3. Sensors
 * 4. Materials (BSDFs, textures, spectra)
 * 5. Emitters (including shapes with area lights)
 * 6. Shapes
 * 7. Media/volumes
 * 8. Other elements
 *
 * Shapes containing area lights are categorized as emitters rather than shapes,
 * keeping light sources grouped together.
 *
 * This transformation only affects the ordering of immediate children of the
 * scene node. It does not recurse into nested structures.
 *
 * Note: This transformation is not included in \ref transform_all() and must
 * be called explicitly if desired.
 *
 * \param config Parser configuration (currently unused)
 * \param state Parser state containing the scene to reorder (modified in-place)
 */
extern MI_EXPORT_LIB void transform_reorder(const ParserConfig &config,
                                            ParserState &state);

/**
 * \brief Relocate scene files to subfolders
 *
 * This transformation identifies file paths in the scene description and
 * relocates them to organized subfolders within the output directory,
 * creating subdirectories as needed.
 *
 * File organization:
 * - Textures and emitter files → textures/ subfolder
 * - Shape files (meshes) → meshes/ subfolder
 * - Spectrum files → spectra/ subfolder
 * - Other files → assets/ subfolder
 *
 * Note: This transformation is not included in \ref transform_all() and must
 * be called explicitly if desired, typically before XML export.
 *
 * \param config Parser configuration (currently unused)
 * \param state Parser state containing the scene (modified in-place)
 * \param output_directory Base directory where files should be relocated
 */
extern MI_EXPORT_LIB void transform_relocate(const ParserConfig &config,
                                             ParserState &state,
                                             const fs::path &output_directory);

/**
 * \brief Apply all transformations in sequence
 *
 * This convenience function applies all parser transformations to the scene
 * graph in the following order:
 * 1. \ref transform_upgrade()
 * 2. \ref transform_resolve()
 * 3. \ref transform_merge_equivalent() (if config.merge_equivalent is enabled)
 * 4. \ref transform_merge_meshes() (if config.merge_meshes is enabled)
 *
 * \param config Parser configuration containing variant and other settings
 * \param state Parser state to transform (modified in-place)
 */
extern MI_EXPORT_LIB void transform_all(const ParserConfig &config,
                                        ParserState &state);

/**
 * \brief Generate a human-readable file location string for error reporting
 *
 * Returns a string in the format "filename.xml:line:col" associated with a
 * given \ref SceneNode. In the case of the dictionary parser, it returns a
 * period-separated string identifying the path to the object.
 *
 * \param state Parser state containing file information
 * \param node Scene node to locate
 * \return Human-readable location string
 */
extern MI_EXPORT_LIB std::string file_location(const ParserState &state,
                                               const SceneNode &node);

/**
 * \brief Instantiate the parsed representation into concrete Mitsuba objects
 *
 * This final stage creates the actual scene objects from the intermediate
 * representation. It handles:
 *
 * - Plugin instantiation via the PluginManager
 * - Dependency ordering for correct instantiation order
 * - Parallel instantiation of independent objects (if enabled via
 *   ``config.parallel``)
 * - Property validation and type checking
 * - Object expansion (\ref Object::expand())
 *
 * This function creates plugins with variant \ref ParserConfig::variant, using
 * parallelism if requested (\ref ParserConfig::parallel). It will usually
 * return a single object but may also produce multiple return values if the
 * top-level object expands into sub-objects.
 *
 * \param config Parser configuration options including target variant and
 *        parallel flag
 *
 * \param state Parser state containing the scene graph
 *
 * \return Expanded top-level object
 */
extern MI_EXPORT_LIB std::vector<ref<Object>> instantiate(const ParserConfig &config,
                                                          ParserState &state);

/**
 * \brief Write scene data back to XML file
 *
 * This function converts the intermediate representation into an XML format
 * and writes it to disk. Useful for:
 * - Converting between scene formats (dict to XML)
 * - Saving programmatically generated scenes
 * - Debugging the parser's intermediate representation
 *
 * \param state Parser state containing the scene graph
 * \param filename Path where the XML file should be written
 * \param add_section_headers Whether to add XML comment headers that group scene
 *        elements by category (e.g., "Materials", "Emitters", "Shapes"). These
 *        section headers improve readability and are particularly useful when the
 *        scene has been reorganized using the \ref transform_reorder() function,
 *        which groups related elements together
 */
extern MI_EXPORT_LIB void write_file(const ParserState &state,
                                     const fs::path &filename,
                                     bool add_section_headers = false);

/**
 * \brief Convert scene data to an XML string
 *
 * Similar to write_file() but returns the XML content as a string
 * instead of writing to disk.
 *
 * \param state Parser state containing the scene graph
 * \param add_section_headers Whether to add section header comments for organization
 * \return XML representation of the scene
 */
extern MI_EXPORT_LIB std::string write_string(const ParserState &state,
                                              bool add_section_headers = false);

NAMESPACE_END(parser)
NAMESPACE_END(mitsuba)
