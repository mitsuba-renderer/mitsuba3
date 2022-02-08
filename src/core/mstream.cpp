#include <mitsuba/core/mstream.h>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

MemoryStream::MemoryStream(size_t capacity)
    : Stream(), m_capacity(0), m_size(0), m_pos(0), m_owns_buffer(true),
      m_data(nullptr), m_is_closed(false) {
    resize(capacity);
}

MemoryStream::MemoryStream(void *ptr, size_t size)
    : Stream(), m_capacity(size), m_size(size), m_pos(0), m_owns_buffer(false),
      m_data(reinterpret_cast<uint8_t *>(ptr)), m_is_closed(false) { }

MemoryStream::~MemoryStream() {
    if (m_data != nullptr && m_owns_buffer)
        std::free(m_data);
}

void MemoryStream::read(void *p, size_t size) {
    if (is_closed())
        Throw("Attempted to read from a closed stream: %s", to_string());

    if (m_pos + size > m_size) {
        const auto old_pos = m_pos;
        // Use signed difference since `m_pos` might be beyond `m_size`
        int64_t size_read = m_size - static_cast<int64_t>(m_pos);
        if (size_read > 0) {
            memcpy(p, m_data + m_pos, static_cast<size_t>(size_read));
            m_pos += static_cast<size_t>(size_read);
        }
        Log(Error, "Reading over the end of a memory stream!"
                   " (amount requested = %llu, amount actually read = %llu,"
                   " total size of the stream = %llu, previous position = %llu)",
            size, size_read, m_size, old_pos);
    }
    memcpy(p, m_data + m_pos, size);
    m_pos += size;
}

void MemoryStream::write(const void *p, size_t size) {
    if (is_closed())
        Throw("Attempted to write to a closed stream: %s", to_string());

    size_t endPos = m_pos + size;
    if (endPos > m_size) {
        if (endPos > m_capacity) {
            // Double capacity until it will fit `endPos`, à la `std::vector`
            auto newSize = m_capacity;
            do { newSize *= 2; } while (endPos > newSize);
            resize(newSize);
        }
        m_size = endPos;
    }
    memcpy(m_data + m_pos, p, size);
    m_pos = endPos;
}

void MemoryStream::resize(size_t size) {
    if (!m_owns_buffer)
        Throw("Tried to resize a buffer, which doesn't "
              "belong to this MemoryStream instance!");

    if (m_data == nullptr)
        m_data = reinterpret_cast<uint8_t *>(std::malloc(size));
    else
        m_data = reinterpret_cast<uint8_t *>(std::realloc(m_data, size));

    if (size > m_capacity)
        memset(m_data + m_capacity, 0, size - m_capacity);

    m_capacity = size;
}

void MemoryStream::truncate(size_t size) {
    m_size = size;
    resize(size);
    if (m_pos > m_size)
        m_pos = m_size;
}

std::string MemoryStream::to_string() const {
    std::ostringstream oss;

    oss << class_()->name() << "[" << std::endl;
    if (is_closed()) {
        oss << "  closed" << std::endl;
    } else {
        oss << "  host_byte_order = " << host_byte_order() << "," << std::endl
            << "  byte_order = " << byte_order() << "," << std::endl
            << "  can_read = " << can_read() << "," << std::endl
            << "  can_write = " << can_write() << "," << std::endl
            << "  owns_buffer = " << owns_buffer() << "," << std::endl
            << "  capacity = " << capacity() << "," << std::endl
            << "  pos = " << tell() << "," << std::endl
            << "  size = " << size() << std::endl;
    }

    oss << "]";

    return oss.str();
}

MTS_IMPLEMENT_CLASS(MemoryStream, Stream)

NAMESPACE_END(mitsuba)
