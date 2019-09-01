#pragma once

#include <mitsuba/mitsuba.h>
#include <string>
#include <vector>

/// Max level of nested <include> directives
#define MTS_XML_INCLUDE_MAX_RECURSION 15

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(xml)

/// Used to pass key=value pairs to the parser
using ParameterList = std::vector<std::pair<std::string, std::string>>;

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
 * \param monochrome
 *     Instantiate a monochrome version of the scene?
 *
 * \param update_scene
 *     When Mitsuba updates scene to a newer version, should the
 *     updated XML file be written back to disk?
 */
extern MTS_EXPORT_CORE ref<Object> load_file(const fs::path &path,
                                             ParameterList parameters = ParameterList(),
                                             bool monochrome = false,
                                             bool update_scene = false);

/// Load a Mitsuba scene from an XML string
extern MTS_EXPORT_CORE ref<Object> load_string(const std::string &string,
                                               ParameterList parameters = ParameterList(),
                                               bool monochrome = false);

NAMESPACE_END(xml)
NAMESPACE_END(mitsuba)
