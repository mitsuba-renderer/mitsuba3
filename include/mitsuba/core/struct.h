#pragma once

#include <mitsuba/core/object.h>
#include <vector>
#include <limits>

NAMESPACE_BEGIN(mitsuba)

#if defined(ENOKI_X86_64)
#  define MTS_STRUCTCONVERTER_USE_JIT 1
#else
#  define MTS_STRUCTCONVERTER_USE_JIT 0
#endif

/**
 * \brief Descriptor for specifying the contents and in-memory layout
 * of a POD-style data record
 *
 * \remark The python API provides an additional \c dtype() method, which
 * returns the NumPy \c dtype equivalent of a given \c Struct instance.
 */
class MTS_EXPORT_CORE Struct : public Object {
public:
    MTS_REGISTER_CLASS(Struct, Object)

    /// Type of a field in the \c Struct
    enum EType {
        /* Invalid/unspecified */
        EInvalid = 0,

        /* Signed and unsigned integer values */
        EUInt8,  EInt8,
        EUInt16, EInt16,
        EUInt32, EInt32,
        EUInt64, EInt64,

        /* Floating point values */
        EFloat16, EFloat32, EFloat64,

        /* Compile-time float precision */
        #if defined(SINGLE_PRECISION)
            EFloat = EFloat32
        #else
            EFloat = EFloat64
        #endif
    };

    /// Byte order of the fields in the \c Struct
    enum EByteOrder {
        ELittleEndian,
        EBigEndian,
        EHostByteOrder
    };

    /// Field-specific flags
    enum EFlags {
        /**
         * Specifies whether an integer field encodes a normalized value in the
         * range [0, 1]. The flag is ignored if specified for floating point
         * valued fields.
         */
        ENormalized = 0x01,

        /**
         * Specifies whether the field encodes a sRGB gamma-corrected value.
         * Assumes \c ENormalized is also specified.
         */
        EGamma      = 0x02,

        /**
         * In \ref FieldConverter::convert, check that the field value matches
         * the specified default value. Otherwise, return a failure
         */
        EAssert     = 0x04,

        /**
         * In \ref FieldConverter::convert, when the field is missing in the
         * source record, replace it by the specified default value
         */
        EDefault    = 0x08,

        /**
         * In \ref FieldConverter::convert, when an input structure contains a
         * weight field, the value of all entries are considered to be
         * expressed relative to its value. Converting to an un-weighted
         * structure entails a division by the weight.
         */
        EWeight     = 0x10
    };

    /// Field specifier with size and offset
    struct MTS_EXPORT Field {
        /// Name of the field
        std::string name;

        /// Type identifier
        EType type;

        /// Size in bytes
        size_t size;

        /// Offset within the \c Struct (in bytes)
        size_t offset;

        /// Additional flags
        uint32_t flags;

        /// Default value
        double default_;

        /**
         * \brief For use with \ref StructConverter::convert()
         *
         * Specifies a pair of weights and source field names that will be
         * linearly blended to obtain the output field value. Note that this
         * only works for floating point fields or integer fields with the \ref
         * ENormalized flag. Gamma-corrected fields will be blended in linear
         * space.
         */
        std::vector<std::pair<double, std::string>> blend;

        /// Return a hash code associated with this \c Field
        friend MTS_EXPORT_CORE size_t hash(const Field &f);

        /// Equality operator
        bool operator==(const Field &f) const {
            return name == f.name && type == f.type && size == f.size &&
                   offset == f.offset && flags == f.flags &&
                   default_ == f.default_;
        }

        /// Equality operator
        bool operator!=(const Field &f) const {
            return !operator==(f);
        }

        bool is_unsigned() const {
            return Struct::is_unsigned(type);
        }

        bool is_signed() const {
            return Struct::is_signed(type);
        }

        bool is_float() const {
            return Struct::is_float(type);
        }

        bool is_integer() const {
            return Struct::is_integer(type);
        }

        std::pair<double, double> range() const {
            return Struct::range(type);
        }
    };

    using FieldIterator      = std::vector<Field>::iterator;
    using FieldConstIterator = std::vector<Field>::const_iterator;

    /// Create a new \c Struct and indicate whether the contents are packed or aligned
    Struct(bool pack = false, EByteOrder byte_order = EHostByteOrder);

    /// Copy constructor
    Struct(const Struct &s);

    /// Append a new field to the \c Struct; determines size and offset automatically
    Struct &append(const std::string &name, EType type,
                   uint32_t flags = 0, double default_ = 0.0);

    /// Append a new field to the \c Struct (manual version)
    Struct &append(Field field) { m_fields.push_back(field); return *this; }

    /// Access an individual field entry
    const Field &operator[](size_t i) const { return m_fields[i]; }

    /// Access an individual field entry
    Field &operator[](size_t i) { return m_fields[i]; }

    /// Check if the \c Struct has a field of the specified name
    bool has_field(const std::string &name) const;

    /// Return the size (in bytes) of the data structure, including padding
    size_t size() const;

    /// Return the alignment (in bytes) of the data structure
    size_t alignment() const;

    /// Return the number of fields
    size_t field_count() const { return m_fields.size(); }

    /// Return the byte order of the \c Struct
    EByteOrder byte_order() const { return m_byte_order; }

    /// Return the byte order of the host machine
    static EByteOrder host_byte_order() {
        #if defined(LITTLE_ENDIAN)
            return ELittleEndian;
        #elif defined(BIG_ENDIAN)
            return ELittleEndian;
        #else
            #error Either LITTLE_ENDIAN or BIG_ENDIAN must be defined!
        #endif
    };

    /// Look up a field by name (throws an exception if not found)
    const Field &field(const std::string &name) const;

    /// Look up a field by name. Throws an exception if not found
    Field &field(const std::string &name);

    /// Return the offset of the i-th field
    size_t offset(size_t i) const { return operator[](i).offset; }

    /// Return the offset of field with the given name
    size_t offset(const std::string &name) const { return field(name).offset; }

    /// Return an iterator associated with the first field
    FieldConstIterator begin() const { return m_fields.cbegin(); }

    /// Return an iterator associated with the first field
    FieldIterator begin() { return m_fields.begin(); }

    /// Return an iterator associated with the end of the data structure
    FieldConstIterator end() const { return m_fields.cend(); }

    /// Return an iterator associated with the end of the data structure
    FieldIterator end() { return m_fields.end(); }

    /// Return a hash code associated with this \c Struct
    friend MTS_EXPORT_CORE size_t hash(const Struct &s);

    /// Equality operator
    bool operator==(const Struct &s) const {
        return m_fields == s.m_fields &&
               m_pack == s.m_pack &&
               m_byte_order == s.m_byte_order;
    }

    /// Inequality operator
    bool operator!=(const Struct &s) const {
        return !operator==(s);
    }

    /// Return a string representation
    std::string to_string() const override;

    /// Check whether the given type is an unsigned type
    static bool is_unsigned(EType type) {
        return type == Struct::EUInt8 ||
               type == Struct::EUInt16 ||
               type == Struct::EUInt32 ||
               type == Struct::EUInt64;
    }

    /// Check whether the given type is a signed type
    static bool is_signed(EType type) {
        return !is_unsigned(type);
    }

    /// Check whether the given type is an integer type
    static bool is_integer(EType type) {
        return !is_float(type);
    }

    /// Check whether the given type is a floating point type
    static bool is_float(EType type) {
        return type == Struct::EFloat16 ||
               type == Struct::EFloat32 ||
               type == Struct::EFloat64;
    }

    /// Return the representable range of the given type
    static std::pair<double, double> range(EType type);

protected:
    std::vector<Field> m_fields;
    bool m_pack;
    EByteOrder m_byte_order;
};

template <typename T> struct struct_traits { };

#define MTS_STRUCT_TRAITS(type, entry) \
    template <> struct struct_traits<type> { \
        static constexpr Struct::EType value = Struct::entry; \
    };

MTS_STRUCT_TRAITS(int8_t, EInt8);
MTS_STRUCT_TRAITS(uint8_t, EUInt8);
MTS_STRUCT_TRAITS(int16_t, EInt16);
MTS_STRUCT_TRAITS(uint16_t, EUInt16);
MTS_STRUCT_TRAITS(int32_t, EInt32);
MTS_STRUCT_TRAITS(uint32_t, EUInt32);
MTS_STRUCT_TRAITS(int64_t, EInt64);
MTS_STRUCT_TRAITS(uint64_t, EUInt64);
MTS_STRUCT_TRAITS(enoki::half, EFloat16);
MTS_STRUCT_TRAITS(float, EFloat32);
MTS_STRUCT_TRAITS(double, EFloat64);

#undef MTS_STRUCT_TRAITS

/**
 * \brief This class solves the any-to-any problem: effiently converting from
 * one kind of structured data representation to another
 *
 * Graphics applications often need to convert from one kind of structured
 * representation to another, for instance when loading/saving image or mesh
 * data. Consider the following data records which both describe positions
 * tagged with color data.
 *
 * \code
 * struct Source { // <-- Big endian! :(
 *    uint8_t r, g, b; // in sRGB
 *    half x, y, z;
 * };
 *
 * struct Target { // <-- Little endian!
 *    float x, y, z;
 *    float r, g, b, a; // in linear space
 * };
 * \endcode
 *
 * The record \c Source may represent what is stored in a file on disk, while
 * \c Target represents the expected input of the implementation. Not only
 * are the formats (e.g. float vs half or uint8_t, incompatible endianness) and
 * encodings different (e.g. gamma correction vs linear space), but the second
 * record even has a different order and extra fields that don't exist in the
 * first one.
 *
 * This class provides a routine \ref convert() which
 * <ol>
 *   <li>reorders entries</li>
 *   <li>converts between many different formats (u[int]8-64, float16-64)</li>
 *   <li>performs endianness conversion</li>
 *   <li>applies or removes gamma correction</li>
 *   <li>optionally checks that certain entries have expected default values</li>
 *   <li>substitutes missing values with specified defaults</li>
 *   <li>performs linear transformations of groups of fields (e.g. between
 *       different RGB color spaces)</li>
 *   <li>applies dithering to avoid banding artifacts when converting 2D images</li>
 * </ol>
 *
 * The above operations can be arranged in countless ways, which makes it hard
 * to provide an efficient generic implementation of this functionality. For
 * this reason, the implementation of this class relies on a JIT compiler that
 * generates fast conversion code on demand for each specific conversion. The
 * function is cached and reused in case the same conversion is needed later
 * on. Note that JIT compilation only works on x86_64 processors; other
 * platforms use a slow generic fallback implementation.
 */
class MTS_EXPORT_CORE StructConverter : public Object {
    using FuncType = bool (*) (size_t, size_t, const void *, void *);
public:
    MTS_REGISTER_CLASS(StructConverter, Object)

    /// Construct an optimized conversion routine going from \c source to \c target
    StructConverter(const Struct *source, const Struct *target, bool dither = false);

    /// Convert \c count elements. Returns \c true upon success
    bool convert(size_t count, const void *src, void *dest) const {
        return convert_2d(count, 1, src, dest);
    }

    /**
     * \brief Convert a 2D image
     *
     * This function should be used instead of \ref convert when working with
     * 2D image data. It is equivalent to calling the former function with
     * <tt>width*height</tt> elements except for one major difference: when
     * quantizing floating point input to integer output, the implementation
     * performs dithering to avoid banding artifacts (if enabled in the
     * constructor).
     *
     * \return \c true upon success
     */
#if MTS_STRUCTCONVERTER_USE_JIT == 1
    bool convert_2d(size_t width, size_t height, const void *src,
                    void *dest) const {
        return m_func(width, height, src, dest);
    }
#else
    bool convert_2d(size_t width, size_t height, const void *src,
                    void *dest) const;
#endif

    /// Return the source \c Struct descriptor
    const Struct *source() const { return m_source.get(); }

    /// Return the target \c Struct descriptor
    const Struct *target() const { return m_target.get(); }

    /// Return a string representation
    std::string to_string() const override;

protected:

#if MTS_STRUCTCONVERTER_USE_JIT == 0
    /* Support data structures/functions for non-accelerated
       conversion backend */

    struct Value {
        Struct::EType type;
        uint32_t flags;
        union {
            Float f;
            float s;
            double d;
            int64_t i;
            uint64_t u;
        };
    };

    bool load(const uint8_t *src, const Struct::Field &f, Value &value) const;
    void linearize(Value &value) const;
    void save(uint8_t *dst, const Struct::Field &f, Value value, size_t x, size_t y) const;
#endif

protected:
    ref<const Struct> m_source;
    ref<const Struct> m_target;
#if MTS_STRUCTCONVERTER_USE_JIT == 1
    FuncType m_func;
#else
    bool m_dither;
#endif
};

extern MTS_EXPORT_CORE std::ostream &operator<<(std::ostream &os, Struct::EType value);

NAMESPACE_END(mitsuba)
