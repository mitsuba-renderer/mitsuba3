#pragma once

#include <any>
#include <mitsuba/mitsuba.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(detail)
// The Properties type projects various types to a common representation. The
// mapping is implemented by the `prop_map` partial template overload. See the
// bottom of this file for specifics.
template <typename T, typename = void> struct prop_map { using type = void; };
template <typename T> using prop_map_t = typename prop_map<T>::type;
NAMESPACE_END(detail)

/** \brief Map data structure for passing parameters to scene objects.
 *
 * The constructors of Mitsuba scene objects take a \ref Properties object as
 * input. It specifies named parameters of various types, such as filenames,
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
 * props.set("color_value", ScalarColor3f(0.1f, 0.2f, 0.3f));
 *
 * // Read from 'props':
 * Color3f value = props.get<ScalarColor3f>("color_value");
 * \endcode
 *
 * In Python, the API behaves like a standard dictionary.
 *
 * \code
 * props = mi.Properties("plugin_name")
 *
 * // Write to 'props':
 * props["color_value"] = mi.ScalarColor3f(0.1, 0.2, 0.3)
 *
 * // Read from 'props':
 * value = props["color_value"]
 * \endcode
 *
 * or using the ``get(key, default)`` method.
 */
class MI_EXPORT_LIB Properties {
public:
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

        /// 4x4 homogeneous coordinate transform
        Transform4f,

        /// Tristimulus color value
        Color,

        /// String
        String,

        /// Indirect dependence to another object (referenced by name)
        Reference,

        /// An arbitrary Mitsuba scene object
        Object,

        /// Generic type wrapper for arbitrary data exchange between plugins
        Any
    };

    /// Represents an indirect dependence on another object (referenced by name)
    struct Reference {
        Reference(std::string_view name) : m_name(name) { }
        operator const std::string&() const { return m_name; }
        bool operator==(const Reference &r) const { return r.m_name == m_name; }
        bool operator!=(const Reference &r) const { return r.m_name != m_name; }

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
     * \brief Retrieve a scalar parameter by name
     *
     * Look up the property ``name``. Raises an exception if the property cannot
     * be found, or when it has an incompatible type. Accessing the parameter
     * automatically marks it as queried (see \ref was_queried).
     *
     * The template parameter ``T`` may refer to:
     *
     * - Strings (``std::string``)
     *
     * - Arithmetic types (``bool``, ``float``, ``double``, ``uint32_t``,
     *   ``int32_t``, ``uint64_t``, ``int64_t``, ``size_t``).
     *
     * - Points/vectors (``ScalarPoint2f``, ``ScalarPoint3f``,
     *   `ScalarVector2f``, or ``ScalarVector3f``).
     *
     * - Tri-stimulus color values (``ScalarColor3f``).
     *
     * - Affine transformations (``ScalarTransform3f``, ``ScalarTransform4f``)
     *
     * Both single/double precision versions of these are supported; the
     * function will convert them as needed. The function cannot be used to
     * obtain vectorized (e.g. JIT-compiled) arrays.
     */
    template <typename T> T get(std::string_view name) const {
        using T2 = detail::prop_map_t<std::decay_t<T>>;
        const T2 &value = *get_impl<T2>(name, true);

        static_assert(
            std::is_same_v<T2, void>,
            "Properties::get<T>(): The requested type is not suppported by the "
            "Properties class. Arbitrary data blobs may alternatively be "
            "passed using Properties::get_any()");

        // Perform a range check just in case
        if constexpr (std::is_integral_v<T2> && !std::is_same_v<T2, bool>) {
            constexpr T2 min = (T2) std::numeric_limits<T>::min(),
                         max = (T2) std::numeric_limits<T>::max();

            if (value < min || value > max)
                Throw("Property \"%s\": value %lld is out of bounds, must be "
                      "in the range [%lld, %lld]", name, value, min, max);
        }

        if constexpr (std::is_same_v<T, ref<Object>>) {
            using T3 = typename T::Type;
            T3 *ptr = dynamic_cast<T3*>(value.get());
            if (!ptr)
                Throw("Property \"%s\": has an incompatible object type!", name);
            return T(*ptr);
        } else {
            return static_cast<T>(value);
        }
    }

    /**
     * \brief Retrieve a parameter (with default value)
     *
     * Please see the \ref get() function above for details. The main difference
     * of this overload is that it automatically substitutes a default value
     * ``def_val`` when the requested parameter cannot be found.
     * It function raises an error if current parameter value has an
     * incompatible type.
     */
    template <typename T> T get(std::string_view name, const T &def_val) const {
        using T2 = detail::prop_map_t<std::decay_t<T>>;
        const T2 *value_p = get_impl<T2>(name, false);
        if (!value_p)
            return def_val;

        const T2 &value = *value_p;
        static_assert(
            std::is_same_v<T2, void>,
            "Properties::get<T>(): The requested type is not suppported by the "
            "Properties class. Arbitrary data blobs may alternatively be "
            "passed using Properties::get_any()");

        // Perform a range check just in case
        if constexpr (std::is_integral_v<T2> && !std::is_same_v<T2, bool>) {
            constexpr T2 min = (T2) std::numeric_limits<T>::min(),
                         max = (T2) std::numeric_limits<T>::max();

            if (value < min || value > max)
                Throw("Property \"%s\": value %lld is out of bounds, must be "
                      "in the range [%lld, %lld]", name, value, min, max);
        }

        if constexpr (std::is_same_v<T, ref<Object>>) {
            using T3 = typename T::Type;
            T3 *ptr = dynamic_cast<T3*>(value.get());
            if (!ptr)
                Throw("Property \"%s\": has an incompatible object type!", name);
            return T(*ptr);
        } else {
            return static_cast<T>(value);
        }
    }

    /**
     * \brief Set a parameter value
     *
     * When a parameter with a matching names is already present, the method
     * raises an exception if \c raise_if_exists is set to \c true (the
     * default). Otherwise, it replaces the parameter.
     *
     * The parameter is initially marked as unqueried (see \ref was_queried).
     */
    template <typename T>
    void set(std::string_view name, T &&value,
             bool raise_if_exists = true) {
        using T2 = detail::prop_map_t<std::decay_t<T>>;

        static_assert(
            std::is_same_v<T2, void>,
            "Properties::set<T>(): The requested type is not suppported by the "
            "Properties class. Arbitrary data blobs may alternatively be "
            "passed using Properties::set(std::make_any(value))");

        set_impl<T2>(name, static_cast<T2&&>(std::forward<T>(value)), raise_if_exists);
    }

    /**
     * \brief Retrieve a texture parameter (dummy implementation)
     *
     * TODO: This is a placeholder that will be implemented later.
     * For now, it just returns nullptr to allow compilation.
     */

    /// CLAUDE: rename this API to get_texture
    template <typename TextureType>
    ref<TextureType> texture(std::string_view name) const;

    /**
     * \brief Retrieve a texture parameter with float default
     *
     * When the texture parameter doesn't exist, creates a uniform texture
     * with the specified floating point value.
     */

    /// CLAUDE: rename this API to get_texture
    template <typename TextureType>
    ref<TextureType> texture(std::string_view name, float def_val) const;

    /**
     * \brief Retrieve a texture parameter with float default value
     *
     * When the texture parameter doesn't exist, creates a uniform D65
     * whitepoint texture scaled by the specified floatint point value.
     */

    /// CLAUDE: rename this API to get_texture_d65
    template <typename TextureType>
    ref<TextureType> texture_d65(std::string_view name, float def_val) const;

    /**
     * \brief Retrieve an arbitrarily typed value for inter-plugin communication
     *
     * The function raises an exception if the parameter does not exist or
     * cannot be cast. Accessing the parameter automatically marks it as
     * queried.
     */
    template <typename T> const T& get_any(std::string_view name) const {
        const T *value = std::any_cast<T *>(*get_impl<std::any>(name, true));
        if (!value)
            Throw("Property \"%s\" cannot be cast to the requested type!", name);
        return *value;
    }

    // CLAUDE: add comment
    template <typename T> void set_any(std::string_view name, const T &value) {
        set(name, std::make_any(value));
    }

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
    std::vector<std::pair<std::string, Reference>>
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
protected:
    /// Implementation detail of get()
    template <typename T> T* get_impl(std::string_view name, bool raise_if_missing) const;

    /// Implementation detail of set()
    template <typename T> void set_impl(std::string_view name, T &&value, bool raise_if_exists) const;

private:
    struct PropertiesPrivate;
    std::unique_ptr<PropertiesPrivate> d;
};

#define MI_EXPORT_PROP(Mode, ...)                                              \
    Mode template MI_EXPORT_LIB __VA_ARGS__ Properties::get_impl<__VA_ARGS__>( \
        std::string_view, bool) const;                                         \
    Mode template MI_EXPORT_LIB void Properties::set_impl<__VA_ARGS__>(        \
        std::string_view, const __VA_ARGS__ &, bool);                          \
    Mode template MI_EXPORT_LIB void Properties::set_impl<__VA_ARGS__>(        \
        std::string_view, __VA_ARGS__ &&, bool);                               \

#define MI_EXPORT_PROP_ALL(Mode)                                               \
    MI_EXPORT_PROP(double)                                                     \
    MI_EXPORT_PROP(int64_t)                                                    \
    MI_EXPORT_PROP(dr::Array<double, 3>)                                       \
    MI_EXPORT_PROP(Color<double, 3>)                                           \
    MI_EXPORT_PROP(Transform<Point<double, 4>>)                                \
    MI_EXPORT_PROP(std::string)                                                \
    MI_EXPORT_PROP(ref<Object>)                                                \
    MI_EXPORT_PROP(typename Properties::Reference)                             \
    MI_EXPORT_PROP(std::any)

MI_EXPORT_PROP_ALL(extern)

NAMESPACE_BEGIN(detail)
template <typename T>
struct prop_map<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    using type = std::conditional_t<
        std::is_floating_point_v<T>, double,
        std::conditional_t<std::is_same_v<T, bool>, bool, int64_t>>;
};
template <typename T, size_t N> struct prop_map<Vector<T, N>> { using type = dr::Array<double, N>; };
template <typename T, size_t N> struct prop_map<Point<T, N>> { using type = dr::Array<double, N>; };
template <typename T, size_t N> struct prop_map<dr::Array<T, N>> { using type = dr::Array<double, N>; };
template <typename T> struct prop_map<Color<T, 3>> { using type = Color<double, 3>; };
template <typename T, size_t N> struct prop_map<Transform<Point<T, N>>> { using type = Transform<Point<double, 4>>; };
template <typename T> struct prop_map<ref<T>> { using type = ref<Object>; };
template <typename T> struct prop_map<dr::Tensor<T>> { using type = std::any; };
template <> struct prop_map<const char *> { using type = std::string; };
template <> struct prop_map<char *> { using type = std::string; };
NAMESPACE_END(detail)
NAMESPACE_END(mitsuba)
