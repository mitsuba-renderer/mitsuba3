#pragma once

#include <mitsuba/mitsuba.h>
#include <memory>
#include <string>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/** \brief Associative parameter map for constructing
 * subclasses of \ref Object.
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
	/// Supported types of properties
	enum EPropertyType {
		/// Boolean value (true/false)
		EBoolean = 0,
		/// 64-bit signed integer
		EInteger,
		/// Floating point value
		EFloat,
		/// 3D point
		EPoint,
		/// 3D vector
		EVector3f,
		/// 4x4 transform for homogeneous coordinates
		ETransform,
		/// An animated 4x4 transformation
		EAnimatedTransform,
		/// Discretized color spectrum
		ESpectrum,
		/// Arbitrary-length string
		EString,
		/// Arbitrary object
		EObject
	};

	/// Construct an empty property container
    Properties();

	/// Construct an empty property container with a specific plugin name
    Properties(const std::string &pluginName);

    /// Copy constructor
    Properties(const Properties &props);

    /// Assignment operator
    void operator=(const Properties &props);

    /// Release all memory
    ~Properties();

    /// Get the associated plugin name
    const std::string &getPluginName() const;

    /// Set the associated plugin name
    void setPluginName(const std::string &name);

    /// Verify if a value with the specified name exists
    bool hasProperty(const std::string &name) const;

    /** \brief Returns the type of an existing property.
     * If no property exists under that name, an error is logged
     * and type <tt>void</tt> is returned.
     */
    EPropertyType getPropertyType(const std::string &name) const;

    /**
     * \brief Remove a property with the specified name
     * \return \c true upon success
     */
    bool removeProperty(const std::string &name);

    /**
     * \brief Manually mark a certain property as queried
     * \return \c true upon success
     */
    bool markQueried(const std::string &name) const;

    /// Check if a certain property was queried
    bool wasQueried(const std::string &name) const;

    /// Returns a unique identifier associated with this instance (or an empty string)
    const std::string &getID() const;

    /// Set the unique identifier associated with this instance
    void setID(const std::string &id);

    /// Copy an attribute from another Properties object and potentially rename it
    void copyAttribute(const Properties &properties,
                       const std::string &sourceName,
                       const std::string &targetName);

    /// Store an array containing the names of all stored properties
    std::vector<std::string> getPropertyNames() const;

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
    bool operator!=(const Properties &props) const {
        return !operator==(props);
    }

    MTS_EXPORT_CORE friend
    std::ostream &operator<<(std::ostream &os, const Properties &p);

public:  // Type-specific getters and setters ----------------------------------

    /// Store a boolean value in the Properties instance
    void setBoolean(const std::string &name, const bool &value, bool warnDuplicates = true);
    /// Retrieve a boolean value
    const bool& getBoolean(const std::string &name) const;
    /// Retrieve a boolean value (use default value if no entry exists)
    const bool& getBoolean(const std::string &name, const bool &defVal) const;

    /// Set an integer value in the Properties instance
    void setInteger(const std::string &name, const int &value, bool warnDuplicates = true) {
        setLong(name, (int64_t) value, warnDuplicates);
    }
    /// Retrieve an integer value
    int getInteger(const std::string &name) const {
        return (int) getLong(name);
    }
    /// Retrieve a boolean value (use default value if no entry exists)
	int getInteger(const std::string &name, const int &defVal) const {
        return (int) getLong(name, (int64_t) defVal);
	}

    /// Store an integer value in the Properties instance
    void setLong(const std::string &name, const int64_t &value, bool warnDuplicates = true);
    /// Retrieve an integer value
    const int64_t& getLong(const std::string &name) const;
    /// Retrieve an integer value (use default value if no entry exists)
    const int64_t& getLong(const std::string &name, const int64_t &defVal) const;

    /// Store a floating point value in the Properties instance
    void setFloat(const std::string &name, const Float &value, bool warnDuplicates = true);
    /// Retrieve a floating point value
    const Float& getFloat(const std::string &name) const;
    /// Retrieve a floating point value (use default value if no entry exists)
    const Float& getFloat(const std::string &name, const Float &defVal) const;

    /// Store a string in the Properties instance
    void setString(const std::string &name, const std::string &value, bool warnDuplicates = true);
    /// Retrieve a string value
    const std::string& getString(const std::string &name) const;
    /// Retrieve a string value (use default value if no entry exists)
    const std::string& getString(const std::string &name, const std::string &defVal) const;

    /// Store a vector in the Properties instance
    void setVector3f(const std::string &name, const Vector3f &value, bool warnDuplicates = true);
    /// Retrieve a vector
    const Vector3f& getVector3f(const std::string &name) const;
    /// Retrieve a vector (use default value if no entry exists)
    const Vector3f& getVector3f(const std::string &name, const Vector3f &defVal) const;

    /// Store an arbitrary object in the Properties instance
    void setObject(const std::string &name, const ref<Object> &value, bool warnDuplicates = true);
    /// Retrieve an arbitrary object
    const ref<Object>& getObject(const std::string &name) const;
    /// Retrieve an arbitrary object (use default value if no entry exists)
    const ref<Object>& getObject(const std::string &name, const ref<Object> &defVal) const;
private:
    struct PropertiesPrivate;
    std::unique_ptr<PropertiesPrivate> d;
};

NAMESPACE_END(mitsuba)
