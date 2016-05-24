#pragma once

#include <mitsuba/core/stream.h>
#include <zlib.h>

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN()
/// Buffer size used to communicate with zlib. The larger, the better.
constexpr size_t kZStreamBufferSize = 32768;
NAMESPACE_END()

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
     *
     * TODO: clarify ownership
     */
    ZStream(Stream *childStream, EStreamType streamType = EDeflateStream,
            int level = Z_DEFAULT_COMPRESSION);

    /// Returns a string representation
    std::string toString() const override;

    // =========================================================================
    //! @{ \name Compression stream-specific features
    // =========================================================================

    /// Returns the child stream of this compression stream
    inline const Stream *getChildStream() const { return m_childStream.get(); }

    /// Returns the child stream of this compression stream
    inline Stream *getChildStream() { return m_childStream; }

    //! @}
    // =========================================================================

    // =========================================================================
    //! @{ \name Implementation of the Stream interface
    // =========================================================================
protected:

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

public:

    /// Unsupported. Always throws.
    virtual void seek(size_t) override {
      Log(EError, "seek(): unsupported in a ZLIB stream!");
    }

    //// Unsupported. Always throws.
    virtual void truncate(size_t) override {
      Log(EError, "truncate(): unsupported in a ZLIB stream!");
    }

    /// Unsupported. Always throws.
    virtual size_t getPos() override {
        Log(EError, "getPos(): unsupported in a ZLIB stream!");
        return 0;
    }

    /// Unsupported. Always throws.
    virtual size_t getSize() override {
        Log(EError, "getSize(): unsupported in a ZLIB stream!");
        return 0;
    }

    /// Unsupported. Always throws.
    virtual void flush() override {
        Log(EError, "flush(): not implemented!");
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
    /// Destructor
    virtual ~ZStream();

private:
    ref<Stream> m_childStream;
    z_stream m_deflateStream, m_inflateStream;
    uint8_t m_deflateBuffer[kZStreamBufferSize];
    uint8_t m_inflateBuffer[kZStreamBufferSize];
    bool m_didWrite;
};

NAMESPACE_END(mitsuba)
