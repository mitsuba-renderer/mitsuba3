#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/logger.h>

NAMESPACE_BEGIN(mitsuba)

Bitmap::Bitmap(EPixelFormat pFormat, Struct::EType cFormat,
               const Vector2s &size, uint32_t channelCount, uint8_t *data)
    : m_pixelFormat(pFormat), m_componentFormat(cFormat), m_size(size),
      m_data(data), m_channelCount(channelCount), m_ownsData(false) {
    Assert(size.x > 0 && size.y > 0, "Invalid bitmap size");

    if (m_componentFormat == Struct::EUInt8)
        m_gamma = -1.0f; // sRGB by default
    else
        m_gamma = 1.0f; // Linear by default

    updateChannelCount();

    if (!m_data) {
        m_data = std::unique_ptr<uint8_t[], AlignedAllocator>(
            AlignedAllocator::alloc<uint8_t>(bufferSize()));

        m_ownsData = true;
    }
}

Bitmap::~Bitmap() {
    if (!m_ownsData)
        m_data.release();
}

void Bitmap::updateChannelCount() {
    switch (m_pixelFormat) {
        case ELuminance: m_channelCount = 1; break;
        case ELuminanceAlpha: m_channelCount = 2; break;
        case ERGB: m_channelCount = 3; break;
        case ERGBA: m_channelCount = 4; break;
        case EXYZ: m_channelCount = 3; break;
        case EXYZA: m_channelCount = 4; break;
        case EMultiChannel: break;
        default: Throw("Unknown pixel format!");
    }
}

size_t Bitmap::bufferSize() const {
    return simd::hprod(m_size) * bytesPerPixel();
}

size_t Bitmap::bytesPerPixel() const {
    size_t result;
    switch (m_componentFormat) {
        case Struct::EInt8:
        case Struct::EUInt8:   result = 1; break;
        case Struct::EInt16:
        case Struct::EUInt16:  result = 2; break;
        case Struct::EInt32:
        case Struct::EUInt32:  result = 4; break;
        case Struct::EFloat16: result = 2; break;
        case Struct::EFloat32: result = 4; break;
        case Struct::EFloat64: result = 8; break;
        default: Throw("Unknown component format!");
    }
    return result * m_channelCount;
}

MTS_IMPLEMENT_CLASS(Bitmap, Object)
NAMESPACE_END(mitsuba)
