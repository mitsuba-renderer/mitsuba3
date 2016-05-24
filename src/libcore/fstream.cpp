#include <mitsuba/core/fstream.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/logger.h>
#include <sstream>
#include <fstream>

namespace fs = mitsuba::filesystem;
using path = fs::path;

NAMESPACE_BEGIN(mitsuba)

FileStream::FileStream(const path &p, bool writeEnabled)
    : Stream(), m_path(p), m_file(new std::fstream), m_writeEnabled(writeEnabled) {
    const bool fileExists = fs::exists(p);
    if (!m_writeEnabled && !fileExists) {
        Log(EError, "\"%s\": tried to open a read-only FileStream pointing to"
                    " a file that cannot be opened.",
            m_path.string().c_str());
    }

    auto mode = std::ios::binary | std::ios::in;
    if (writeEnabled) {
        mode |= std::ios::out;
        // If the file doesn't exist, add truncate mode to create it
        if (!fileExists) {
            mode |= std::ios::trunc;
        }
    }

    m_file->open(p.string(), mode);

    if (!m_file->good()) {
        Log(EError, "\"%s\": I/O error while attempting to open the file.",
            m_path.string().c_str());
    }
}

FileStream::~FileStream() {
    if (m_file->is_open()) {
        m_file->close();
    }
}

std::string FileStream::toString() const {
    std::ostringstream oss;
    oss << "FileStream[" << Stream::toString()
        << ", path=" << m_path.string()
        << ", size=" << getSize()
        << ", pos=" << getPos()
        << ", writeEnabled=" << (m_writeEnabled ? "true" : "false")
        << "]";
    return oss.str();
}

void FileStream::read(void *p, size_t size) {
    if (!canRead()) {
        Log(EError, "\"%s\": attempted to read from a write-only FileStream",
            m_path.string().c_str());
    }

    m_file->read((char *)p, size);

    if (!m_file->good()) {
        Log(EError, "\"%s\": I/O error while attempting to read %llu bytes",
            m_path.string().c_str(), size);
    }
}

void FileStream::write(const void *p, size_t size) {
    if (!canWrite()) {
        Log(EError, "\"%s\": attempted to write to a read-only FileStream",
            m_path.string().c_str());
    }

    m_file->write((char *)p, size);

    if (!m_file->good()) {
        Log(EError, "\"%s\": I/O error while attempting to write %llu bytes",
            m_path.string().c_str(), size);
    }
}

void FileStream::seek(size_t pos) {
    if (m_writeEnabled)
        m_file->seekg(static_cast<std::streamoff>(pos));
    else
        m_file->seekp(static_cast<std::streamoff>(pos));

    if (!m_file->good()) {
        Log(EError, "\"%s\": I/O error while attempting to seek to offset %llu",
            m_path.string().c_str(), pos);
    }
}

void FileStream::truncate(size_t size) {
    if (!m_writeEnabled) {
        Log(EError, "\"%s\": attempting to truncate a read-only FileStream",
            m_path.string().c_str());
    }

    flush();
    const auto old_pos = getPos();
    seek(0);
    // TODO: is it safe to change the underlying file while the fstream is still open?
    fs::resize_file(m_path, size);
    seek(std::min(old_pos, size));

    if (!m_file->good()) {
        Log(EError, "\"%s\": I/O error while attempting to truncate file to size %llu",
            m_path.string().c_str(), size);
    }
}

size_t FileStream::getPos() const {
    const auto pos = (m_writeEnabled ? m_file->tellg() : m_file->tellp());
    if (pos < 0) {
        Log(EError, "\"%s\": I/O error while attempting to determine position in file",
            m_path.string().c_str());
    }
    return static_cast<size_t>(pos);
}

void FileStream::flush() {
    m_file->flush();
}

MTS_IMPLEMENT_CLASS(FileStream, Stream)

NAMESPACE_END(mitsuba)
