#pragma once

#include <mitsuba/core/platform.h>
#include <mitsuba/core/variant.h>

#include <map>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>

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
    /// Construct with the default id and an empty plugin name
    Properties();
    /// Construct with the default id a specific plugin name
    Properties(const std::string &pluginName);
    /// Copy constructor
    Properties(const Properties &);

    /// Assignment operator
    void operator=(const Properties &props);

    /// Release all memory
    ~Properties();

    /// Verify if a value with the specified name exists
    bool hasProperty(const std::string &name) const;

    /** \brief Returns the type of an existing property.
     * If no property exists under that name, an error is logged
     * and type <tt>void</tt> is returned.
     */
    // TODO: is it bad design to "leak" std::type_info? (too low-level?)
    const std::type_info &getPropertyType(const std::string &name) const;

    /**
     * \brief Remove a property with the specified name
     * \return \c true upon success
     */
    bool removeProperty(const std::string &name);

    /// Manually mark a certain property as queried
    void markQueried(const std::string &name) const;

    /// Check if a certain property was queried
    bool wasQueried(const std::string &name) const;

    /// Get the associated plugin name
    const std::string &getPluginName() const;
    /// Set the associated plugin name
    void setPluginName(const std::string &name);

    /// Returns the associated identifier (or the string "unnamed")
    const std::string &getID() const;
    /// Set the associated identifier
    void setID(const std::string &id);

    /// Copy an attribute from another Properties object and potentially rename it
    void copyAttribute(const Properties &properties,
                       const std::string &sourceName, const std::string &targetName);

    /// Store an array containing the names of all stored properties
    void putPropertyNames(std::vector<std::string> &results) const;

    /// Return an array containing the names of all stored properties
    inline std::vector<std::string> getPropertyNames() const {
        std::vector<std::string> results;
        putPropertyNames(results);
        return results;
    }

    /// Return the list of un-queried attributed
    std::vector<std::string> getUnqueried() const;

    /**
     * Merge another properties record into the current one.
     * Existing properties will be overwritten with the values
     * from <tt>props</tt> if they have the same name.
     */
    void merge(const Properties &props);

    /// Equality comparison operator
    bool operator==(const Properties &props) const;

    /// Inequality comparison operator
    inline bool operator!=(const Properties &props) const {
        return !operator==(props);
    }

    MTS_EXPORT_CORE friend
    std::ostream &operator<<(std::ostream &os, const Properties &p);

public:  // Type-specific getters and setters ----------------------------------

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

private:
    struct PropertiesPrivate;
    std::unique_ptr<PropertiesPrivate> d;
};

NAMESPACE_END(mitsuba)
