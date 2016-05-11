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

FileStream::~FileStream() {
    // TODO: release resources
}

MTS_IMPLEMENT_CLASS(FileStream, Stream)

NAMESPACE_END(mitsuba)
