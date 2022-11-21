#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/vector.h>

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
 * \code
 * myProps = mitsuba.core.Properties("plugin_name")
 * myProps["stringProperty"] = "hello"
 * myProps["spectrumProperty"] = mitsuba.core.Spectrum(1.0)
 * \endcode
 *
 * or using the ``get(key, default)`` method.
 */
class MI_EXPORT_LIB Properties {
public:
    /// Supported types of properties
    enum class Type {
        Bool,              /// Boolean value (true/false)
        Long,              /// 64-bit signed integer
        Float,             /// Floating point value
        Array3f,           /// 3D array
        Transform,         /// 4x4 transform for homogeneous coordinates
        AnimatedTransform, /// An animated 4x4 transformation
        Color,             /// Tristimulus color value
        String,            /// String
        NamedReference,    /// Named reference to another named object
        Object,            /// Arbitrary object
        Pointer            /// const void* pointer (for internal communication between plugins)
    };

    // Scene parsing in double precision
    using Float = double;
    using Array3f = dr::Array<Float, 3>;
    MI_IMPORT_CORE_TYPES()

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

    MI_EXPORT_LIB friend
    std::ostream &operator<<(std::ostream &os, const Properties &p);

public:  // Type-specific getters and setters ----------------------------------

    /// Generic getter method to retrieve properties. (throw an error if type isn't supported)
    template <typename T> T get(const std::string &name) const;
    /// Generic getter method to retrieve properties. (use default value if no entry exists)
    template <typename T> T get(const std::string &name, const T &def_val) const;

    /// Store a boolean value in the Properties instance
    void set_bool(const std::string &name, const bool &value, bool error_duplicates = true);

    /// Set an integer value in the Properties instance
    void set_int(const std::string &name, const int &value, bool error_duplicates = true) {
        set_long(name, (int64_t) value, error_duplicates);
    }

    /// Store a long value in the Properties instance
    void set_long(const std::string &name, const int64_t &value, bool error_duplicates = true);

    /// Store a floating point value in the Properties instance
    void set_float(const std::string &name, const Float &value, bool error_duplicates = true);

    /// Store a string in the Properties instance
    void set_string(const std::string &name, const std::string &value, bool error_duplicates = true);
    /// Retrieve a string value
    const std::string& string(const std::string &name) const;
    /// Retrieve a string value (use default value if no entry exists)
    const std::string& string(const std::string &name, const std::string &def_val) const;

    /// Store a named reference in the Properties instance
    void set_named_reference(const std::string &name, const NamedReference &value, bool error_duplicates = true);
    /// Retrieve a named reference value
    const NamedReference& named_reference(const std::string &name) const;
    /// Retrieve a named reference value (use default value if no entry exists)
    const NamedReference& named_reference(const std::string &name, const NamedReference &def_val) const;

    /// Store a 3D array in the Properties instance
    void set_array3f(const std::string &name, const Array3f &value, bool error_duplicates = true);

    /// Store a color in the Properties instance
    void set_color(const std::string &name, const Color3f &value, bool error_duplicates = true);

    /// Store a 4x4 homogeneous coordinate transformation in the Properties instance
    void set_transform(const std::string &name, const Transform4f &value, bool error_duplicates = true);

#if 0
    /// Store an animated transformation in the Properties instance
    void set_animated_transform(const std::string &name, ref<AnimatedTransform> value,
                                bool error_duplicates = true);
    /// Store a (constant) animated transformation in the Properties instance
    void set_animated_transform(const std::string &name, const Transform4f &value,
                                bool error_duplicates = true);
    /// Retrieve an animated transformation
    ref<AnimatedTransform> animated_transform(const std::string &name) const;
    /// Retrieve an animated transformation (use default value if no entry exists)
    ref<AnimatedTransform> animated_transform(
            const std::string &name, ref<AnimatedTransform> def_val) const;
    /// Retrieve an animated transformation (default value is a constant transform)
    ref<AnimatedTransform> animated_transform(const std::string &name,
                                              const Transform<Point4f> &def_val) const;
#endif

    /// Store an arbitrary object in the Properties instance
    void set_object(const std::string &name, const ref<Object> &value, bool error_duplicates = true);
    /// Retrieve an arbitrary object
    const ref<Object>& object(const std::string &name) const;
    /// Retrieve an arbitrary object (use default value if no entry exists)
    const ref<Object>& object(const std::string &name, const ref<Object> &def_val) const;

    /// Store an arbitrary pointer in the Properties instance
    void set_pointer(const std::string &name, const void * const &value, bool error_duplicates = true);
    /// Retrieve an arbitrary pointer
    const void * const& pointer(const std::string &name) const;
    /// Retrieve an arbitrary pointer (use default value if no entry exists)
    const void * const& pointer(const std::string &name, const void * const &def_val) const;

    /// Retrieve a texture (if the property is a float, create a uniform texture instead)
    template <typename Texture>
    ref<Texture> texture(const std::string &name) const {
        if (!has_property(name))
            Throw("Property \"%s\" has not been specified!", name);

        auto p_type = type(name);
        if (p_type == Properties::Type::Object) {
            ref<Object> object = find_object(name);
            if (!object->class_()->derives_from(MI_CLASS(Texture)))
                Throw("The property \"%s\" has the wrong type (expected "
                      " <spectrum> or <texture>).", name);
            mark_queried(name);
            return (Texture *) object.get();
        } else if (p_type == Properties::Type::Float) {
            Properties props("uniform");
            props.set_float("value", get<Float>(name));
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
    template <typename Texture, typename FloatType>
    ref<Texture> texture(const std::string &name, FloatType def_val) const {
        if (!has_property(name)) {
            Properties props("uniform");
            props.set_float("value", Float(def_val));
            return (Texture *) PluginManager::instance()->create_object<Texture>(props).get();
        }
        return texture<Texture>(name);
    }

    /// Retrieve a texture multiplied by D65 if necessary (if the property is a float, create a D65 texture instead)
    template <typename Texture>
    ref<Texture> texture_d65(const std::string &name) const {
        if (!has_property(name))
            Throw("Property \"%s\" has not been specified!", name);

        auto p_type = type(name);
        if (p_type == Properties::Type::Object) {
            ref<Object> object = find_object(name);
            if (!object->class_()->derives_from(MI_CLASS(Texture)))
                Throw("The property \"%s\" has the wrong type (expected "
                      " <spectrum> or <texture>).", name);
            mark_queried(name);
            return (Texture *) Texture::D65((Texture *) object.get()).get();
        } else if (p_type == Properties::Type::Float) {
            return (Texture *) Texture::D65((typename Texture::ScalarFloat) get<Float>(name)).get();
        } else {
            Throw("The property \"%s\" has the wrong type (expected "
                  " <spectrum> or <texture>).", name);
        }
    }

    /// Retrieve a texture multiplied by D65 if necessary (use the provided texture if no entry exists)
    template <typename Texture>
    ref<Texture> texture_d65(const std::string &name, ref<Texture> def_val) const {
        if (!has_property(name))
            return def_val;
        return texture_d65<Texture>(name);
    }

    /// Retrieve a texture multiplied by D65 if necessary (or create D65 texture with default value)
    template <typename Texture, typename FloatType>
    ref<Texture> texture_d65(const std::string &name, FloatType def_val) const {
        if (!has_property(name))
            return (Texture *) Texture::D65(def_val).get();
        return texture_d65<Texture>(name);
    }

    /// Retrieve a 3D texture
    template <typename Volume>
    ref<Volume> volume(const std::string &name) const {

        if (!has_property(name))
            Throw("Property \"%s\" has not been specified!", name);

        auto p_type = type(name);
        if (p_type == Properties::Type::Object) {
            ref<Object> object = find_object(name);
            if (!object->class_()->derives_from(MI_CLASS(Volume::Texture)) &&
                !object->class_()->derives_from(MI_CLASS(Volume)))
                Throw("The property \"%s\" has the wrong type (expected "
                    " <spectrum>, <texture>. or <volume>).", name);

            mark_queried(name);
            if (object->class_()->derives_from(MI_CLASS(Volume))) {
                return (Volume *) object.get();
            } else {
                Properties props("constvolume");
                props.set_object("value", object);
                return (Volume *) PluginManager::instance()->create_object<Volume>(props).get();
            }
        } else if (p_type == Properties::Type::Float) {
            Properties props("constvolume");
            props.set_float("value", get<Float>(name));
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

    template <typename Volume, typename FloatType>
    ref<Volume> volume(const std::string &name, FloatType def_val) const {
        if (!has_property(name)) {
            Properties props("constvolume");
            props.set_float("value", Float(def_val));
            return (Volume *) PluginManager::instance()->create_object<Volume>(props).get();
        }
        return volume<Volume>(name);
    }

private:
    /// Return a reference to an object for a specific name (return null ref if doesn't exist)
    ref<Object> find_object(const std::string &name) const;
    struct PropertiesPrivate;
    std::unique_ptr<PropertiesPrivate> d;
};

#define EXTERN_EXPORT_PROPERTY_ACCESSOR(T) \
    extern template MI_EXPORT_LIB T Properties::get<T>(const std::string &) const; \
    extern template MI_EXPORT_LIB T Properties::get<T>(const std::string &, const T&) const;

#define T(...) __VA_ARGS__
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(bool))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(float))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(double))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(uint32_t))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(int32_t))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(uint64_t))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(int64_t))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(dr::Array<float, 3>))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(dr::Array<double, 3>))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(Point<float, 3>))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(Point<double, 3>))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(Vector<float, 3>))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(Vector<double, 3>))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(Color<float, 3>))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(Color<double, 3>))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(Transform<Point<float, 4>>))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(Transform<Point<double, 4>>))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(std::string))
EXTERN_EXPORT_PROPERTY_ACCESSOR(T(ref<Object>))
#undef T
#undef EXTERN_EXPORT_PROPERTY_ACCESSOR

NAMESPACE_END(mitsuba)
