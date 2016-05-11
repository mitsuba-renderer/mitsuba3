#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/object.h>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(detail)
template <typename T> struct serialization_helper;
NAMESPACE_END(detail)

/** \brief Abstract seekable stream class
 *
 * Specifies all functions to be implemented by stream
 * subclasses and provides various convenience functions
 * layered on top of on them.
 *
 * All read<b>X</b>() and write<b>X</b>() methods support transparent
 * conversion based on the endianness of the underlying system and the
 * value passed to \ref setByteOrder(). Whenever \ref getHostByteOrder()
 * and \ref getByteOrder() disagree, the endianness is swapped.
 *
 * TODO: explain Table of Contents feature.
 * TODO: explain write-only / read-only modes.
 *
 * \sa FileStream, MemoryStream, DummyStream
 */
class MTS_EXPORT_CORE Stream : public Object {

protected:
    template <typename T> friend struct detail::serialization_helper;

public:
    /// Defines the byte order to use in this Stream
    enum EByteOrder {
        EBigEndian = 0,                ///< PowerPC, SPARC, Motorola 68K
        ELittleEndian = 1,             ///< x86, x86_64
        ENetworkByteOrder = EBigEndian ///< Network byte order (an alias for big endian)
    };

    /**
     * \brief Creates a new stream.
     * By default, it assumes the byte order of the underlying system,
     * i.e. no endianness conversion is performed.
     *
     * \param writeEnabled If true, the stream will be write-only, otherwise
     *                     it will be read-only.
     * \param tableOfContents If true, the stream will function by generating
     *                        a "table of contents" (TOC), using hierarchical
     *                        prefixes for fields being written.
     *                        TODO: enable / disable some methods (use templates?)
     */
    Stream(bool writeEnabled, bool tableOfContents = true);

    /// Returns a string representation of the stream
    virtual std::string toString() const;

    // =========================================================================
    //! @{ \name Abstract methods that need to be implemented by subclasses
    // =========================================================================
protected:
    /**
     * \brief Reads a specified amount of data from the stream.
     *
     * Throws an exception when the stream ended prematurely.
     */
    virtual void read(void *ptr, size_t size) = 0;

    /**
     * \brief Writes a specified amount of data into the stream.
     *
     * Throws an exception when not all data could be written.
     */
    virtual void write(const void *ptr, size_t size) = 0;

    /// @}
    // =========================================================================

public:
    // =========================================================================
    //! @{ \name Endianness handling
    // =========================================================================

    // TODO

    /// @}
    // =========================================================================

    // =========================================================================
    //! @{ \name Table of Contents (TOC)
    // =========================================================================

    /**
     * Push a name prefix onto the stack (use this to isolate
     * identically-named data fields)
     */
    void push(const std::string &name);

    /// Pop a name prefix from the stack
    void pop();

    /// Return all field names under the current name prefix
    std::vector<std::string> keys() const;

    /// Retrieve a field from the serialized file (only valid in read mode)
    template <typename T> bool get(const std::string &name, T &value);

    /// Store a field in the serialized file (only valid in write mode)
    template <typename T> void set(const std::string &name, const T &value);

    /// @}
    // =========================================================================

protected:
    // TODO: implementation helpers


private:
    bool m_write_Mode;
    bool m_tocEnabled;
    static const EByteOrder m_hostByteOrder;
    EByteOrder m_byteOrder;
    // TODO: data members (ToC, edianness, etc).
};

extern MTS_EXPORT_CORE std::ostream
    &operator<<(std::ostream &os, const Stream::EByteOrder &value);

NAMESPACE_BEGIN(detail)

/**
 * \brief The <tt>serialization_trait</tt> templated structure provides
 * type-specific information relevant to serialization.
 * <tt>type_id</tt> is a unique, prefix-free code identifying the type.
 */
template <typename T, typename SFINAE = void> struct serialization_traits { };
template <> struct serialization_traits<int8_t>           { const char *type_id = "u8";  };
template <> struct serialization_traits<uint8_t>          { const char *type_id = "s8";  };

template <typename T> struct serialization_traits<T> :
    serialization_traits<typename std::underlying_type<T>::type,
                         typename std::enable_if<std::is_enum<T>::value>::type> { };

template <typename T> struct serialization_helper {
    static std::string type_id() { return serialization_traits<T>().type_id; }

    static void write(Stream &s, const T *value, size_t count) {
        s.write(value, sizeof(T) * count);
    }

    static void read(Stream &s, T *value, size_t count) {
        s.read(value, sizeof(T) * count);
    }
};

// TODO: `serialization_helper` template specializations to support various types

NAMESPACE_END(detail)

NAMESPACE_END(mitsuba)

