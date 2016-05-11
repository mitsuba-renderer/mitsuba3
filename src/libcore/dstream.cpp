#include <mitsuba/core/dstream.h>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

std::string DummyStream::toString() const {
    std::ostringstream oss;
    oss << "DummyStream[" << Stream::toString()
    // TODO: complete with implementation-specific information
    << "]";
    return oss.str();
}

MTS_IMPLEMENT_CLASS(DummyStream, Stream)

NAMESPACE_END(mitsuba)
