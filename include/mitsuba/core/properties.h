#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/any.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(detail)
// The Properties type projects various types to a common representation. The
// mapping is implemented by the `prop_map` partial template overload. See the
// bottom of this file for specifics.
template <typename T, typename = void> struct prop_map { using type = void; };
template <typename T> using prop_map_t = typename prop_map<std::decay_t<T>>::type;
template <typename T> struct base_type;
template <typename T> struct is_transform_3: std::false_type { };
template <typename T> struct is_transform_3<AffineTransform<Point<T, 3>>> : std::true_type { };
NAMESPACE_END(detail)

/** \brief Associative container for passing configuration parameters to Mitsuba
 * plugins.
 *
 * When Mitsuba scene objects (BSDFs, textures, emitters, etc.) are
 * instantiated, they receive their configuration through a Properties object.
 * This container maps parameter names to values of various types: booleans,
 * integers, floats, strings, colors, transforms, and references to other scene
 * objects.
 *
 * The Properties class combines the convenience of a dictionary with
 * type safety and provides several key features:
 *
 * - Fast O(1) insertion and lookup by property name.
 * - Traversal of properties in the original insertion order.
 * - Automatic tracking of queried properties.
 * - Reference properties that can be used to build object hierarchies.
 *
 * ## Basic C++ Usage
 *
 * \code
 * Properties props("plugin_name");
 *
 * // Write to 'props':
 * props.set("color_value", ScalarColor3f(0.1f, 0.2f, 0.3f));
 * props.set("my_bsdf", bsdf); // ref<BSDF> or BSDF*
 *
 * // Read from 'props':
 * ScalarColor3f value = props.get<ScalarColor3f>("color_value");
 * BSDF *bsdf = props.get<BSDF*>("my_bsdf");
 * \endcode
 *
 * ## Iterating Over Properties
 *
 * \code
 * // Iterate over all properties
 * for (const auto &prop : props) {
 *     std::cout << prop.name() << " = " << prop.type() << std::endl;
 * }
 *
 * // Iterate only over object properties
 * for (const auto &prop : props.objects()) {
 *     if (BSDF *bsdf = prop.try_get<BSDF>()) {
 *         // Process BSDF object
 *     } else if (Texture *texture = prop.try_get<Texture>()) {
 *         // Process Texture object
 *     }
 * }
 * \endcode
 *
 * ## Iterator stability
 *
 * It is legal to mutate the container (e.g., by adding/removing elements) while
 * iterating over its elements.
 *
 * ## Python API
 *
 * In Python, Property instances implement a dictionary-like interface:
 *
 * \code
 * props = mi.Properties("plugin_name")
 *
 * # Write to 'props':
 * props["color_value"] = mi.ScalarColor3f(0.1, 0.2, 0.3)
 *
 * for k, v in props.items():
 *    print(f'{k} = {v}')
 * \endcode
 *
 * ## Query Tracking
 *
 * Each property stores a flag that tracks whether it has been accessed. This
 * helps detect configuration errors such as typos in parameter names or unused
 * parameters. The \ref get() function automatically marks parameters as
 * queried.
 *
 * Use the following methods to work with query tracking:
 * - was_queried(name): Check if a specific parameter was accessed
 * - unqueried(): Get a list of all parameters that were never accessed
 * - mark_queried(name): Manually mark a parameter as accessed
 *
 * This is particularly useful during plugin initialization to warn users about
 * potentially misspelled or unnecessary parameters in their scene descriptions.
 *
 * ## Caveats
 *
 * Deleting parameters leaves unused entries ("tombstones") that reduce
 * memory efficiency especially following a large number of deletions.
 */
class MI_EXPORT_LIB Properties {
public:
    /// Enumeration of representable property types
    enum class Type {
        /* WARNING: The order of entries in this enum must EXACTLY match the order
           of types in the Variant definition in properties.cpp. This allows us to
           use variant::index() for efficient type determination.

           You must update 'property_type_names' in 'properties.cpp' when making
           any changes to this enumeration. */

        /// Unknown/deleted property (used for tombstones)
        Unknown,

        /// Boolean value (true/false)
        Bool,

        /// 64-bit signed integer
        Integer,

        /// Floating point value
        Float,

        /// String
        String,

        /// 3D array
        Vector,

        /// Tristimulus color value
        Color,

        /// Spectrum data (uniform value or wavelength-value pairs)
        Spectrum,

        /// 3x3 or 4x4 homogeneous coordinate transform
        Transform,

        /// Indirect reference to another scene object (by name)
        Reference,

        /// Indirect reference to another scene object (by index)
        ResolvedReference,

        /// An arbitrary Mitsuba scene object
        Object,

        /// Generic type wrapper for arbitrary data exchange between plugins
        Any
    };

    /// Represents an indirect dependence on another object by name
    struct Reference {
        Reference(std::string_view name) : m_name(name) { }
        std::string_view id() const { return m_name; }
        bool operator==(const Reference &r) const { return r.m_name == m_name; }
        bool operator!=(const Reference &r) const { return r.m_name != m_name; }

    private:
        std::string m_name;
    };

    /// Represents an indirect dependence that has been resolved to a specific
    /// element of ``ParserState::nodes`` (by the parser::transform_resolve_references pass)
    struct ResolvedReference {
        ResolvedReference(size_t index) : m_index(index) { }
        size_t index() const { return m_index; }
        bool operator==(const ResolvedReference &r) const { return r.m_index == m_index; }
        bool operator!=(const ResolvedReference &r) const { return r.m_index != m_index; }

    private:
        size_t m_index;
    };

    /// Temporary data structure to store spectral data before expansion into a
    /// plugin like ``RegularSpectrum`` or ``IrregularSpectrum``.
    struct MI_EXPORT_LIB Spectrum {
        /// For sampled spectra: wavelength values (in nanometers)
        std::vector<double> wavelengths;

        /// Corresponding values, or uniform value if wavelengths is empty
        std::vector<double> values;

        /// True if wavelengths are regularly spaced
        bool m_regular = false;

        /// Default constructor (creates empty spectrum)
        Spectrum() = default;

        /// Construct uniform spectrum
        explicit Spectrum(double value) : m_regular(false) {
            values.push_back(value);
        }

        /// Construct spectrum from wavelength-value pairs
        Spectrum(std::vector<double> &&wl, std::vector<double> &&values);

        /// Construct spectrum from string (either single value or wavelength:value pairs)
        explicit Spectrum(std::string_view str);

        /// Construct regular spectrum from string of values and wavelength range
        Spectrum(std::string_view values, double wavelength_min, double wavelength_max);

        /// Construct regular spectrum from vector of values and wavelength range
        Spectrum(std::vector<double> &&values, double wavelength_min, double wavelength_max);

        /// Construct irregular spectrum from separate wavelength and value strings
        Spectrum(std::string_view wavelengths, std::string_view values);

        /// Construct spectrum from file
        explicit Spectrum(const fs::path &filename);

        /// Check if this is a uniform spectrum (single value, no wavelengths)
        bool is_uniform() const { return wavelengths.empty(); }

        /// Check if wavelengths are regularly spaced
        bool is_regular() const { return m_regular; }

        bool operator==(const Spectrum &other) const {
            return wavelengths == other.wavelengths && values == other.values;
        }
    };

    /// Forward declaration of iterator
    class key_iterator;

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
     * - Mitsuba object classes (``ref<BSDF>``, ``BSDF *``, etc.)
     *
     * Both single/double precision versions of arithmetic types are supported;
     * the function will convert them as needed. The function *cannot* be used
     * to obtain vectorized (e.g. JIT-compiled) arrays.
     */
    template <typename T> T get(std::string_view name) const {
        return get<T>(key_index_checked(name));
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
        size_t index = key_index(name);
        if (index == size_t(-1))
            return def_val;
        return get<T>(index);
    }

    /**
     * \brief Set a parameter value
     *
     * When a parameter with a matching names is already present, the method
     * raises an exception if \c raise_if_exists is set (the default).
     * Otherwise, it replaces the parameter.
     *
     * The parameter is initially marked as unqueried (see \ref was_queried).
     */
    template <typename T>
    void set(std::string_view name, T &&value, bool raise_if_exists = true) {
        using T2 = detail::prop_map_t<T>;

        static_assert(
            !std::is_same_v<T2, void>,
            "Properties::set<T>(): The requested type is not supported by the "
            "Properties class. Arbitrary data blobs may alternatively be "
            "passed using Properties::set(std::make_any(value))");

        size_t index = maybe_append(name, raise_if_exists);

        // Avoid implicit float->double conversion warning
        if constexpr (std::is_same_v<std::decay_t<T>, float>)
            set_impl<T2>(index, (T2) value);
        else if constexpr (detail::is_transform_3<std::decay_t<T>>::value)
            set_impl<AffineTransform<Point<double, 3>>>(index, AffineTransform<Point<double, 3>>(value));
        else
            set_impl<T2>(index, std::forward<T>(value));
    }

    /// Retrieve a texture parameter with variant-specific conversions
    /// (see get_texture_impl for details)
    template <typename T> ref<T> get_texture(std::string_view name) const {
        return static_cast<T*>(get_texture_impl(name, T::Variant, false, false).get());
    }

    /// Retrieve a texture parameter with default value and variant-specific conversions
    /// (see get_texture_impl for details)
    template <typename T, typename Float> ref<T> get_texture(std::string_view name, Float def) const {
        return static_cast<T*>(get_texture_impl(name, T::Variant, false, false, (double) def).get());
    }

    /// Retrieve an emissive texture parameter with variant-specific conversions
    /// (see get_texture_impl for details)
    template <typename T> ref<T> get_emissive_texture(std::string_view name) const {
        return static_cast<T*>(get_texture_impl(name, T::Variant, true, false).get());
    }

    /// Retrieve an emissive texture parameter with default value and variant-specific conversions
    /// (see get_texture_impl for details)
    template <typename T, typename Float> ref<T> get_emissive_texture(std::string_view name, Float def) const {
        return static_cast<T*>(get_texture_impl(name, T::Variant, true, false, (double) def).get());
    }

    /// Retrieve an unbounded texture parameter with default value and variant-specific conversions
    /// (see get_texture_impl for details)
    template <typename T, typename Float> ref<T> get_unbounded_texture(std::string_view name, Float def) const {
        return static_cast<T*>(get_texture_impl(name, T::Variant, false, true, (double) def).get());
    }

    /// Retrieve an unbounded texture parameter with variant-specific conversions
    /// (see get_texture_impl for details)
    template <typename T> ref<T> get_unbounded_texture(std::string_view name) const {
        return static_cast<T*>(get_texture_impl(name, T::Variant, false, true).get());
    }

    /**
     * \brief Retrieve a texture parameter (internal method)
     *
     * This method exposes a low level interface for texture construction, in
     * general \ref get_texture(), \ref get_emissive_texture(), and \ref
     * get_unbounded_texture() are preferable.
     *
     * The method retrieves or construct a texture object (a subclass of
     * ``mitsuba::Texture<...>``).
     *
     * If the parameter already holds a texture object, this function returns it
     * directly. Otherwise, it creates an appropriate texture based on the
     * property type and the current variant. The exact behavior is:
     *
     * **Float/Integer Values:**
     *   - Monochromatic variants: Create ``uniform`` texture with the value.
     *   - RGB/spectral variants:
     *     - For reflectance spectra: Create ``uniform`` texture with the value.
     *     - For emission spectra: Create ``d65`` texture with grayscale color.
     *
     * **Color Values (RGB triplets):**
     *   - Monochromatic variants: Compute luminance and create a
     *     ``uniform`` texture.
     *   - RGB/spectral variants:
     *     - For emission spectra: Create ``d65`` texture.
     *     - For reflectance spectra: Create ``srgb`` texture.
     *
     * **Spectrum Values:**
     *   *Uniform spectrum (single value):*
     *     - RGB variants: For emission spectra, create ``srgb`` texture with a
     *       color that represents the RGB appearance of a uniform spectral
     * emitter.
     *     - All other cases: Create ``uniform`` texture.
     *
     *   *Wavelength-value pairs:*
     *     - Spectral variants: create a ``regular`` or ``irregular`` spectrum
     *       texture based on the regularity of the wavelength-value pairs.
     *     - RGB/monochromatic variants: Pre-integrate against the CIE color
     *       matching functions to convert to sRGB color, then:
     *       - Monochromatic: Extract luminance and create ``uniform`` texture.
     *       - RGB: Create a ``srgb`` texture with the computed color.
     *
     * \param name
     *     The property name to look up
     *
     * \param emitter
     *     Set to true when retrieving textures for emission spectra
     *
     * \param unbounded
     *     Set this parameter to true if the spectrum is not emissive but may
     *     still exceed the [0,1] range. An example would be the real or
     *     imaginary index of refraction. This is important when spectral
     *     upsampling is involved.
     */
    ref<Object> get_texture_impl(std::string_view name,
                                 std::string_view variant,
                                 bool emitter,
                                 bool unbounded) const;

    /// Version of the above that switches to a default initialization when
    /// the property was not specified
    ref<Object> get_texture_impl(std::string_view name,
                                 std::string_view variant,
                                 bool emitter,
                                 bool unbounded,
                                 double value) const;

    /**
     * \brief Retrieve a volume parameter
     *
     * This method retrieves a volume parameter, where ``T`` is a subclass of
     * ``mitsuba::Volume<...>``.
     *
     * Scalar and texture values are also accepted. In this case, the plugin
     * manager will automatically construct a ``constvolume`` instance.
     */
    template <typename T>
    ref<T> get_volume(std::string_view name) const;

    /**
     * \brief Retrieve a volume parameter with float default
     *
     * When the volume parameter doesn't exist, creates a constant volume
     * with the specified floating point value.
     */
    template <typename T, typename Float>
    ref<T> get_volume(std::string_view name, Float def_val) const;

    /**
     * \brief Retrieve an arbitrarily typed value for inter-plugin communication
     *
     * This method enables plugins to exchange custom types that are not
     * natively supported by the Properties system. It uses type-erased storage
     * to hold arbitrary C++ objects while preserving type safety through
     * runtime type checking.
     *
     * The function raises an exception if the parameter does not exist or
     * cannot be cast to the requested type. Accessing the parameter
     * automatically marks it as queried.
     */
    template <typename T> const T& get_any(std::string_view name) const {
        size_t index = key_index_checked(name);
        const T *value = any_cast<T>(get_impl<Any>(index, true));
        if (!value)
            raise_any_type_error(index, typeid(T));
        return *value;
    }

    /**
     * \brief Set an arbitrarily typed value for inter-plugin communication
     *
     * This method allows storing arbitrary data types that cannot be represented
     * by the standard Properties types. The value is stored in a type-erased
     * Any container and can be retrieved later using get_any<T>().
     */
    template <typename T> void set_any(std::string_view name, T &&value) {
        set(name, Any(std::forward<T>(value)));
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
     * \brief Rename a property
     *
     * Changes the name of an existing property while preserving its value and
     * queried status.
     *
     * \param old_name The current name of the property
     * \param new_name The new name for the property
     * \return \c true upon success, \c false if the old property doesn't exist or new name already exists
     */
    bool rename_property(std::string_view old_name, std::string_view new_name);

    /**
     * \brief Manually mark a certain property as queried
     *
     * \param name The property name
     * \param value Whether to mark as queried (true) or unqueried (false)
     * \return \c true upon success
     */
    bool mark_queried(std::string_view name, bool value = true) const;

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

    /// Return the list of unqueried attributed
    std::vector<std::string> unqueried() const;

    /// Return one of the parameters (converting it to a string if necessary)
    std::string as_string(std::string_view name) const;

    /// Return one of the parameters (converting it to a string if necessary, with default value)
    std::string as_string(std::string_view name, std::string_view def_val) const;

    /**
     * \brief Try to retrieve a property value without implicit conversions
     *
     * This method attempts to retrieve a property value of type T. Unlike get<T>(),
     * it returns a pointer to the stored value without performing any implicit
     * conversions. If the property doesn't exist, has a different type, or would
     * require conversion, it returns nullptr.
     *
     * For Object-derived types, dynamic_cast is used to check type compatibility.
     * The property is only marked as queried if retrieval succeeds.
     *
     * \tparam T The requested type
     * \param name Property name
     * \return Pointer to the value if successful, nullptr otherwise
     */
    template <typename T>
    T* try_get(std::string_view name) const {
        size_t index = key_index(name);
        if (index == size_t(-1))
            return nullptr;
        return try_get<T>(index);
    }

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

    /**
     * \brief Compute a hash of the Properties object
     *
     * This hash is suitable for deduplication and ignores:
     * - The insertion order of properties
     * - The 'id' field (which assigns a name to the object elsewhere)
     * - Property names starting with '_arg_' (which are auto-generated)
     *
     * The hash function is designed to work with the equality operator
     * for identifying equivalent Properties objects that can be merged
     * during scene optimization.
     */
    size_t hash() const;

    /// Return the number of entries
    size_t size() const;

    /// Check if there are no entries
    bool empty() const { return size() == 0; }

    /// Return an iterator representing the beginning of the container
    key_iterator begin() const;

    /// Return an iterator representing the end of the container
    key_iterator end() const;

    /// Helper class to provide a range for filtered iteration
    class filtered_range {
    public:
        filtered_range(const Properties *props, Type filter_type)
            : m_props(props), m_filter_type(filter_type) { }

        key_iterator begin() const;
        key_iterator end() const;

    private:
        const Properties *m_props;
        Type m_filter_type;
    };

    /// Return a range that only yields properties of the specified type
    filtered_range filter(Type type) const { return filtered_range(this, type); }

    /// Return a range that only yields Object-type properties
    filtered_range objects() const { return filter(Type::Object); }

    MI_EXPORT_LIB friend
    std::ostream &operator<<(std::ostream &os, const Properties &p);
protected:
    /**
     * \brief Find the index of a property by name
     *
     * \return The index in the internal storage, or ``size_t(-1)`` if not found
     */
    size_t key_index(std::string_view name) const noexcept;

    /**
     * \brief Find the index of a property by name or raise an exception if the
     * entry was not found.
     *
     * \return The index in the internal storage
     */
    size_t key_index_checked(std::string_view name) const;

    /**
     * \brief Retrieve a scalar parameter by index
     *
     * This is the primary implementation. All type conversions and error
     * checking is done here. The name-based get() is a thin wrapper.
     */
    template <typename T> T get(size_t index) const;

    /// Raise an exception when an object has incompatible type
    [[noreturn]] void raise_object_type_error(size_t index,
                                              ObjectType expected_type,
                                              const ref<Object> &value) const;

    /// Raise an exception when get_any() has incompatible type
    [[noreturn]] void raise_any_type_error(size_t index,
                                           const std::type_info &requested_type) const;

    /**
     * \brief Mark a property as queried by its internal index
     *
     * This method is used internally by the iterator to mark properties
     * as accessed only after successful type casting.
     */
    void mark_queried(size_t index) const;

    /// Implementation detail of get()
    template <typename T>
    const T *get_impl(size_t index, bool raise_if_incompatible,
                      bool mark_queried = true) const;

    /// Implementation detail of set()
    template <typename T> void set_impl(size_t index, const T &value);
    template <typename T> void set_impl(size_t index, T &&value);

    /// Try to get a property value by index without implicit conversions
    template <typename T>
    T* try_get(size_t index) const {
        using T2 = detail::prop_map_t<T>;

        static_assert(
            !std::is_same_v<T2, void>,
            "Properties::try_get<T>(): The type T requires implicit conversion "
            "and cannot be retrieved via try_get() which returns a pointer. "
            "Use get<T>() instead.");

        // For Object types, handle dynamic cast
        if constexpr (std::is_base_of_v<Object, T>) {
            // Get the object without marking as queried
            const ref<Object> *obj = get_impl<ref<Object>>(index, false, false);
            if (!obj)
                return nullptr;

            // Ensure the type implements this interface
            T *ptr = dynamic_cast<T *>(const_cast<Object *>(obj->get()));
            if (!ptr)
                return nullptr;

            // Only mark as queried if cast succeeded
            mark_queried(index);
            return ptr;
        } else {
            // For non-Object types, directly get the value without conversions
            const T2 *value = get_impl<T2>(index, false, false);
            if (!value)
                return nullptr;

            mark_queried(index);
            return const_cast<T2*>(value);
        }
    }

    /**
     * \brief Get or create a property entry by name
     *
     * This method looks up an existing property by name. If found and
     * raise_if_exists is true, it raises an exception. If not found,
     * it creates a new entry with the given name and an empty value.
     */
    size_t maybe_append(std::string_view name, bool raise_if_exists);

    /// Get the name of an entry by index (for error messages)
    std::string_view entry_name(size_t index) const;

private:
    struct PropertiesPrivate;
    std::unique_ptr<PropertiesPrivate> d;
};

/// Iterator class for Properties
class MI_EXPORT_LIB Properties::key_iterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = key_iterator;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const key_iterator *;
    using reference         = const key_iterator &;

    key_iterator(const Properties *props, size_t index, Type filter_type = Type::Unknown);

    /// Return the current property name
    std::string_view name() const { return m_name; }

    /// Return the current property type
    Type type() const { return m_type; }

    /// Return the current property index in the internal storage
    size_t index() const { return m_index; }

    /// Check if the current property has been queried
    bool queried() const { return m_queried; }

    /// Retrieve the current property value using Properties::get<T>
    template <typename T> T get() const {
        return m_props->get<T>(m_index);
    }

    /**
     * \brief Attempt to retrieve and cast an object property to a specific type
     *
     * This method retrieves the property value if it's an Object type and
     * attempts to dynamically cast it to the requested type T. The property
     * is only marked as queried if the cast succeeds.
     *
     * \tparam T The target type (must be derived from Object)
     * \return A pointer to the object of type T if successful, nullptr otherwise
     */
    template <typename T>
    T* try_get() const {
        return m_props->try_get<T>(m_index);
    }

    reference operator*() const { return *this; }
    pointer operator->() const { return this; }

    key_iterator &operator++();
    key_iterator operator++(int);

    bool operator==(const key_iterator &other) const {
        return m_index == other.m_index;
    }
    bool operator!=(const key_iterator &other) const {
        return m_index != other.m_index;
    }

private:
    const Properties *m_props;
    size_t m_index;
    std::string_view m_name;
    Type m_type;
    Type m_filter_type;
    bool m_queried;

    void skip_to_next_valid();
};

/// Turn a Properties::Type enumeration value into string form
extern MI_EXPORT_LIB std::string_view property_type_name(Properties::Type type);

inline Properties::key_iterator Properties::filtered_range::begin() const {
    return key_iterator(m_props, 0, m_filter_type);
}

inline Properties::key_iterator Properties::filtered_range::end() const {
    return key_iterator(nullptr, (size_t) -1);
}


// Template implementation of Properties::get(size_t index)
template <typename T> T Properties::get(size_t index) const {
    using T2 = detail::prop_map_t<T>;

    static_assert(
        !std::is_same_v<T2, void>,
        "Properties::get<T>(): The requested type is not supported by the "
        "Properties class. Arbitrary data blobs may alternatively be "
        "passed using Properties::get_any()");

    // Handle int -> float conversion for convenience
    if constexpr (std::is_same_v<T2, double>) {
        if (auto *alt = get_impl<int64_t>(index, false); alt)
            return (T) *alt;
    }

    // Handle Vector3f -> Color3f conversion for convenience
    if constexpr (std::is_same_v<T2, Color<double, 3>>) {
        if (auto *alt = get_impl<dr::Array<double, 3>>(index, false); alt)
            return Color<double, 3>(*alt);
    }

    // Try to get the exact type
    const T2 &value = *get_impl<T2>(index, true);

    // Perform a range check just in case
    if constexpr (std::is_integral_v<T2> && !std::is_same_v<T2, bool>) {
        constexpr T min = std::numeric_limits<T>::min(),
                    max = std::numeric_limits<T>::max();

        if constexpr (std::is_unsigned_v<T>) {
            bool out_of_bounds = value < 0;
            if constexpr (max <= (T) std::numeric_limits<T2>::max())
                out_of_bounds |= value > (T2) max;

            if (out_of_bounds) {
                std::string_view name = entry_name(index);
                Throw("Property \"%s\": value %lld is out of bounds, must be "
                      "in the range [%llu, %llu]", name,
                      (long long) value,
                      (unsigned long long) min,
                      (unsigned long long) max);
            }
        } else {
            if (value < (T2) min || value > (T2) max) {
                std::string_view name = entry_name(index);
                Throw("Property \"%s\": value %lld is out of bounds, must be "
                      "in the range [%lld, %lld]", name,
                      (long long) value,
                      (long long) min,
                      (long long) max);
            }
        }
    }

    if constexpr (std::is_same_v<T2, ref<Object>>) {
        using TargetType = typename detail::base_type<T>::type;
        TargetType *ptr = dynamic_cast<TargetType *>(const_cast<Object *>(value.get()));
        if (!ptr)
            raise_object_type_error(index, TargetType::Type, value);
        return T(ptr);
    } else {
        // Handle Transform4f -> Transform3f conversion
        if constexpr (detail::is_transform_3<T>::value)
            return static_cast<T>(value.extract());
        else
            return static_cast<T>(value);
    }
}

// Implementation of volume-related template methods
template <typename T>
ref<T> Properties::get_volume(std::string_view name) const {
    if (!has_property(name))
        Throw("Property \"%s\" has not been specified!", name);

    Type prop_type = type(name);
    mark_queried(name);

    if (prop_type == Type::Object) {
        ref<Object> object = get<ref<Object>>(name);
        // Check if it's already a Volume
        if (T *volume = dynamic_cast<T *>(object.get()))
            return ref<T>(volume);

        // Otherwise, assume it's a texture and wrap it in a constvolume
        Properties props("constvolume");
        props.set("value", object);
        return PluginManager::instance()->create_object<T>(props);
    } else if (prop_type == Type::Float) {
        Properties props("constvolume");
        props.set("value", get<double>(name));
        return PluginManager::instance()->create_object<T>(props);
    } else if (prop_type == Type::Color || prop_type == Type::Spectrum) {
        // For Color/Spectrum properties, create a texture first
        Properties props("constvolume");
        props.set("value", get_texture_impl(name, T::Variant, false, false));
        return PluginManager::instance()->create_object<T>(props);
    } else {
        Throw("The property \"%s\" has the wrong type (expected "
              "<volume>, <texture> or <float>, got %s).", name,
              property_type_name(prop_type));
    }
}

template <typename T, typename Float>
ref<T> Properties::get_volume(std::string_view name, Float def_val) const {
    static_assert(std::is_arithmetic_v<Float>, "get_volume(): requires an arithmetic 'def_val' argument");
    if (!has_property(name)) {
        Properties props("constvolume");
        props.set("value", (double) def_val);
        return PluginManager::instance()->create_object<T>(props);
    }
    return get_volume<T>(name);
}

template<> MI_EXPORT_LIB void
Properties::set_impl<AffineTransform<Point<double, 3>>>(
    size_t, const AffineTransform<Point<double, 3>> &);

template<> MI_EXPORT_LIB void
Properties::set_impl<AffineTransform<Point<double, 3>>>(
    size_t, AffineTransform<Point<double, 3>> &&);

#define MI_EXPORT_PROP(Mode, ...)                                              \
    Mode template MI_EXPORT_LIB const __VA_ARGS__ *                            \
    Properties::get_impl<__VA_ARGS__>(size_t, bool, bool) const;               \
    Mode template MI_EXPORT_LIB void Properties::set_impl<__VA_ARGS__>(        \
        size_t, const __VA_ARGS__ &);                                          \
    Mode template MI_EXPORT_LIB void Properties::set_impl<__VA_ARGS__>(        \
        size_t, __VA_ARGS__ &&);

#define MI_EXPORT_PROP_ALL(Mode)                                               \
    MI_EXPORT_PROP(Mode, double)                                               \
    MI_EXPORT_PROP(Mode, int64_t)                                              \
    MI_EXPORT_PROP(Mode, bool)                                                 \
    MI_EXPORT_PROP(Mode, dr::Array<double, 3>)                                 \
    MI_EXPORT_PROP(Mode, Color<double, 3>)                                     \
    MI_EXPORT_PROP(Mode, AffineTransform<Point<double, 4>>)                    \
    MI_EXPORT_PROP(Mode, std::string)                                          \
    MI_EXPORT_PROP(Mode, ref<Object>)                                          \
    MI_EXPORT_PROP(Mode, typename Properties::Reference)                       \
    MI_EXPORT_PROP(Mode, typename Properties::ResolvedReference)               \
    MI_EXPORT_PROP(Mode, typename Properties::Spectrum)                        \
    MI_EXPORT_PROP(Mode, Any)


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
template <typename T, size_t N> struct prop_map<AffineTransform<Point<T, N>>> { using type = AffineTransform<Point<double, 4>>; };
template <typename T> struct prop_map<mitsuba::ref<T>> { using type = ref<Object>; };
template <typename T> struct prop_map<T, std::enable_if_t<std::is_base_of_v<Object, std::remove_pointer_t<T>>>> { using type = ref<Object>; };
template <typename T> struct prop_map<dr::Tensor<T>> { using type = Any; };
template <> struct prop_map<const char *> { using type = std::string; };
template <> struct prop_map<char *> { using type = std::string; };
template <> struct prop_map<std::string> { using type = std::string; };
template <> struct prop_map<std::string_view> { using type = std::string; };
template <size_t N> struct prop_map<char[N]> { using type = std::string; };
template <size_t N> struct prop_map<const char[N]> { using type = std::string; };
template <> struct prop_map<Properties::Reference> { using type = Properties::Reference; };
template <> struct prop_map<Properties::ResolvedReference> { using type = Properties::ResolvedReference; };
template <> struct prop_map<Properties::Spectrum> { using type = Properties::Spectrum; };
template <> struct prop_map<Any> { using type = Any; };
template <typename T> struct base_type { using type = T; };
template <typename T> struct base_type<ref<T>> { using type = T; };
NAMESPACE_END(detail)
NAMESPACE_END(mitsuba)
