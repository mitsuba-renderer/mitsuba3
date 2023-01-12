#pragma once

#include <drjit/array_utils.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/logger.h>
#include <set>

#if defined(_WIN32)
    #include <intrin.h>
#endif

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(detail)
template <typename T, typename SFINAE = void> struct serialization_helper;
NAMESPACE_END(detail)

/** \brief Abstract seekable stream class
 *
 * Specifies all functions to be implemented by stream
 * subclasses and provides various convenience functions
 * layered on top of on them.
 *
 * All ``read*()`` and ``write*()`` methods support transparent
 * conversion based on the endianness of the underlying system and the
 * value passed to \ref set_byte_order(). Whenever \ref host_byte_order()
 * and \ref byte_order() disagree, the endianness is swapped.
 *
 * \sa FileStream, MemoryStream, DummyStream
 */
class MI_EXPORT_LIB Stream : public Object {

protected:
    /* In general, only low-level type serializers should call the stream's
     * protected read & write functions. Otherwise, make sure to handle
     * endianness swapping explicitly.
     */
    template <typename T, typename SFINAE> friend struct detail::serialization_helper;

public:

    /// Defines the byte order (endianness) to use in this Stream
    enum EByteOrder {
        EBigEndian = 0,                /// PowerPC, SPARC, Motorola 68K
        ELittleEndian = 1,             /// x86, x86_64
        ENetworkByteOrder = EBigEndian /// Network byte order (an alias for big endian)
    };

    /**
     * \brief Creates a new stream.
     *
     * By default, this function sets the stream byte order
     * to that of the system (i.e. no conversion is performed)
     */
    Stream();

    /// Returns a human-readable descriptor of the stream
    virtual std::string to_string() const override;

    /** \brief Closes the stream.
     *
     * No further read or write operations are permitted.
     *
     * This function is idempotent.
     * It may be called automatically by the destructor.
     */
    virtual void close() = 0;

    /// Whether the stream is closed (no read or write are then permitted).
    virtual bool is_closed() const = 0;

    // =========================================================================
    //! @{ \name Abstract methods that need to be implemented by subclasses
    // =========================================================================

    /**
     * \brief Reads a specified amount of data from the stream.
     * \note This does <b>not</b> handle endianness swapping.
     *
     * Throws an exception when the stream ended prematurely.
     * Implementations need to handle endianness swap when appropriate.
     */
    virtual void read(void *p, size_t size) = 0;

    /**
     * \brief Writes a specified amount of data into the stream.
     * \note This does <b>not</b> handle endianness swapping.
     *
     * Throws an exception when not all data could be written.
     * Implementations need to handle endianness swap when appropriate.
     */
    virtual void write(const void *p, size_t size) = 0;

    /** \brief Seeks to a position inside the stream.
     *
     * Seeking beyond the size of the buffer will not modify the length of
     * its contents. However, a subsequent write should start at the sought
     * position and update the size appropriately.
     */
    virtual void seek(size_t pos) = 0;

    /** \brief Truncates the stream to a given size.
     *
     * The position is updated to <tt>min(old_position, size)</tt>.
     * Throws an exception if in read-only mode.
     */
    virtual void truncate(size_t size) = 0;

    /// Gets the current position inside the stream
    virtual size_t tell() const = 0;

    /// Returns the size of the stream
    virtual size_t size() const = 0;

    /// Flushes the stream's buffers, if any
    virtual void flush() = 0;

    /// Can we write to the stream?
    virtual bool can_write() const = 0;

    /// Can we read from the stream?
    virtual bool can_read() const = 0;

    /// @}
    // =========================================================================

    // =========================================================================
    //! @{ \name Read and write values
    // =========================================================================

    /**
     * \brief Reads one object of type T from the stream at the current position
     * by delegating to the appropriate <tt>serialization_helper</tt>.
     *
     * Endianness swapping is handled automatically if needed.
     */
    template <typename T>
    void read(T &value) {
        using helper = detail::serialization_helper<T>;
        helper::read(*this, &value, 1, needs_endianness_swap());
    }

    /**
     * \brief Reads multiple objects of type T from the stream at the current position
     * by delegating to the appropriate <tt>serialization_helper</tt>.
     *
     * Endianness swapping is handled automatically if needed.
     */
    template <typename T>
    void read_array(T *value, size_t count) {
        using helper = detail::serialization_helper<T>;
        helper::read(*this, value, count, needs_endianness_swap());
    }

    /**
     * \brief Reads one object of type T from the stream at the current position
     * by delegating to the appropriate <tt>serialization_helper</tt>.
     *
     * Endianness swapping is handled automatically if needed.
     */
    template <typename T>
    void write(const T &value) {
        using helper = detail::serialization_helper<T>;
        helper::write(*this, &value, 1, needs_endianness_swap());
    }

    /**
     * \brief Reads multiple objects of type T from the stream at the current position
     * by delegating to the appropriate <tt>serialization_helper</tt>.
     *
     * Endianness swapping is handled automatically if needed.
     */
    template <typename T>
    void write_array(const T *value, size_t count) {
        using helper = detail::serialization_helper<T>;
        helper::write(*this, value, count, needs_endianness_swap());
    }

    /// Convenience function for reading a line of text from an ASCII file
    virtual std::string read_line();

    /// Convenience function for reading a contiguous token from an ASCII file
    virtual std::string read_token();

    /// Convenience function for writing a line of text to an ASCII file
    void write_line(const std::string &text);

    /// Skip ahead by a given number of bytes
    void skip(size_t amount);

    /// @}
    // =========================================================================

    // =========================================================================
    //! @{ \name Endianness handling
    // =========================================================================

    /** \brief Sets the byte order to use in this stream.
     *
     * Automatic conversion will be performed on read and write operations
     * to match the system's native endianness.
     *
     * No consistency is guaranteed if this method is called after
     * performing some read and write operations on the system using a
     * different endianness.
     */
    void set_byte_order(EByteOrder byte_order);

    /// Returns the byte order of this stream.
    EByteOrder byte_order() const { return m_byte_order; }

    /// Returns true if we need to perform endianness swapping before writing or reading.
    bool needs_endianness_swap() const {
        return m_byte_order != m_host_byte_order;
    }

    /// Returns the byte order of the underlying machine.
    static EByteOrder host_byte_order() { return m_host_byte_order; }


    /// @}
    // =========================================================================

    MI_DECLARE_CLASS()
protected:
    /// Destructor
    virtual ~Stream();

    /// Copying is disallowed.
    Stream(const Stream&) = delete;
    void operator=(const Stream&) = delete;

private:
    static const EByteOrder m_host_byte_order;
    EByteOrder m_byte_order;
};

extern MI_EXPORT_LIB std::ostream
    &operator<<(std::ostream &os, const Stream::EByteOrder &value);

NAMESPACE_BEGIN(detail)

/*
 * The remainder of this file provides template specializations to add support
 * for serialization of several basic types. Support for new types can be added
 * in other headers by providing the appropriate template specializations.
 */

template <typename T, std::enable_if_t<sizeof(T) == 1, int> = 0> T swap(const T &v) {
    return v;
}

template <typename T, std::enable_if_t<sizeof(T) == 2, int> = 0> T swap(const T &v) {
#if !defined(_WIN32)
    return dr::memcpy_cast<T>(__builtin_bswap16(dr::memcpy_cast<uint16_t>(v)));
#else
    return dr::memcpy_cast<T>(_byteswap_ushort(dr::memcpy_cast<uint16_t>(v)));
#endif
}

template <typename T, std::enable_if_t<sizeof(T) == 4, int> = 0> T swap(const T &v) {
#if !defined(_WIN32)
    return dr::memcpy_cast<T>(__builtin_bswap32(dr::memcpy_cast<uint32_t>(v)));
#else
    return dr::memcpy_cast<T>(_byteswap_ulong(dr::memcpy_cast<uint32_t>(v)));
#endif
}

template <typename T, std::enable_if_t<sizeof(T) == 8, int> = 0> T swap(const T &v) {
#if !defined(_WIN32)
    return dr::memcpy_cast<T>(__builtin_bswap64(dr::memcpy_cast<uint64_t>(v)));
#else
    return dr::memcpy_cast<T>(_byteswap_uint64(dr::memcpy_cast<uint64_t>(v)));
#endif
}

/** \brief The serialization_helper<T> implementations for new types should
 * in general be implemented as a series of calls to the lower-level
 * serialization_helper::{read,write} functions.
 * This way, endianness swapping needs only be handled at the lowest level.
 */
template <typename T, typename SFINAE> struct serialization_helper {
    static std::string type_id() {
        std::string descr(2, '\0');
        descr[0] = std::is_floating_point_v<T> ? 'f' : (std::is_signed_v<T> ? 's' : 'u');
        descr[1] = '0' + sizeof(T);
        return descr;
    }

    /** \brief Writes <tt>count</tt> values of type T into stream <tt>s</tt>
     * starting at its current position.
     * Note: <tt>count</tt> is the number of values, <b>not</b> a size in bytes.
     *
     * Support for additional types can be added in any header file by
     * declaring a template specialization for your type.
     */
    static void write(Stream &s, const T *value, size_t count, bool swap) {
        if (likely(!swap)) {
            s.write(value, sizeof(T) * count);
        } else {
            std::unique_ptr<T[]> v(new T[count]);
            for (size_t i = 0; i < count; ++i)
                v[i] = detail::swap(value[i]);
            s.write(v.get(), sizeof(T) * count);
        }
    }

    /** \brief Reads <tt>count</tt> values of type T from stream <tt>s</tt>,
     * starting at its current position.
     * Note: <tt>count</tt> is the number of values, <b>not</b> a size in bytes.
     *
     * Support for additional types can be added in any header file by
     * declaring a template specialization for your type.
     */
    static void read(Stream &s, T *value, size_t count, bool swap) {
        s.read(value, sizeof(T) * count);
        if (unlikely(swap)) {
            for (size_t i = 0; i < count; ++i)
                value[i] = detail::swap(value[i]);
        }
    }
};

template <> struct serialization_helper<std::string> {
    static std::string type_id() { return "S"; }

    static void write(Stream &s, const std::string *value, size_t count, bool swap) {
        for (size_t i = 0; i < count; ++i) {
            uint32_t length = (uint32_t) value->length();
            serialization_helper<uint32_t>::write(s, &length, 1, swap);
            serialization_helper<char>::write(s, value->data(),
                                              value->length(), swap);
            value++;
        }
    }

    static void read(Stream &s, std::string *value, size_t count, bool swap) {
        for (size_t i = 0; i < count; ++i) {
            uint32_t length = 0;
            serialization_helper<uint32_t>::read(s, &length, 1, swap);
            value->resize(length);
            serialization_helper<char>::read(s, const_cast<char *>(value->data()),
                                             length, swap);
            value++;
        }
    }
};

template <typename T1, typename T2> struct serialization_helper<std::pair<T1, T2>> {
    static std::string type_id() {
        return "P" +
               serialization_helper<T1>::type_id() +
               serialization_helper<T2>::type_id();
    }

    static void write(Stream &s, const std::pair<T1, T1> *value, size_t count, bool swap) {
        std::unique_ptr<T1> first (new T1[count]);
        std::unique_ptr<T2> second(new T2[count]);

        for (size_t i = 0; i < count; ++i) {
            first.get()[i]  = value[i].first;
            second.get()[i] = value[i].second;
        }

        serialization_helper<T1>::write(s, first.get(), count, swap);
        serialization_helper<T2>::write(s, second.get(), count, swap);
    }

    static void read(Stream &s, std::pair<T1, T1> *value, size_t count, bool swap) {
        std::unique_ptr<T1> first (new T1[count]);
        std::unique_ptr<T2> second(new T2[count]);

        serialization_helper<T1>::read(s, first.get(), count, swap);
        serialization_helper<T2>::read(s, second.get(), count, swap);

        for (size_t i = 0; i < count; ++i) {
            value[i].first = first.get()[i];
            value[i].second = second.get()[i];
        }
    }
};

template <typename T> struct serialization_helper<std::vector<T>> {
    static std::string type_id() {
        return "V" + serialization_helper<T>::type_id();
    }

    static void write(Stream &s, const std::vector<T> *value, size_t count, bool swap) {
        for (size_t i = 0; i < count; ++i) {
            size_t size = value->size();
            serialization_helper<size_t>::write(s, &size, 1, swap);
            serialization_helper<T>::write(s, value->data(), size, swap);
            value++;
        }
    }

    static void read(Stream &s, std::vector<T> *value, size_t count, bool swap) {
        for (size_t i = 0; i < count; ++i) {
            size_t size = 0;
            serialization_helper<size_t>::read(s, &size, 1, swap);
            value->resize(size);
            serialization_helper<T>::read(s, value->data(), size, swap);
            value++;
        }
    }
};

template <typename T> struct serialization_helper<std::set<T>> {
    static std::string type_id() {
        return "S" + serialization_helper<T>::type_id();
    }

    static void write(Stream &s, const std::set<T> *value, size_t count, bool swap) {
        for (size_t i = 0; i < count; ++i) {
            std::vector<T> temp(value->size());
            size_t idx = 0;
            for (auto it = value->begin(); it != value->end(); ++it)
                temp[idx++] = *it;
            serialization_helper<std::vector<T>>::write(s, &temp, 1, swap);
            value++;
        }
    }

    static void read(Stream &s, std::set<T> *value, size_t count, bool swap) {
        for (size_t i = 0; i < count; ++i) {
            std::vector<T> temp;
            serialization_helper<std::vector<T>>::read(s, &temp, 1, swap);
            value->clear();
            for (auto k : temp)
                value->insert(k);
            value++;
        }
    }
};

NAMESPACE_END(detail)

NAMESPACE_END(mitsuba)
