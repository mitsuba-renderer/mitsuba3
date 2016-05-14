#include <mitsuba/core/dummy_stream.h>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

std::string DummyStream::toString() const {
    std::ostringstream oss;
    oss << "DummyStream[" << Stream::toString()
        << ", size=" << m_size
        << ", pos=" << m_pos
        << "]";
    return oss.str();
}

MTS_IMPLEMENT_CLASS(DummyStream, Stream)

NAMESPACE_END(mitsuba)
