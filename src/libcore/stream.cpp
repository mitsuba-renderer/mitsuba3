#include <mitsuba/core/stream.h>
#include <mitsuba/core/dstream.h>
#include <sstream>
#include <cctype>

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(detail)
static Stream::EByteOrder byte_order() {
    union {
        uint8_t  char_value[2];
        uint16_t short_value;
    };
    char_value[0] = 1;
    char_value[1] = 0;

    if (short_value == 1)
        return Stream::ELittleEndian;
    else
        return Stream::EBigEndian;
}
NAMESPACE_END(detail)

const Stream::EByteOrder Stream::m_host_byte_order = detail::byte_order();

// -----------------------------------------------------------------------------

Stream::Stream()
    : m_byte_order(m_host_byte_order) { }

Stream::~Stream() { }

void Stream::set_byte_order(EByteOrder value) {
    m_byte_order = value;
}

// -----------------------------------------------------------------------------

std::string Stream::to_string() const {
    std::ostringstream oss;

    oss << class_()->name() << "[" << std::endl;
    if (is_closed()) {
        oss << "  closed" << std::endl;
    } else {
        oss << "  host_byte_order = " << m_host_byte_order << "," << std::endl
            << "  byte_order = " << m_byte_order << "," << std::endl
            << "  can_read = " << can_read() << "," << std::endl
            << "  can_write = " << can_write() << "," << std::endl
            << "  pos = " << tell() << "," << std::endl
            << "  size = " << size() << std::endl;
    }

    oss << "]";

    return oss.str();
}

std::ostream &operator<<(std::ostream &os, const Stream::EByteOrder &value) {
    switch (value) {
        case Stream::ELittleEndian: os << "little-endian"; break;
        case Stream::EBigEndian: os << "big-endian"; break;
        default: os << "invalid"; break;
    }
    return os;
}

void Stream::write_line(const std::string &text) {
    write(text.data(), text.length() * sizeof(char));
    write('\n');
}

std::string Stream::read_line() {
    std::string result;
    result.reserve(80);

    try {
        do {
            char data;
            read(&data, sizeof(char));
            if (data == '\n')
                break;
            else if (data != '\r')
                result += data;
        } while (true);
    } catch (...) {
        if (tell() == size() && !result.empty())
            return result;
        throw;
    }

    return result;
}

std::string Stream::read_token() {
    std::string result;

    try {
        do {
            char data;
            read(&data, sizeof(char));
            if (std::isspace(data)) {
                if (result.empty())
                    continue;
                else
                    break;
            }
            result += data;
        } while (true);
    } catch (...) {
        if (tell() == size() && !result.empty())
            return result;
        throw;
    }

    return result;
}

void Stream::skip(size_t amount) {
    seek(tell() + amount);
}

MTS_IMPLEMENT_CLASS(Stream, Object)

NAMESPACE_END(mitsuba)
