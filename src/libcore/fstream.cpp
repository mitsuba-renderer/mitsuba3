#include <mitsuba/core/fstream.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/logger.h>
#include <sstream>
#include <fstream>

namespace fs = mitsuba::filesystem;

NAMESPACE_BEGIN(mitsuba)

namespace {
inline void open_stream(std::fstream &f, const fs::path &p, bool write_enabled, bool file_exists) {
    auto mode = std::ios::binary | std::ios::in;
    if (write_enabled) {
        mode |= std::ios::out;
        // If the file doesn't exist, add truncate mode to create it
        if (!file_exists) {
            mode |= std::ios::trunc;
        }
    }

    f.open(p.string(), mode);
}
}  // end anonymous namespace

FileStream::FileStream(const fs::path &p, bool write_enabled)
    : Stream(), m_path(p), m_file(new std::fstream), m_write_enabled(write_enabled) {
    const bool file_exists = fs::exists(p);
    if (!m_write_enabled && !file_exists) {
        Log(EError, "\"%s\": tried to open a read-only FileStream pointing to"
                    " a file that cannot be opened.", m_path.string());
    }

    open_stream(*m_file, m_path, m_write_enabled, file_exists);

    if (!m_file->good()) {
        Log(EError, "\"%s\": I/O error while attempting to open the file.",
            m_path.string());
    }
}

FileStream::~FileStream() {
    close();
}

std::string FileStream::to_string() const {
    std::ostringstream oss;

    oss << class_()->name() << "[" << std::endl;
    if (is_closed()) {
        oss << "  closed" << std::endl;
    } else {
        oss << "  path = \"" << m_path.string() << "\"" << "," << std::endl
            << "  host_byte_order = " << host_byte_order() << "," << std::endl
            << "  byte_order = " << byte_order() << "," << std::endl
            << "  can_read = " << can_read() << "," << std::endl
            << "  can_write = " << can_write() << "," << std::endl
            << "  pos = " << tell() << "," << std::endl
            << "  size = " << size() << std::endl;
    }

    oss << "]";

    return oss.str();
}

void FileStream::close() {
    m_file->close();
};

bool FileStream::is_closed() const {
    return !m_file->is_open();
};

void FileStream::read(void *p, size_t size) {
    if (!can_read()) {
        Log(EError, "\"%s\": attempted to read from a write-only FileStream",
            m_path.string());
    }

    m_file->read((char *)p, size);

    if (!m_file->good()) {
        Log(EError, "\"%s\": I/O error while attempting to read %llu bytes",
            m_path.string(), size);
    }
}

void FileStream::write(const void *p, size_t size) {
    if (!can_write()) {
        Log(EError, "\"%s\": attempted to write to a read-only FileStream",
            m_path.string());
    }

    m_file->write((char *) p, size);

    if (!m_file->good()) {
        Log(EError, "\"%s\": I/O error while attempting to write %llu bytes",
            m_path.string(), size);
    }
}

void FileStream::seek(size_t pos) {
    if (m_write_enabled)
        m_file->seekg(static_cast<std::streamoff>(pos));
    else
        m_file->seekp(static_cast<std::streamoff>(pos));

    if (!m_file->good()) {
        Log(EError, "\"%s\": I/O error while attempting to seek to offset %llu",
            m_path.string(), pos);
    }
}

void FileStream::truncate(size_t size) {
    if (!m_write_enabled) {
        Log(EError, "\"%s\": attempting to truncate a read-only FileStream",
            m_path.string());
    }

    flush();
    const auto old_pos = tell();
#if defined(__WINDOWS__)
    // Windows won't allow a resize if the file is open
    m_file->close();
#else
    seek(0);
#endif

    fs::resize_file(m_path, size);

#if defined(__WINDOWS__)
    open_stream(*m_file, m_path, m_write_enabled, true);
#endif

    seek(std::min(old_pos, size));

    if (!m_file->good()) {
        Log(EError, "\"%s\": I/O error while attempting to truncate file to size %llu",
            m_path.string(), size);
    }
}

size_t FileStream::tell() const {
    const auto pos = (m_write_enabled ? m_file->tellg() : m_file->tellp());
    if (pos < 0) {
        Log(EError, "\"%s\": I/O error while attempting to determine position in file",
            m_path.string());
    }
    return static_cast<size_t>(pos);
}

void FileStream::flush() {
    m_file->flush();
}

size_t FileStream::size() const {
    return fs::file_size(m_path);
}

std::string FileStream::read_line() {
    std::string result;
    if (!std::getline(*m_file, result))
        Log(EError, "\"%s\": I/O error while attempting to read a line of text",
            m_path.string());
    return result;
}

MTS_IMPLEMENT_CLASS(FileStream, Stream)

NAMESPACE_END(mitsuba)
