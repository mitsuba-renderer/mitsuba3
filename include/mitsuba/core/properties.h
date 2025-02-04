#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

/** \brief Map data structure for passing parameters to scene objects.
 *
 * The constructors of Mitsuba scene objects generally take a Properties object
 * as input. It specifies named parameters of various types, such as filenames,
 * colors, etc. It can also pass nested objects that have already been
 * constructed.
 *
 * The C++ and Python APIs are slightly different. In C++, the type to be
 * retrieved must be specified as a template parameter:
 *
 * \code
 * Properties props("plugin_name")
 *
 * // Write to 'props':
 * props.set("color_value", mi::Color3f(0.1, 0.2, 0.3));
 *
 * // Read from 'props':
 * Color3f value = props.get<mi::Color3f>("color_value");
 * \endcode
 *
 * In Python, the API behaves like a standard dictionary.
 *
 * \code
 * props = mi.Properties("plugin_name")
 *
 * // Write to 'props':
 * props["color_value"] = mi.Color3f(0.1, 0.2, 0.3)
 *
 * // Read from 'props':
 * value = props["color_value"]
 * \endcode
 *
 * or using the ``get(key, default)`` method.
 */
class MI_EXPORT_LIB Properties {
public:
    // The properties object uses double precision storage internally so
    // that it can be used in all Mitsuba variants without loss of precision
    using Float = double;

    MI_IMPORT_CORE_TYPES()

    /// Enumeration of representable property types
    enum class Type {
        /// Boolean value (true/false)
        Bool,

        /// 64-bit signed integer
        Long,

        /// Floating point value
        Float,

        /// 3D array
        Array3f,

        /// A tensor of arbitrary shape
        Tensor,

        /// 3x3 transform for homogeneous coordinates
        Transform3f,

        /// 4x4 transform for homogeneous coordinates
        Transform4f,

        /// An animated 4x4 transformation
        AnimatedTransform,

        /// Tristimulus color value
        Color,

        /// String
        String,

        /// Indirect dependence to another object (referenced by name)
        NamedReference,

        /// An arbitrary Mitsuba scene object
        Object,

        /// const void* pointer (for internal communication between plugins)
        Pointer
    };

    /// Represents an indirect dependence to another object (referenced by name)
    struct NamedReference {
        NamedReference(std::string_view name) : m_name(name) { }
        operator const std::string&() const { return m_name; }
        bool operator==(const NamedReference &r) const { return r.m_name == m_name; }
        bool operator!=(const NamedReference &r) const { return r.m_name != m_name; }

    private:
        std::string m_name;
    };

    /// Construct an empty and unnamed properties object
    Properties();

    /// Free the properties object
    ~Properties();

    /// Construct an empty properties object with a specific plugin name
    Properties(std::string_view plugin_name);

    /// Copy constructor
    Properties(const Properties &props);

    /// Move constructor
    Properties(Properties &&props);

    /// Copy assignment operator
    Properties &operator=(const Properties &props);

    /// Move assignment operator
    Properties &operator=(Properties &&props);

    /// Get the plugin name
    std::string_view plugin_name() const;

    /// Set the plugin name
    void set_plugin_name(std::string_view name);

    /**
     * \brief Retrieve a parameter by name
     *
     * Look up the property ``name``. Raises an exception if the property cannot
     * be found, or when it has an incompatible type. Accessing the parameter
     * automatically marks it as queried (see \ref was_queried).
     *
     * The template parameter ``T`` may refer to:
     *
     * - A ``std::string``
     *
     * - A primitive type such as ``bool``, ``float``, ``double``, ``uint32_t``,
     * ``int32_t``, ``uint64_t``, ``int64_t``, ``size_t``.
     *
     * - A 2D ``mi::Point2f``/``mi::Point3f`` or
     * `mi::Vector2f``/``mi::Vector3f``.
     *
     * - A tri-stimulus``mi::Color3f``.
     *
     * - An affine transform, such as ``mi::Transform3f`` or ``mi::Transform4f``
     */
    template <typename T> T get(std::string_view name) const;

    /**
     * \brief Retrieve a parameter (with default)
     *
     * Look up the property ``name``. When the property does not exist, the
     * parameter ``def_value`` is used instead. The function raises an error
     * if the specified name maps to a parameter of an incompatible type.
     * Accessing the parameter automatically marks it as queried (see
     * \ref was_queried).
     */
    template <typename T> T get(std::string_view name, const T &def_val) const;

    /**
     * \brief Set a parameter value
     *
     * When a parameter with a matching names is already present, the method
     * raises an exception if \c error_duplicates is set to \c true (the
     * default). Otherwise, it replaces the parameter.
     *
     * The parameter is initially marked as unqueried (see \ref was_queried).
     */
    template <typename T>
    void set(std::string_view name, const T &value,
             bool error_duplicates = true);

    /// Verify if a property with the specified name exists
    bool has_property(std::string_view name) const;

    /** \brief Returns the type of an existing property.
     *
     * Raises an exception if the property does not exist.
     */
    Type type(std::string_view name) const;

    /**
     * \brief Remove a property with the specified name
     *
     * \return \c true upon success
     */
    bool remove_property(std::string_view name);

    /**
     * \brief Manually mark a certain property as queried
     *
     * \return \c true upon success
     */
    bool mark_queried(std::string_view name) const;

    /**
     * \brief Check if a certain property was queried
     *
     * Mitsuba assigns a queried bit with every parameter. Unqueried
     * parameters are detected to issue warnings, since this is usually
     * indicative of typos.
     */
    bool was_queried(std::string_view name) const;

    /**
     * \brief Returns a unique identifier associated with this instance (or an empty string)
     *
     * The ID is used to enable named references by other plugins
     */
    std::string_view id() const;

    /// Set the unique identifier associated with this instance
    void set_id(std::string_view id);

    /// Return an array containing the names of all stored properties
    std::vector<std::string> property_names() const;

    /**
     * \brief Return an array containing directly nested scene objects and
     * their associated names
     *
     * \param mark_queried
     *    Whether all stored objects should be marked as queried
     */
    std::vector<std::pair<std::string, ref<Object>>>
    objects(bool mark_queried = true) const;

    /**
     * \brief Return an array containing indirect references to scene
     * objects and their associated names
     *
     * \param mark_queried
     *    Whether all stored objects should be marked as queried
     */
    std::vector<std::pair<std::string, NamedReference>>
    named_references(bool mark_queried = true) const;

    /// Return the list of unqueried attributed
    std::vector<std::string> unqueried() const;

    /// Return one of the parameters (converting it to a string if necessary)
    std::string as_string(std::string_view name) const;

    /// Return one of the parameters (converting it to a string if necessary, with default value)
    std::string as_string(std::string_view name, std::string_view def_val) const;

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

private:
    struct PropertiesPrivate;
    std::unique_ptr<PropertiesPrivate> d;
};

#define MI_EXPORT_PROP(Mode, ...)                                              \
    Mode template MI_EXPORT_LIB __VA_ARGS__ Properties::get<__VA_ARGS__>(      \
        std::string_view) const;                                               \
    Mode template MI_EXPORT_LIB __VA_ARGS__ Properties::get<__VA_ARGS__>(      \
        std::string_view, const __VA_ARGS__ &) const;                          \
    Mode template MI_EXPORT_LIB void Properties::set<__VA_ARGS__>(             \
        std::string_view, const __VA_ARGS__ &, bool);

#if defined(__APPLE__)
#  define MI_EXPORT_PROP_SIZE_T(Mode) MI_EXPORT_PROP(Mode, size_t)
#else
#  define MI_EXPORT_PROP_SIZE_T(Mode)
#endif

#define MI_EXPORT_PROP_ALL(Mode)                                               \
    MI_EXPORT_PROP_SIZE_T(Mode)                                                \
    MI_EXPORT_PROP(Mode, bool)                                                 \
    MI_EXPORT_PROP(Mode, float)                                                \
    MI_EXPORT_PROP(Mode, double)                                               \
    MI_EXPORT_PROP(Mode, uint32_t)                                             \
    MI_EXPORT_PROP(Mode, int32_t)                                              \
    MI_EXPORT_PROP(Mode, uint64_t)                                             \
    MI_EXPORT_PROP(Mode, int64_t)                                              \
    MI_EXPORT_PROP(Mode, dr::Array<float, 3>)                                  \
    MI_EXPORT_PROP(Mode, dr::Array<double, 3>)                                 \
    MI_EXPORT_PROP(Mode, Point<float, 3>)                                      \
    MI_EXPORT_PROP(Mode, Point<double, 3>)                                     \
    MI_EXPORT_PROP(Mode, Vector<float, 3>)                                     \
    MI_EXPORT_PROP(Mode, Vector<double, 3>)                                    \
    MI_EXPORT_PROP(Mode, Color<float, 3>)                                      \
    MI_EXPORT_PROP(Mode, Color<double, 3>)                                     \
    MI_EXPORT_PROP(Mode, Transform<Point<float, 3>>)                           \
    MI_EXPORT_PROP(Mode, Transform<Point<double, 3>>)                          \
    MI_EXPORT_PROP(Mode, Transform<Point<float, 4>>)                           \
    MI_EXPORT_PROP(Mode, Transform<Point<double, 4>>)                          \
    MI_EXPORT_PROP(Mode, std::string)                                          \
    MI_EXPORT_PROP(Mode, ref<Object>)

MI_EXPORT_PROP_ALL(extern)

NAMESPACE_END(mitsuba)
