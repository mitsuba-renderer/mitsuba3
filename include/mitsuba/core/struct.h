#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/core/hash.h>
#include <vector>
#include <limits>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Descriptor for specifying the contents and in-memory layout of a
 * POD-style data record
 */
class MTS_EXPORT_CORE Struct : public Object {
public:
    /// Type of a field in the \c Struct
    enum EType {
        /* Signed and unsigned integer values */
        EInt8, EUInt8,
        EInt16, EUInt16,
        EInt32, EUInt32,
        EInt64, EUInt64,

        /* Floating point values */
        EFloat16, EFloat32, EFloat64
    };

    /// Byte order of the fields in the \c Struct
    enum EByteOrder {
        ELittleEndian,
        EBigEndian
    };

    /// Field-specific flags
    enum EFlags {
        /**
         * Only applies to integer fields: specifies
         * whether the field encodes a normalized value
         * in the range [0, 1]
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
        EAssert     = 0x04
    };

    /// Field specifier with size and offset
    struct Field {
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

        /// Return a hash code associated with this \c Field
        friend size_t hash(const Field &f) {
            size_t value = hash(f.name);
            value = hash_combine(value, hash(f.type));
            value = hash_combine(value, hash(f.size));
            value = hash_combine(value, hash(f.offset));
            value = hash_combine(value, hash(f.flags));
            value = hash_combine(value, hash(f.default_));
            return value;
        }

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

        bool isUnsigned() const {
            return type == Struct::EUInt8 ||
                   type == Struct::EUInt16 ||
                   type == Struct::EUInt32 ||
                   type == Struct::EUInt64;
        }

        bool isFloat() const {
            return type == Struct::EFloat16 ||
                   type == Struct::EFloat32 ||
                   type == Struct::EFloat64;
        }

        bool isSigned() const {
            return !isUnsigned();
        }

        bool isInteger() const {
            return !isFloat();
        }

        std::pair<double, double> getRange() const;
    };

    /// Create a new \c Struct and indicate whether the contents are packed or aligned
    Struct(bool pack = false, EByteOrder byteOrder = ELittleEndian);

    /// Append a new field to the \c Struct; determines size and offset automatically
    Struct &append(const std::string &name, EType type, uint32_t flags = 0,
                   double default_ = 0.0);

    /// Append a new field to the \c Struct (manual version)
    Struct &append(Field field) { m_fields.push_back(field); return *this; }

    /// Access an individual field entry
    const Field &operator[](size_t i) const { return m_fields[i]; }

    /// Return the size (in bytes) of the data structure, including padding
    size_t size() const;

    /// Return the alignment (in bytes) of the data structure
    size_t alignment() const;

    /// Return the number of fields
    size_t fieldCount() const { return m_fields.size(); }

    /// Return the byte order of the \c Struct
    EByteOrder byteOrder() const { return m_byteOrder; }

    /// Return the byte order of the host machine
    static EByteOrder hostByteOrder();

    /// Look up a field by name (throws an exception if not found)
    const Field &getField(const std::string &name) const;

    /// Return a string representation
    std::string toString() const override;

    /// Return an iterator associated with the first field
    std::vector<Field>::const_iterator begin() const { return m_fields.cbegin(); }

    /// Return an iterator associated with the end of the data structure
    std::vector<Field>::const_iterator end() const { return m_fields.cend(); }

    /// Return a hash code associated with this \c Struct
    friend size_t hash(const Struct &s) {
        return hash_combine(hash_combine(hash(s.m_fields), hash(s.m_pack)),
                            hash(s.m_byteOrder));
    }

    /// Equality operator
    bool operator==(const Struct &s) const {
        return m_fields == s.m_fields && m_pack == s.m_pack && m_byteOrder == s.m_byteOrder;
    }

    /// Inequality operator
    bool operator!=(const Struct &s) const {
        return !operator==(s);
    }

    MTS_DECLARE_CLASS()
protected:
    std::vector<Field> m_fields;
    bool m_pack;
    EByteOrder m_byteOrder;
};

/**
 * \brief This class solves the any-to-any problem: effiently converting from
 * one kind of structured data representation to another
 *
 * Graphics applications often need to convert from one kind of structured
 * representation to another. Consider the following data records which
 * both describe positions tagged with color data.
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
 * \c Target represents the assumed input of an existing algorithm. Not only
 * are the formats (e.g. float vs half or uint8_t, incompatible endianness) and
 * encodings different (e.g. gamma correction vs linear space), but the second
 * record even has a different order and extra fields that don't exist in the
 * first one.
 *
 * This class provides a routine \ref convert() which
 * <ol>
 * <li>reorders entries</li>
 * <li>converts between many different formats (u[int]8-64, float16-64)</li>
 * <li>performs endianness conversion</li>
 * <li>applies or removes gamma correction</li>
 * <li>optionally checks that certain entries have expected default values</li>
 * <li>substitutes missing values with specified defaults</li>
 * </ol>
 *
 * On x86_64 platforms, the implementation of this class relies on a JIT
 * compiler to instantiate a function that efficiently performs the conversion
 * for any number of elements. The function is cached and reused if this
 * particular conversion is needed any any later point.
 *
 * On non-x86_64 platforms, a slow fallback implementation is used.
 */
class MTS_EXPORT_CORE StructConverter : public Object {
    using FuncType = bool (*) (size_t, const void *, void *);
public:
    /// Construct an optimized conversion routine going from \c source to \c target
    StructConverter(const Struct *source, const Struct *target);

    /// Convert \c count elements. Returns \c true upon success
#if defined(__x86_64__) || defined(_WIN64)
    bool convert(size_t count, const void *src, void *dest) const {
        return m_func(count, src, dest);
    }
#else
    bool convert(size_t count, const void *src, void *dest) const;
#endif

    /// Return the source \c Struct descriptor
    const Struct *source() const { return m_source.get(); }

    /// Return the target \c Struct descriptor
    const Struct *target() const { return m_target.get(); }

    /// Return a string representation
    std::string toString() const override;

    MTS_DECLARE_CLASS()
protected:
    ref<const Struct> m_source;
    ref<const Struct> m_target;
#if defined(__x86_64__) || defined(_WIN64)
    FuncType m_func;
#endif
};

NAMESPACE_END(mitsuba)
