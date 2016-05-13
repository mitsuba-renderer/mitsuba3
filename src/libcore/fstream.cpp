#include <mitsuba/core/fstream.h>
#include <sstream>
#include <mitsuba/core/logger.h>

namespace fs = mitsuba::filesystem;
using path = fs::path;

NAMESPACE_BEGIN(mitsuba)

FileStream::FileStream(path p, bool writeEnabled)
: Stream(writeEnabled), m_path(p) {
    const auto mode = std::ios_base::app | (m_writeMode ? std::ios_base::out
                                                        : std::ios_base::in);
    m_file.open(p.string(), mode);

    if (!m_file.good()) {
        Log(EError, "\"%s\": I/O error while attempting to seek to open the file.",
            m_path.string().c_str());
    }
}

FileStream::~FileStream() {
    m_file.close();
}

std::string FileStream::toString() const {
    std::ostringstream oss;
    oss << "FileStream[" << Stream::toString()
        << ",path=" << m_path.string()
//        << ",size=" << getSize()
//        << ",pos=" << getPos()
        << "]";
    return oss.str();
}

void FileStream::read(void *p, size_t size) {
    // TODO: handle endianness swap (?)
    m_file.read((char *)p, size);

    if (!m_file.good()) {
        Log(EError, "\"%s\": I/O error while attempting to read %llu bytes",
            m_path.string().c_str(), size);
    }
}

void FileStream::write(const void *p, size_t size) {
    // TODO: handle endianness swap (?)
    m_file.write((char *)p, size);

    if (!m_file.good()) {
        Log(EError, "\"%s\": I/O error while attempting to write %llu bytes",
            m_path.string().c_str(), size);
    }
}

void FileStream::seek(size_t pos) {
    if (m_writeMode)
        m_file.seekg(static_cast<std::streamoff>(pos));
    else
        m_file.seekp(static_cast<std::streamoff>(pos));

    if (!m_file.good()) {
        Log(EError, "\"%s\": I/O error while attempting to seek to offset %llu",
            m_path.string().c_str(), pos);
    }
}

void FileStream::truncate(size_t size) {
    if (!m_writeMode) {
        Log(EError, "\"%s\": attempting to truncate a read-only FileStream",
            m_path.string().c_str());
    }

    // TODO: is it really necessary to close & reopen?
    m_file.close();
    fs::resize_file(m_path, size);
    const auto mode = std::ios_base::app | (m_writeMode ? std::ios_base::out
                                                        : std::ios_base::in);
    m_file.open(m_path.string(), mode);

    if (!m_file.good()) {
        Log(EError, "\"%s\": I/O error while attempting to truncate file to size %llu",
            m_path.string().c_str(), size);
    }
}

size_t FileStream::getPos() {
    // TODO: Y U NO CONST?
    const auto pos = (m_writeMode ? m_file.tellg() : m_file.tellp());
    if (pos < 0) {
        Log(EError, "\"%s\": I/O error while attempting to determine position in file",
            m_path.string().c_str());
    }
    return static_cast<size_t>(pos);
}

MTS_IMPLEMENT_CLASS(FileStream, Stream)

NAMESPACE_END(mitsuba)
