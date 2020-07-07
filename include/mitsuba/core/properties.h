#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/spectrum.h>
#include <memory>
#include <string>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/// Wrapper object used to represent named references to Object instances
class NamedReference {
public:
    NamedReference(const std::string &value) : m_value(value) { }
    operator const std::string&() const { return m_value; }
    bool operator==(const NamedReference &r) const { return r.m_value == m_value; }
    bool operator!=(const NamedReference &r) const { return r.m_value != m_value; }

private:
    std::string m_value;
};

/** \brief Associative parameter map for constructing
 * subclasses of \ref Object.
 *
 * Note that the Python bindings for this class do not implement
 * the various type-dependent getters and setters. Instead, they
 * are accessed just like a normal Python map, e.g:
 *
 * TODO update
 * \code
 * myProps = mitsuba.core.Properties("plugin_name")
 * myProps["stringProperty"] = "hello"
 * myProps["spectrumProperty"] = mitsuba.core.Spectrum(1.0)
 * \endcode
 */
class MTS_EXPORT_CORE Properties {
public:
    /// Supported types of properties
    enum class Type {
        Bool,              ///< Boolean value (true/false)
        Long,              ///< 64-bit signed integer
        Float,             ///< Floating point value
        Array3f,           ///< 3D array
        Transform,         ///< 4x4 transform for homogeneous coordinates
        AnimatedTransform, ///< An animated 4x4 transformation
        Color,             ///< Tristimulus color value
        String,            ///< String
        NamedReference,    ///< Named reference to another named object
        Object,            ///< Arbitrary object
        Pointer            ///< const void* pointer (for internal communication between plugins)
    };

    using Float = float;
    using Array3f = Array<Float, 3>;
    MTS_IMPORT_CORE_TYPES()

    /// Construct an empty property container
    Properties();

    /// Construct an empty property container with a specific plugin name
    Properties(const std::string &plugin_name);

    /// Copy constructor
    Properties(const Properties &props);

    /// Assignment operator
    void operator=(const Properties &props);

    /// Release all memory
    ~Properties();

    /// Get the associated plugin name
    const std::string &plugin_name() const;

    /// Set the associated plugin name
    void set_plugin_name(const std::string &name);

    /// Verify if a value with the specified name exists
    bool has_property(const std::string &name) const;

    /** \brief Returns the type of an existing property.
     * If no property exists under that name, an error is logged
     * and type <tt>void</tt> is returned.
     */
    Type type(const std::string &name) const;

    /**
     * \brief Remove a property with the specified name
     * \return \c true upon success
     */
    bool remove_property(const std::string &name);

    /**
     * \brief Manually mark a certain property as queried
     * \return \c true upon success
     */
    bool mark_queried(const std::string &name) const;

    /// Check if a certain property was queried
    bool was_queried(const std::string &name) const;

    /// Returns a unique identifier associated with this instance (or an empty string)
    const std::string &id() const;

    /// Set the unique identifier associated with this instance
    void set_id(const std::string &id);

    /// Copy a single attribute from another Properties object and potentially rename it
    void copy_attribute(const Properties &properties,
                        const std::string &source_name,
                        const std::string &targetName);

    /// Return an array containing the names of all stored properties
    std::vector<std::string> property_names() const;

    /// Return an array containing all named references and their destinations
    std::vector<std::pair<std::string, NamedReference>> named_references() const;

    /**
     * \brief Return an array containing names and references for all stored objects
     *
     * \param mark_queried
     *    Whether all stored objects should be marked as queried
     */
    std::vector<std::pair<std::string, ref<Object>>> objects(bool mark_queried = true) const;

    /// Return the list of un-queried attributed
    std::vector<std::string> unqueried() const;

    /// Return one of the parameters (converting it to a string if necessary)
    std::string as_string(const std::string &name) const;

    /// Return one of the parameters (converting it to a string if necessary, with default value)
    std::string as_string(const std::string &name, const std::string &def_val) const;

    /**
     * \brief Merge another properties record into the current one.
     *
     * Existing properties will be overwritten with the values from
     * <tt>props</tt> if they have the same name.
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
    void set_bool(const std::string &name, const bool &value, bool warn_duplicates = true);
    /// Retrieve a boolean value
    const bool& bool_(const std::string &name) const;
    /// Retrieve a boolean value (use default value if no entry exists)
    const bool& bool_(const std::string &name, const bool &def_val) const;

    /// Set an integer value in the Properties instance
    void set_int(const std::string &name, const int &value, bool warn_duplicates = true) {
        set_long(name, (int64_t) value, warn_duplicates);
    }

    /// Retrieve an integer value
    int int_(const std::string &name) const { return (int) long_(name); }
    /// Retrieve an integer value (use default value if no entry exists)
    int int_(const std::string &name, const int &def_val) const {
        return (int) long_(name, (int64_t) def_val);
    }

    /// Store a long value in the Properties instance
    void set_long(const std::string &name, const int64_t &value, bool warn_duplicates = true);
    /// Retrieve a long value
    const int64_t& long_(const std::string &name) const;
    /// Retrieve a long value (use default value if no entry exists)
    const int64_t& long_(const std::string &name, const int64_t &def_val) const;

    /// Retrieve a size_t value. Since the underlying storage has type int64_t,
    /// an exception is thrown if the value is negative.
    size_t size_(const std::string &name) const;
    /// Retrieve a long value (use default value if no entry exists). Since the
    /// underlying storage has type int64_t an exception is thrown if the value
    /// is negative).
    size_t size_(const std::string &name, const size_t &def_val) const;

    /// Store a floating point value in the Properties instance
    void set_float(const std::string &name, const Float &value, bool warn_duplicates = true);
    /// Retrieve a floating point value
    Float float_(const std::string &name) const;
    /// Retrieve a floating point value (use default value if no entry exists)
    Float float_(const std::string &name, const Float &def_val) const;

    /// Store a string in the Properties instance
    void set_string(const std::string &name, const std::string &value, bool warn_duplicates = true);
    /// Retrieve a string value
    const std::string& string(const std::string &name) const;
    /// Retrieve a string value (use default value if no entry exists)
    const std::string& string(const std::string &name, const std::string &def_val) const;

    /// Store a named reference in the Properties instance
    void set_named_reference(const std::string &name, const NamedReference &value, bool warn_duplicates = true);
    /// Retrieve a named reference value
    const NamedReference& named_reference(const std::string &name) const;
    /// Retrieve a named reference value (use default value if no entry exists)
    const NamedReference& named_reference(const std::string &name, const NamedReference &def_val) const;

    /// Store a 3D array in the Properties instance
    void set_array3f(const std::string &name, const Array3f &value, bool warn_duplicates = true);
    /// Retrieve a 3D array
    Array3f array3f(const std::string &name) const;
    /// Retrieve a 3D array (use default value if no entry exists)
    Array3f array3f(const std::string &name, const Array3f &def_val) const;

    /// Retrieve a 3D point
    Point3f point3f(const std::string &name) const { return array3f(name); }
    /// Retrieve a 3D point (use default value if no entry exists)
    Point3f point3f(const std::string &name, const Point3f &def_val) const { return array3f(name, def_val); }

    /// Retrieve a 3D vector
    Vector3f vector3f(const std::string &name) const { return array3f(name); }
    /// Retrieve a 3D vector (use default value if no entry exists)
    Vector3f vector3f(const std::string &name, const Vector3f &def_val) const { return array3f(name, def_val); }

    /// Store a 4x4 homogeneous coordinate transformation in the Properties instance
    void set_transform(const std::string &name, const Transform4f &value, bool warn_duplicates = true);
    /// Retrieve a 4x4 homogeneous coordinate transformation
    const Transform4f& transform(const std::string &name) const;
    /// Retrieve a 4x4 homogeneous coordinate transformation (use default value if no entry exists)
    const Transform4f& transform(const std::string &name, const Transform4f &def_val) const;

    /// Store an animated transformation in the Properties instance
    void set_animated_transform(const std::string &name, ref<AnimatedTransform> value,
                                bool warn_duplicates = true);
    /// Store a (constant) animated transformation in the Properties instance
    void set_animated_transform(const std::string &name, const Transform4f &value,
                                bool warn_duplicates = true);
    /// Retrieve an animated transformation
    ref<AnimatedTransform> animated_transform(const std::string &name) const;
    /// Retrieve an animated transformation (use default value if no entry exists)
    ref<AnimatedTransform> animated_transform(
            const std::string &name, ref<AnimatedTransform> def_val) const;
    /// Retrieve an animated transformation (default value is a constant transform)
    ref<AnimatedTransform> animated_transform(const std::string &name,
                                              const Transform<Point4f> &def_val) const;

    /// Store an arbitrary object in the Properties instance
    void set_object(const std::string &name, const ref<Object> &value, bool warn_duplicates = true);
    /// Retrieve an arbitrary object
    const ref<Object>& object(const std::string &name) const;
    /// Retrieve an arbitrary object (use default value if no entry exists)
    const ref<Object>& object(const std::string &name, const ref<Object> &def_val) const;

    /// Store an arbitrary pointer in the Properties instance
    void set_pointer(const std::string &name, const void * const &value, bool warn_duplicates = true);
    /// Retrieve an arbitrary pointer
    const void * const& pointer(const std::string &name) const;
    /// Retrieve an arbitrary pointer (use default value if no entry exists)
    const void * const& pointer(const std::string &name, const void * const &def_val) const;

    /// Store a color in the Properties instance
    void set_color(const std::string &name, const Color3f &value, bool warn_duplicates = true);
    /// Retrieve a color
    const Color3f& color(const std::string &name) const;
    /// Retrieve a color (use default value if no entry exists)
    const Color3f& color(const std::string &name, const Color3f &def_val) const;

    /// Retrieve a texture (if the property is a float, create a uniform texture instead)
    template <typename Texture>
    ref<Texture> texture(const std::string &name) const {
        if (!has_property(name))
            Throw("Property \"%s\" has not been specified!", name);

        auto p_type = type(name);
        if (p_type == Properties::Type::Object) {
            ref<Object> object = find_object(name);
            if (!object->class_()->derives_from(MTS_CLASS(Texture)))
                Throw("The property \"%s\" has the wrong type (expected "
                    " <spectrum> or <texture>).", name);
            mark_queried(name);
            return (Texture *) object.get();
        } else if (p_type == Properties::Type::Float) {
            Properties props("uniform");
            props.set_float("value", float_(name));
            return (Texture *) PluginManager::instance()->create_object<Texture>(props).get();
        } else {
            Throw("The property \"%s\" has the wrong type (expected "
                    " <spectrum> or <texture>).", name);
        }
    }

    /// Retrieve a texture (use the provided texture if no entry exists)
    template <typename Texture>
    ref<Texture> texture(const std::string &name, ref<Texture> def_val) const {
        if (!has_property(name))
            return def_val;
        return texture<Texture>(name);
    }

    /// Retrieve a texture (or create uniform texture with default value)
    template <typename Texture>
    ref<Texture> texture(const std::string &name, Float def_val) const {
        if (!has_property(name)) {
            Properties props("uniform");
            props.set_float("value", def_val);
            return (Texture *) PluginManager::instance()->create_object<Texture>(props).get();
        }
        return texture<Texture>(name);
    }

    /// Retrieve a 3D texture
    template <typename Volume>
    ref<Volume> volume(const std::string &name) const {

        if (!has_property(name))
            Throw("Property \"%s\" has not been specified!", name);

        auto p_type = type(name);
        if (p_type == Properties::Type::Object) {
            ref<Object> object = find_object(name);
            if (!object->class_()->derives_from(MTS_CLASS(Volume::Texture)) &&
                !object->class_()->derives_from(MTS_CLASS(Volume)))
                Throw("The property \"%s\" has the wrong type (expected "
                    " <spectrum>, <texture>. or <volume>).", name);

            mark_queried(name);
            if (object->class_()->derives_from(MTS_CLASS(Volume))) {
                return (Volume *) object.get();
            } else {
                Properties props("constvolume");
                props.set_object("color", object);
                return (Volume *) PluginManager::instance()->create_object<Volume>(props).get();
            }
        } else if (p_type == Properties::Type::Float) {
            Properties props("constvolume");
            props.set_float("color", float_(name));
            return (Volume *) PluginManager::instance()->create_object<Volume>(props).get();
        } else {
            Throw("The property \"%s\" has the wrong type (expected "
                    " <spectrum>, <texture> or <volume>).", name);
        }
    }

    /// Retrieve a 3D texture (use the provided texture if no entry exists)
    template <typename Volume>
    ref<Volume> volume(const std::string &name, ref<Volume> def_val) const {
        if (!has_property(name))
            return def_val;
        return volume<Volume>(name);
    }

    template <typename Volume>
    ref<Volume> volume(const std::string &name, Float def_val) const {
        if (!has_property(name)) {
            Properties props("constvolume");
            props.set_float("color", def_val);
            return (Volume *) PluginManager::instance()->create_object<Volume>(props).get();
        }
        return volume<Volume>(name);
    }


private:
    // Return a reference to an object for a specific name (return null ref if doesn't exist)
    ref<Object> find_object(const std::string &name) const;
    struct PropertiesPrivate;
    std::unique_ptr<PropertiesPrivate> d;
};

NAMESPACE_END(mitsuba)
