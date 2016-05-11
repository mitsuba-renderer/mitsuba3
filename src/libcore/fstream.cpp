#include <mitsuba/core/fstream.h>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

std::string FileStream::toString() const {
    std::ostringstream oss;
    oss << "FileStream[" << Stream::toString()
    // TODO: complete with implementation-specific information
        << "]";
    return oss.str();
}

NAMESPACE_END(mitsuba)
