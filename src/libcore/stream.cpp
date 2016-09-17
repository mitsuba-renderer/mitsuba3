#include <mitsuba/core/stream.h>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(detail)
static Stream::EByteOrder byteOrder() {
    union {
        uint8_t  charValue[2];
        uint16_t shortValue;
    };
    charValue[0] = 1;
    charValue[1] = 0;

    if (shortValue == 1)
        return Stream::ELittleEndian;
    else
        return Stream::EBigEndian;
}
NAMESPACE_END(detail)

const Stream::EByteOrder Stream::m_hostByteOrder = detail::byteOrder();

// -----------------------------------------------------------------------------

Stream::Stream()
    : m_byteOrder(m_hostByteOrder) { }

Stream::~Stream() { }

void Stream::setByteOrder(EByteOrder value) {
    m_byteOrder = value;
}

// -----------------------------------------------------------------------------

std::string Stream::toString() const {
    std::ostringstream oss;

    oss << "hostByteOrder=" << m_hostByteOrder
        << ", byteOrder=" << m_byteOrder
        << ", isClosed=" << isClosed();

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

void Stream::writeLine(const std::string &text) {
    write(text.data(), text.length());
    write('\n');
}

std::string Stream::readLine() {
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
        if (tell() != size() || result.empty())
            throw;
    }
    return result;
}

MTS_IMPLEMENT_CLASS(Stream, Object)

NAMESPACE_END(mitsuba)
