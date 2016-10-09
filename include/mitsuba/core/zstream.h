#pragma once

#include <mitsuba/core/stream.h>
#include <zlib.h>

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(detail)
/// Buffer size used to communicate with zlib. The larger, the better.
constexpr size_t kZStreamBufferSize = 32768;
NAMESPACE_END(detail)

/**
 * \brief Transparent compression/decompression stream based on \c zlib.
 *
 * This class transparently decompresses and compresses reads and writes
 * to a nested stream, respectively.
 */
class MTS_EXPORT_CORE ZStream : public Stream {
public:
    enum EStreamType {
        EDeflateStream, ///< A raw deflate stream
        EGZipStream ///< A gzip-compatible stream
    };

    /** \brief Creates a new compression stream with the given underlying stream.
     * This new instance takes ownership of the child stream. The child stream
     * must outlive the ZStream.
     */
    ZStream(Stream *childStream, EStreamType streamType = EDeflateStream,
            int level = Z_DEFAULT_COMPRESSION);

    /// Returns a string representation
    std::string toString() const override;

    /** \brief Closes the stream, but not the underlying child stream.
     * No further read or write operations are permitted.
     *
     * This function is idempotent.
     * It is called automatically by the destructor.
     */
    virtual void close() override;

    /// Whether the stream is closed (no read or write are then permitted).
    virtual bool isClosed() const override { return m_childStream && m_childStream->isClosed(); };

    // =========================================================================
    //! @{ \name Compression stream-specific features
    // =========================================================================

    /// Returns the child stream of this compression stream
    const Stream *childStream() const { return m_childStream.get(); }

    /// Returns the child stream of this compression stream
    Stream *childStream() { return m_childStream; }

    //! @}
    // =========================================================================

    // =========================================================================
    //! @{ \name Implementation of the Stream interface
    // =========================================================================

    /**
     * \brief Reads a specified amount of data from the stream, decompressing
     * it first using ZLib.
     * Throws an exception when the stream ended prematurely.
     */
    virtual void read(void *p, size_t size) override;

    /**
     * \brief Writes a specified amount of data into the stream, compressing
     * it first using ZLib.
     * Throws an exception when not all data could be written.
     */
    virtual void write(const void *p, size_t size) override;

    /// Flushes any buffered data
    virtual void flush() override;

    /// Unsupported. Always throws.
    virtual void seek(size_t) override {
        Log(EError, "seek(): unsupported in a ZLIB stream!");
    }

    //// Unsupported. Always throws.
    virtual void truncate(size_t) override {
        Log(EError, "truncate(): unsupported in a ZLIB stream!");
    }

    /// Unsupported. Always throws.
    virtual size_t tell() const override {
        Log(EError, "tell(): unsupported in a ZLIB stream!");
        return 0;
    }

    /// Unsupported. Always throws.
    virtual size_t size() const override {
        Log(EError, "size(): unsupported in a ZLIB stream!");
        return 0;
    }

    /// Can we write to the stream?
    virtual bool canWrite() const override {
        return m_childStream->canWrite();
    }

    /// Can we read from the stream?
    virtual bool canRead() const override {
        return m_childStream->canRead();
    }

    //! @}
    // =========================================================================

    MTS_DECLARE_CLASS()

protected:
    /// Protected destructor
    virtual ~ZStream();

private:
    ref<Stream> m_childStream;
    z_stream m_deflateStream, m_inflateStream;
    uint8_t m_deflateBuffer[detail::kZStreamBufferSize];
    uint8_t m_inflateBuffer[detail::kZStreamBufferSize];
    bool m_didWrite;
};

NAMESPACE_END(mitsuba)
