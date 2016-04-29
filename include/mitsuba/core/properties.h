#pragma once

#include <mitsuba/core/platform.h>
#include <string>
#include <map>

NAMESPACE_BEGIN(mitsuba)

/** \brief Associative parameter map for constructing
 * subclasses of \ref ConfigurableObject.
 *
 * Note that the Python bindings for this class do not implement
 * the various type-dependent getters and setters. Instead, they
 * are accessed just like a normal Python map, e.g:
 *
 * \code
 * myProps = mitsuba.core.Properties("pluginName")
 * myProps["stringProperty"] = "hello"
 * myProps["spectrumProperty"] = mitsuba.core.Spectrum(1.0)
 * \endcode
 */
class MTS_EXPORT_CORE Properties {
public:
    Properties();
    Properties(const Properties &);

	/// Verify if a value with the specified name exists
	bool hasProperty(const std::string &name) const;

	/// Set a boolean value
	void setBoolean(const std::string &name, const bool &value, bool warnDuplicates = true);
	/// Retrieve a boolean value
	bool getBoolean(const std::string &name) const;
	/// Retrieve a boolean value (use default value if no entry exists)
	bool getBoolean(const std::string &name, const bool &defVal) const;

private:
    struct PropertiesPrivate;
    std::unique_ptr<PropertiesPrivate> d;
};

NAMESPACE_END(mitsuba)
