#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/util.h>

/* libpng */
#include <png.h>

/* libjpeg */
extern "C" {
    #include <jpeglib.h>
    #include <jerror.h>
};

/* OpenEXR */
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-register"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#endif

#include <ImfInputFile.h>
#include <ImfStandardAttributes.h>
#include <ImfRgbaYca.h>
#include <ImfOutputFile.h>
#include <ImfChannelList.h>
#include <ImfStringAttribute.h>
#include <ImfIntAttribute.h>
#include <ImfFloatAttribute.h>
#include <ImfDoubleAttribute.h>
#include <ImfVecAttribute.h>
#include <ImfMatrixAttribute.h>
#include <ImfVersion.h>
#include <ImfIO.h>
#include <ImathBox.h>

#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

NAMESPACE_BEGIN(mitsuba)

Bitmap::Bitmap(EPixelFormat pFormat, Struct::EType cFormat,
               const Vector2s &size, size_t channelCount, uint8_t *data)
    : m_data(data), m_pixelFormat(pFormat), m_componentFormat(cFormat),
      m_size(size), m_ownsData(false) {
    Assert(size.x > 0 && size.y > 0, "Invalid bitmap size");

    if (m_componentFormat == Struct::EUInt8)
        m_gamma = -1.0f; // sRGB by default
    else
        m_gamma = 1.0f; // Linear by default

    rebuildStruct(channelCount);

    if (!m_data) {
        m_data = std::unique_ptr<uint8_t[], AlignedAllocator>(
            AlignedAllocator::alloc<uint8_t>(bufferSize()));

        m_ownsData = true;
    }
}

Bitmap::Bitmap(const Bitmap &bitmap)
    : m_pixelFormat(bitmap.m_pixelFormat),
      m_componentFormat(bitmap.m_componentFormat), m_size(bitmap.m_size),
      m_struct(new Struct(*bitmap.m_struct)), m_gamma(bitmap.m_gamma),
      m_ownsData(true) {
    size_t size = bufferSize();
    m_data = std::unique_ptr<uint8_t[], AlignedAllocator>(
        AlignedAllocator::alloc<uint8_t>(size));
    memcpy(m_data.get(), bitmap.m_data.get(), size);
}

Bitmap::Bitmap(Stream *stream, EFileFormat format) {
    if (format == EAuto) {
        /* Try to automatically detect the file format */
        size_t pos = stream->tell();
        uint8_t start[8];
        stream->read(start, 8);

        if (start[0] == 'B' && start[1] == 'M') {
            format = EBMP;
        } else if (start[0] == '#' && start[1] == '?') {
            format = ERGBE;
        } else if (start[0] == 'P' && (start[1] == 'F' || start[1] == 'f')) {
            format = EPFM;
        } else if (start[0] == 'P' && start[1] == '6') {
            format = EPPM;
        } else if (start[0] == 0xFF && start[1] == 0xD8) {
            format = EJPEG;
        } else if (png_sig_cmp(start, 0, 8) == 0) {
            format = EPNG;
        } else if (Imf::isImfMagic((const char *) start)) {
            format = EOpenEXR;
        } else {
            /* Check for a TGAv2 file */
            char footer[18];
            stream->seek(stream->size() - 18);
            stream->read(footer, 18);
            if (footer[17] == 0 && strncmp(footer, "TRUEVISION-XFILE.", 17) == 0)
                format = ETGA;
        }
        stream->seek(pos);
    }
}

Bitmap::~Bitmap() {
    if (!m_ownsData)
        m_data.release();
}

void Bitmap::clear() {
    memset(m_data.get(), 0, bufferSize());
}

void Bitmap::rebuildStruct(size_t channelCount) {
    std::vector<std::string> channels;

    switch (m_pixelFormat) {
        case ELuminance:      channels = { "y" };               break;
        case ELuminanceAlpha: channels = { "y", "a" };          break;
        case ERGB:            channels = { "r", "g", "b"};      break;
        case ERGBA:           channels = { "r", "g", "b", "a"}; break;
        case EXYZ:            channels = { "x", "y", "z"};      break;
        case EXYZA:           channels = { "x", "y", "z", "a"}; break;
        case EMultiChannel: {
            for (size_t i = 0; i < channelCount; ++i)
                channels.push_back(tfm::format("ch%i", i));
            break;
        };
        default: Throw("Unknown pixel format!");
    }

    m_struct = new Struct();
    for (auto ch: channels)
        m_struct->append(ch, m_componentFormat);
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
    return result * channelCount();
}

std::ostream &operator<<(std::ostream &os, Bitmap::EPixelFormat value) {
    switch (value) {
        case Bitmap::ELuminance: os << "luminance"; break;
        case Bitmap::ELuminanceAlpha: os << "luminanceAlpha"; break;
        case Bitmap::ERGB: os << "rgb"; break;
        case Bitmap::ERGBA: os << "rgba"; break;
        case Bitmap::EXYZ: os << "xyz"; break;
        case Bitmap::EXYZA: os << "xyza"; break;
        case Bitmap::EMultiChannel: os << "multiChannel"; break;
        default: os << "invalid"; break;
    }
    return os;
}

std::string Bitmap::toString() const {
    std::ostringstream oss;
    oss << "Bitmap[" << std::endl
        << "  type = " << m_pixelFormat << "," << std::endl
        << "  componentFormat = " << m_componentFormat << "," << std::endl
        << "  size = " << m_size << "," << std::endl
        << "  struct = " << string::indent(m_struct->toString()) << "," << std::endl;

    std::vector<std::string> keys = m_metaData.propertyNames();
    if (!keys.empty()) {
        oss << "  metadata = {" << std::endl;
        for (std::vector<std::string>::const_iterator it = keys.begin(); it != keys.end(); ) {
            std::string value = m_metaData.asString(*it);
            if (value.size() > 50)
                value = value.substr(0, 50) + ".. [truncated]";

            oss << "    \"" << *it << "\" => \"" << value << "\"";
            if (++it != keys.end())
                oss << ",";
            oss << std::endl;
        }
        oss << "  }," << std::endl;
    }

    oss << "  gamma = " << m_gamma << "," << std::endl
        << "  data = [ " << util::memString(bufferSize()) << " of image data ]" << std::endl
        << "]";
    return oss.str();
}

// -----------------------------------------------------------------------------
//   OpenEXR bitmap I/O
// -----------------------------------------------------------------------------

class EXRIStream : public Imf::IStream {
public:
    EXRIStream(Stream *stream) : IStream(stream->toString().c_str()),
        m_stream(stream) {
        m_offset = stream->tell();
        m_size = stream->size();
    }

    bool read(char *c, int n) {
        m_stream->read(c, n);
        return m_stream->tell() == m_size;
    }

    Imf::Int64 tellg() {
        return m_stream->tell()-m_offset;
    }

    void seekg(Imf::Int64 pos) {
        m_stream->seek((size_t) pos + m_offset);
    }

    void clear() { }
private:
    ref<Stream> m_stream;
    size_t m_offset, m_size;
};

class EXROStream : public Imf::OStream {
public:
    EXROStream(Stream *stream) : OStream(stream->toString().c_str()),
        m_stream(stream) {
    }

    void write(const char *c, int n) { m_stream->write(c, n); }
    Imf::Int64 tellp() { return m_stream->tell(); }
    void seekp(Imf::Int64 pos) { m_stream->seek((size_t) pos); }

    void clear() { }
private:
    ref<Stream> m_stream;
};

#if 0
void Bitmap::readOpenEXR(Stream *stream) {
    EXRIStream istr(stream);
    Imf::InputFile file(istr);

    const Imf::Header &header = file.header();

    /* Load metadata if present */
    for (Imf::Header::ConstIterator it = header.begin(); it != header.end(); ++it) {
        std::string name = it.name(), typeName = it.attribute().typeName();
        const Imf::StringAttribute *sattr;
        const Imf::IntAttribute *iattr;
        const Imf::FloatAttribute *fattr;
        const Imf::DoubleAttribute *dattr;
        const Imf::V3fAttribute *vattr;
        const Imf::M44fAttribute *mattr;

        if (typeName == "string" &&
            (sattr = header.findTypedAttribute<Imf::StringAttribute>(name.c_str())))
            m_metaData.setString(name, sattr->value());
        else if (typeName == "int" &&
            (iattr = header.findTypedAttribute<Imf::IntAttribute>(name.c_str())))
            m_metaData.setLong(name, iattr->value());
        else if (typeName == "float" &&
            (fattr = header.findTypedAttribute<Imf::FloatAttribute>(name.c_str())))
            m_metaData.setFloat(name, (Float) fattr->value());
        else if (typeName == "double" &&
            (dattr = header.findTypedAttribute<Imf::DoubleAttribute>(name.c_str())))
            m_metaData.setFloat(name, (Float) dattr->value());
        else if (typeName == "v3f" &&
            (vattr = header.findTypedAttribute<Imf::V3fAttribute>(name.c_str()))) {
            Imath::V3f vec = vattr->value();
            m_metaData.setVector3f(name, Vector3f(vec.x, vec.y, vec.z));
        } else if (typeName == "m44f" &&
            (mattr = header.findTypedAttribute<Imf::M44fAttribute>(name.c_str()))) {
            Matrix4x4 M;
            for (int i=0; i<4; ++i)
                for (int j=0; j<4; ++j)
                    M(i, j) = mattr->value().x[i][j];
            m_metadata.setTransform(name, Transform(M));
        }
    }

    const Imf::ChannelList &channels = header.channels();
    m_gamma = 1.0f;
    m_struct = new Struct();

    for (const auto& ch : channels) {
        switch (type) {
            case Imf::HALF: break;
            case Imf::FLOAT: break;
            case Imf::UINT: break;
            default:
        Throw("readOpenEXR(): Invalid component type (must be "
            "float16, float32, or uint32)");

        }

    }
    /* Just how big is this image? */
    Imath::Box2i dataWindow = file.header().dataWindow();

    m_size = Vector2s(dataWindow.max.x - dataWindow.min.x + 1,
                      dataWindow.max.y - dataWindow.min.y + 1);

    /* Compute pixel / row strides */
    size_t pixelStride = compSize * m_channelCount,
           rowStride = pixelStride * m_size.x;

    /* Finally, allocate memory for it */
    m_data = static_cast<uint8_t *>(allocAligned(getBufferSize()));
    m_ownsData = true;
    char *ptr = (char *) m_data;

    ptr -= (dataWindow.min.x + dataWindow.min.y * m_size.x) * pixelStride;

    ref_vector<Bitmap> resampleBuffers((size_t) m_channelCount);
    ref<ReconstructionFilter> rfilter;

    /* Tell OpenEXR where the image data should be put */
    Imf::FrameBuffer frameBuffer;
    for (size_t i=0; i<sourceChannels.size(); ++i) {
        const char *channelName = sourceChannels[i];
        const Imf::Channel &channel = channels[channelName];
        Vector2i sampling(channel.xSampling, channel.ySampling);
        m_channelNames.push_back(channelName);

        if (channel.type != pxType)
            Log(EError, "readOpenEXR(): file has multiple channel formats, this is unsupported!");

        if (sampling == Vector2i(1)) {
            /* This is a full resolution channel. Load the ordinary way */
            frameBuffer.insert(channelName, Imf::Slice(pxType, ptr, pixelStride, rowStride));
            ptr += compSize;
        } else {
            /* Uh oh, this is a sub-sampled channel. We will need to scale it up */
            Vector2i channelSize(m_size.x / sampling.x, m_size.y / sampling.y);
            resampleBuffers[i] = new Bitmap(Bitmap::ELuminance, m_componentFormat, channelSize);
            uint8_t *resamplePtr = resampleBuffers[i]->getUInt8Data();
            resamplePtr -= (dataWindow.min.x/sampling.x + dataWindow.min.y/sampling.x * channelSize.x) * compSize;
            frameBuffer.insert(channelName, Imf::Slice(pxType, (char *) resamplePtr,
                compSize, compSize*channelSize.x, sampling.x, sampling.y));
            ptr += compSize;
        }
    }

    Log(EDebug, "Loading a %ix%i OpenEXR file (%s format, %s encoding)",
        m_size.x, m_size.y, formatString.c_str(), encodingString.c_str());

    file.setFrameBuffer(frameBuffer);
    file.readPixels(dataWindow.min.y, dataWindow.max.y);
}
#endif

MTS_IMPLEMENT_CLASS(Bitmap, Object)
NAMESPACE_END(mitsuba)
