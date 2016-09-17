#pragma once

#include <mitsuba/core/object.h>
#include <vector>
#include "logger.h"

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
 * value passed to \ref setByteOrder(). Whenever \ref hostByteOrder()
 * and \ref byteOrder() disagree, the endianness is swapped.
 *
 * \sa FileStream, MemoryStream, DummyStream
 */
class MTS_EXPORT_CORE Stream : public Object {

protected:
    /* In general, only low-level type serializers should call the stream's
       protected read & write functions. Otherwise, make sure to handle
       endianness swapping explicitly.
    */
    template <typename T> friend struct detail::serialization_helper;

public:
    /// Defines the byte order (endianness) to use in this Stream
    enum EByteOrder {
        EBigEndian = 0,                ///< PowerPC, SPARC, Motorola 68K
        ELittleEndian = 1,             ///< x86, x86_64
        ENetworkByteOrder = EBigEndian ///< Network byte order (an alias for big endian)
    };

    /**
     * \brief Creates a new stream.
     *
     * By default, this function sets the stream byte order
     * to that of the system (i.e. no conversion is performed)
     */
    Stream();

    /// Returns a human-readable desriptor of the stream
    virtual std::string toString() const override;

    /** \brief Closes the stream.
     *
     * No further read or write operations are permitted.
     *
     * This function is idempotent.
     * It may be called automatically by the destructor.
     */
    virtual void close() = 0;

    /// Whether the stream is closed (no read or write are then permitted).
    virtual bool isClosed() const = 0;

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
     * its contents. However, a subsequent write should start at the seeked
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
    virtual bool canWrite() const = 0;

    /// Can we read from the stream?
    virtual bool canRead() const = 0;

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
        helper::read(*this, &value, 1, needsEndiannessSwapping());
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
        helper::write(*this, &value, 1, needsEndiannessSwapping());
    }

    /// Convenience function for reading a line of text from an ASCII file
    virtual std::string readLine();

    /// Convenience function for writing a line of text to an ASCII file
    void writeLine(const std::string &text);

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
    void setByteOrder(EByteOrder byteOrder);

    /// Returns the byte order of this stream.
    EByteOrder byteOrder() const { return m_byteOrder; }

    /// Returns true if we need to perform endianness swapping before writing or reading.
    bool needsEndiannessSwapping() const {
        return m_byteOrder != m_hostByteOrder;
    }

    /// Returns the byte order of the underlying machine.
    static EByteOrder hostByteOrder() { return m_hostByteOrder; }


    /// @}
    // =========================================================================

    MTS_DECLARE_CLASS();

protected:
    /// Destructor
    virtual ~Stream();

    /// Copying is disallowed.
    Stream(const Stream&) = delete;
    void operator=(const Stream&) = delete;

private:
    static const EByteOrder m_hostByteOrder;
    EByteOrder m_byteOrder;
};

extern MTS_EXPORT_CORE std::ostream
    &operator<<(std::ostream &os, const Stream::EByteOrder &value);

NAMESPACE_END(mitsuba)

#include "detail/stream.hpp"
