#pragma once

#include <mitsuba/core/stream.h>

extern "C" {
    struct z_stream_s;
    typedef struct z_stream_s z_stream;
};

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
class MI_EXPORT_LIB ZStream : public Stream {
public:

    enum EStreamType {
        EDeflateStream, /// A raw deflate stream
        EGZipStream /// A gzip-compatible stream
    };

    using Stream::read;
    using Stream::write;

    /** \brief Creates a new compression stream with the given underlying stream.
     * This new instance takes ownership of the child stream. The child stream
     * must outlive the ZStream.
     */
    ZStream(Stream *child_stream, EStreamType stream_type = EDeflateStream,
            int level = -1);

    /// Returns a string representation
    std::string to_string() const override;

    /** \brief Closes the stream, but not the underlying child stream.
     * No further read or write operations are permitted.
     *
     * This function is idempotent.
     * It is called automatically by the destructor.
     */
    virtual void close() override;

    /// Whether the stream is closed (no read or write are then permitted).
    virtual bool is_closed() const override { return m_child_stream && m_child_stream->is_closed(); };

    // =========================================================================
    //! @{ \name Compression stream-specific features
    // =========================================================================

    /// Returns the child stream of this compression stream
    const Stream *child_stream() const { return m_child_stream.get(); }

    /// Returns the child stream of this compression stream
    Stream *child_stream() { return m_child_stream; }

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
        Throw("seek(): unsupported in a ZLIB stream!");
    }

    //// Unsupported. Always throws.
    virtual void truncate(size_t) override {
        Throw("truncate(): unsupported in a ZLIB stream!");
    }

    /// Unsupported. Always throws.
    virtual size_t tell() const override {
        Throw("tell(): unsupported in a ZLIB stream!");
        return 0;
    }

    /// Unsupported. Always throws.
    virtual size_t size() const override {
        Throw("size(): unsupported in a ZLIB stream!");
        return 0;
    }

    /// Can we write to the stream?
    virtual bool can_write() const override {
        return m_child_stream->can_write();
    }

    /// Can we read from the stream?
    virtual bool can_read() const override {
        return m_child_stream->can_read();
    }

    //! @}
    // =========================================================================

    MI_DECLARE_CLASS()
protected:
    /// Protected destructor
    virtual ~ZStream();

private:
    ref<Stream> m_child_stream;
    std::unique_ptr<z_stream> m_deflate_stream, m_inflate_stream;
    uint8_t m_deflate_buffer[detail::kZStreamBufferSize];
    uint8_t m_inflate_buffer[detail::kZStreamBufferSize];
    bool m_did_write;
};

NAMESPACE_END(mitsuba)
