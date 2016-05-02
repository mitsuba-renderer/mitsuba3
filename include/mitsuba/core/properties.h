#pragma once

#include <mitsuba/core/platform.h>
#include <mitsuba/core/variant.h>
#include <string>
#include <map>
#include <memory>

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
    Properties(const std::string &pluginName);
    Properties(const Properties &);

    /// Assignment operator
    void operator=(const Properties &props);

    /// Release all memory
    ~Properties();

    /// Verify if a value with the specified name exists
    bool hasProperty(const std::string &name) const;

    /// Set a boolean value
    void setBoolean(const std::string &name, const bool &value, bool warnDuplicates = true);
    /// Retrieve a boolean value
    bool getBoolean(const std::string &name) const;
    /// Retrieve a boolean value (use default value if no entry exists)
    bool getBoolean(const std::string &name, const bool &defVal) const;

    /// Set an integer value
    void setLong(const std::string &name, const int64_t &value, bool warnDuplicates = true);
    /// Retrieve an integer value
    int64_t getLong(const std::string &name) const;
    /// Retrieve an integer value (use default value if no entry exists)
    int64_t getLong(const std::string &name, const int64_t &defVal) const;

    /// Set a single precision floating point value
    void setFloat(const std::string &name, const Float &value, bool warnDuplicates = true);
    /// Get a single precision floating point value
    Float getFloat(const std::string &name) const;
    /// Get a single precision floating point value (with default)
    Float getFloat(const std::string &name, const Float &defVal) const;

    /// Set a string
    void setString(const std::string &name, const std::string &value, bool warnDuplicates = true);
    /// Get a string
    std::string getString(const std::string &name) const;
    /// Get a string (with default)
    std::string getString(const std::string &name, const std::string &defVal) const;

    /// Return a string representation of all set properties
    std::string toString() const;

private:
    struct PropertiesPrivate;
    std::unique_ptr<PropertiesPrivate> d;
};

NAMESPACE_END(mitsuba)
