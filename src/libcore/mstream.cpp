#include <mitsuba/core/mstream.h>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

std::string MemoryStream::toString() const {
    std::ostringstream oss;
    oss << "MemoryStream[" << Stream::toString()
    // TODO: complete with implementation-specific information
    << "]";
    return oss.str();
}


NAMESPACE_END(mitsuba)
