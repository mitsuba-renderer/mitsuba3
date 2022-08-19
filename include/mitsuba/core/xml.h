#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/properties.h>
#include <string>

/// Max level of nested <include> directives
#define MI_XML_INCLUDE_MAX_RECURSION 15

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(xml)

/// Used to pass key=value pairs to the parser
using ParameterList = std::vector<std::tuple<std::string, std::string, bool>>;

/**
 * Load a Mitsuba scene from an XML file
 *
 * \param path
 *     Filename of the scene XML file
 *
 * \param parameters
 *     Optional list of parameters that can be referenced as <tt>$varname</tt>
 *     in the scene.
 *
 * \param variant
 *     Specifies the variant of plugins to instantiate (e.g. "scalar_rgb")
 *
 * \param update_scene
 *     When Mitsuba updates scene to a newer version, should the
 *     updated XML file be written back to disk?
 */
extern MI_EXPORT_LIB std::vector<ref<Object>> load_file(
                                        const fs::path &path,
                                        const std::string &variant,
                                        ParameterList parameters = ParameterList(),
                                        bool update_scene = false,
                                        bool parallel = true);

/// Load a Mitsuba scene from an XML string
extern MI_EXPORT_LIB std::vector<ref<Object>> load_string(
                                        const std::string &string,
                                        const std::string &variant,
                                        ParameterList parameters = ParameterList(),
                                        bool parallel = true);



NAMESPACE_BEGIN(detail)
/// Create a Texture object from RGB values
extern MI_EXPORT_LIB ref<Object> create_texture_from_rgb(
                                        const std::string &name,
                                        Color<float, 3> color,
                                        const std::string &variant,
                                        bool within_emitter);

/// Create a Texture object from a constant value or spectral values if available
extern MI_EXPORT_LIB ref<Object> create_texture_from_spectrum(
                                        const std::string &name,
                                        Properties::Float const_value,
                                        std::vector<Properties::Float> &wavelengths,
                                        std::vector<Properties::Float> &values,
                                        const std::string &variant,
                                        bool within_emitter,
                                        bool is_spectral_mode,
                                        bool is_monochromatic_mode);

/// Expands a node (if it does not expand it is wrapped into a std::vector)
extern MI_EXPORT_LIB std::vector<ref<Object>> expand_node(
                                        const ref<Object> &top_node);

/// Read a Mitsuba XML file and return a list of pairs containing the
/// name of the plugin and the corresponding populated Properties object
extern MI_EXPORT_LIB std::vector<std::pair<std::string, Properties>> xml_to_properties(
                                        const fs::path &path,
                                        const std::string &variant);

NAMESPACE_END(detail)

NAMESPACE_END(xml)
NAMESPACE_END(mitsuba)
