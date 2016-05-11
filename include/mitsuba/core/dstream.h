#pragma once

#include <mitsuba/core/stream.h>
#include "logger.h"

NAMESPACE_BEGIN(mitsuba)

/** \brief \ref Stream implementation that never writes to disk, but keeps track
 * of the size of the content being written.
 * It can be used, for example, to measure the precise amount of memory needed
 * to store serialized content.
 */
class MTS_EXPORT_CORE DummyStream : public Stream {
public:

    DummyStream()
        : Stream(true, false) { }

    /// Returns a string representation
    std::string toString() const override;

    // =============================================================
    //! @{ \name Implementation of the Stream interface
    // =============================================================
protected:

    /// Does not actually write anything, only updates the stream's position and size.
    virtual void write(const void *, size_t size) override {
        m_size += size - (m_size - m_pos);
        m_pos += size;
    }

public:

    virtual void seek(size_t pos) override;

    virtual void truncate(size_t size) override {
        m_size = size;  // Nothing else to do
    }

    virtual size_t getPos() const override { return m_pos; }

    virtual size_t getSize() const override { return m_size; }

    virtual void flush() override { /* Nothing to do */ }

    virtual bool canWrite() const override { return true; }

    virtual bool canRead() const override { return false; }

    //! @}
    // =============================================================

    MTS_DECLARE_CLASS()

protected:

    /// Trivial destructor.
    virtual ~DummyStream() = default;

private:
    /// Size of all data written to the stream
    size_t m_size;
    /** \brief Current position in the "virtual" stream (even though nothing
     * is ever written, we need to maintain consistent positioning).
     */
    size_t m_pos;

};

NAMESPACE_END(mitsuba)
