#include <mitsuba/core/dstream.h>

NAMESPACE_BEGIN(mitsuba)

DummyStream::DummyStream()
    : Stream(), m_size(0), m_pos(0), m_is_closed(false) { }

void DummyStream::close() { m_is_closed = true; };

bool DummyStream::is_closed() const { return m_is_closed; };

void DummyStream::read(void *, size_t) {
    /// Always throws, since DummyStream is write-only.
    Throw("DummyStream does not support reading.");
}

void DummyStream::write(const void *, size_t size) {
    /// Does not actually write anything, only updates the stream's position and size.
    if (is_closed())
        Throw("Attempted to write to a closed stream: %s", to_string());

    m_size = std::max(m_size, m_pos + size);
    m_pos += size;
}

void DummyStream::seek(size_t pos) {
    /* Updates the current position in the stream.
      Even though the <tt>DummyStream</tt> doesn't write anywhere, position is
      taken into account to accurately compute the size of the stream. */
    m_pos = pos;
}

void DummyStream::truncate(size_t size) {
    /*  Simply sets the current size of the stream.
   The position is updated to <tt>min(old_position, size)</tt>. */
    m_size = size;  // No underlying data, so there's nothing else to do.
    m_pos = std::min(m_pos, size);
}

size_t DummyStream::tell() const { return m_pos; }
size_t DummyStream::size() const { return m_size; }
void DummyStream::flush() { /* Nothing to do */ }
bool DummyStream::can_write() const { return !is_closed(); }
bool DummyStream::can_read() const { return false; }

MTS_IMPLEMENT_CLASS(DummyStream, Stream)

NAMESPACE_END(mitsuba)
