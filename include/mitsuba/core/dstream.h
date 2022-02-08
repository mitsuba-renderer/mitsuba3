#pragma once

#include <mitsuba/core/stream.h>
#include <mitsuba/core/logger.h>

NAMESPACE_BEGIN(mitsuba)

/** \brief \ref Stream implementation that never writes to disk, but keeps track
 * of the size of the content being written.
 * It can be used, for example, to measure the precise amount of memory needed
 * to store serialized content.
 */
class MI_EXPORT_LIB DummyStream : public Stream {
public:
    DummyStream();

    /** \brief Closes the stream.
     * No further read or write operations are permitted.
     *
     * This function is idempotent.
     * It may be called automatically by the destructor.
     */
    virtual void close() override;

    /// Whether the stream is closed (no read or write are then permitted).
    virtual bool is_closed() const override;

    // =========================================================================
    //! @{ \name Implementation of the Stream interface
    // =========================================================================

    virtual void read(void *, size_t) override;
    virtual void write(const void *, size_t size) override;
    virtual void seek(size_t pos) override;
    virtual void truncate(size_t size) override;
    virtual size_t tell() const override;
    virtual size_t size() const override;
    virtual void flush() override;
    virtual bool can_write() const override;
    virtual bool can_read() const override;

    //! @}
    // =========================================================================

    MI_DECLARE_CLASS()
protected:

    /// Protected destructor.
    virtual ~DummyStream() = default;

private:
    /// Size of all data written to the stream
    size_t m_size;
    /** \brief Current position in the "virtual" stream (even though nothing
     * is ever written, we need to maintain consistent positioning).
     */
    size_t m_pos;
    /// Whether the stream has been closed.
    bool m_is_closed;

};

NAMESPACE_END(mitsuba)
