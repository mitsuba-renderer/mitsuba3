#include <mitsuba/core/dstream.h>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

std::string DummyStream::toString() const {
    std::ostringstream oss;
    oss << "DummyStream[" << Stream::toString()
        << "size=" << m_size
        << ",pos=" << m_pos
        << "]";
    return oss.str();
}

void DummyStream::seek(size_t pos) {
    if (pos >= m_size) {
        Log(EError, "Tried to seek beyond length of the file (%llu >= %llu)",
            pos, m_size);
    }
    m_pos = pos;
}

MTS_IMPLEMENT_CLASS(DummyStream, Stream)

NAMESPACE_END(mitsuba)
