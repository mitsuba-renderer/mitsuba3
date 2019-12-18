#include <mitsuba/core/fstream.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/logger.h>
#include <sstream>
#include <fstream>

namespace fs = mitsuba::filesystem;

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(detail)

inline std::ios::openmode ios_flag(FileStream::EMode mode) {
    switch (mode) {
        case FileStream::ERead:
            return std::ios::binary | std::ios::in;
        case FileStream::EReadWrite:
            return std::ios::binary | std::ios::in | std::ios::out;
        case FileStream::ETruncReadWrite:
            return std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc;
        default:
            Throw("Internal error");
    }
}

NAMESPACE_END(detail)

FileStream::FileStream(const fs::path &p, EMode mode)
    : Stream(), m_mode(mode), m_path(p), m_file(new std::fstream) {

    m_file->open(p.string(), detail::ios_flag(mode));

    if (!m_file->good())
        Throw("\"%s\": I/O error while attempting to open file: %s",
              m_path.string(), strerror(errno));
}

FileStream::~FileStream() {
    close();
}

void FileStream::close() {
    m_file->close();
};

bool FileStream::is_closed() const {
    return !m_file->is_open();
};

void FileStream::read(void *p, size_t size) {
    m_file->read((char *) p, size);

    if (unlikely(!m_file->good())) {
        bool eof = m_file->eof();
        size_t gcount = m_file->gcount();
        m_file->clear();
        if (eof)
            throw EOFException(tfm::format("\"%s\": read %zu out of %zu bytes",
                                           m_path.string(), gcount, size), gcount);
        else
            Throw("\"%s\": I/O error while attempting to read %zu bytes: %s",
                  m_path.string(), size, strerror(errno));
    }
}

void FileStream::write(const void *p, size_t size) {
    m_file->write((char *) p, size);

    if (unlikely(!m_file->good())) {
        m_file->clear();
        Throw("\"%s\": I/O error while attempting to write %zu bytes: %s",
              m_path.string(), size, strerror(errno));
    }
}

void FileStream::truncate(size_t size) {
    if (m_mode == ERead) {
        Throw("\"%s\": attempting to truncate a read-only FileStream",
              m_path.string());
    }

    flush();
    const size_t old_pos = tell();
#if defined(__WINDOWS__)
    // Windows won't allow a resize if the file is open
    close();
#else
    seek(0);
#endif

    fs::resize_file(m_path, size);

#if defined(__WINDOWS__)
    m_file->open(m_path, detail::ios_flag(EReadWrite));
    if (!m_file->good())
        Throw("\"%s\": I/O error while attempting to open file: %s",
              m_path.string(), strerror(errno));
#endif

    seek(std::min(old_pos, size));
}

void FileStream::seek(size_t pos) {
    m_file->seekg(static_cast<std::ios::pos_type>(pos));

    if (unlikely(!m_file->good()))
        Throw("\"%s\": I/O error while attempting to seek to offset %zu: %s",
              m_path.string(), pos, strerror(errno));
}

size_t FileStream::tell() const {
    std::ios::pos_type pos = m_file->tellg();
    if (unlikely(pos == std::ios::pos_type(-1)))
        Throw("\"%s\": I/O error while attempting to determine "
              "position in file", m_path.string());
    return static_cast<size_t>(pos);
}

void FileStream::flush() {
    m_file->flush();
    if (unlikely(!m_file->good())) {
        m_file->clear();
        Throw("\"%s\": I/O error while attempting flush "
              "file stream: %s", m_path.string(), strerror(errno));
    }
}

size_t FileStream::size() const {
    return fs::file_size(m_path);
}

std::string FileStream::read_line() {
    std::string result;
    if (!std::getline(*m_file, result))
        Log(Error, "\"%s\": I/O error while attempting to read a line of text: %s", m_path.string(),
            strerror(errno));
    return result;
}

std::string FileStream::to_string() const {
    std::ostringstream oss;

    oss << class_()->name() << "[" << std::endl;
    if (is_closed()) {
        oss << "  closed" << std::endl;
    } else {
        size_t pos = (size_t) -1;
        try { pos = tell(); } catch (...) { }
        oss << "  path = \"" << m_path.string() << "\"" << "," << std::endl
            << "  host_byte_order = " << host_byte_order() << "," << std::endl
            << "  byte_order = " << byte_order() << "," << std::endl
            << "  can_read = " << can_read() << "," << std::endl
            << "  can_write = " << can_write() << "," << std::endl
            << "  pos = " << pos << "," << std::endl
            << "  size = " << size() << std::endl;
    }

    oss << "]";

    return oss.str();
}

MTS_IMPLEMENT_CLASS(FileStream, Stream)

NAMESPACE_END(mitsuba)
