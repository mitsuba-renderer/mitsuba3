#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/util.h>

#include <mitsuba/core/fstream.h>

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

Bitmap::Bitmap(EPixelFormat pfmt, Struct::EType cfmt,
               const Vector2s &size, size_t channel_count, uint8_t *data)
    : m_data(data), m_pixel_format(pfmt), m_component_format(cfmt),
      m_size(size), m_owns_data(false) {

    if (m_component_format == Struct::EUInt8)
        m_gamma = -1.0f; // sRGB by default
    else
        m_gamma = 1.0f; // Linear by default

    rebuild_struct(channel_count);

    if (!m_data) {

        m_data = std::unique_ptr<uint8_t[], enoki::aligned_deleter>(
            enoki::alloc<uint8_t>(buffer_size()));

        m_owns_data = true;
    }
}

Bitmap::Bitmap(const Bitmap &bitmap)
    : m_pixel_format(bitmap.m_pixel_format),
      m_component_format(bitmap.m_component_format), m_size(bitmap.m_size),
      m_struct(new Struct(*bitmap.m_struct)), m_gamma(bitmap.m_gamma),
      m_owns_data(true) {
    size_t size = buffer_size();
    m_data = std::unique_ptr<uint8_t[], enoki::aligned_deleter>(
        enoki::alloc<uint8_t>(size));
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
    if (!m_owns_data)
        m_data.release();
}

void Bitmap::clear() {
    memset(m_data.get(), 0, buffer_size());
}

void Bitmap::rebuild_struct(size_t channel_count) {
    std::vector<std::string> channels;

    switch (m_pixel_format) {
        case ELuminance:      channels = { "y" };               break;
        case ELuminanceAlpha: channels = { "y", "a" };          break;
        case ERGB:            channels = { "r", "g", "b"};      break;
        case ERGBA:           channels = { "r", "g", "b", "a"}; break;
        case EXYZ:            channels = { "x", "y", "z"};      break;
        case EXYZA:           channels = { "x", "y", "z", "a"}; break;
        case EMultiChannel: {
            for (size_t i = 0; i < channel_count; ++i)
                channels.push_back(tfm::format("ch%i", i));
            break;
        };
        default: Throw("Unknown pixel format!");
    }

    m_struct = new Struct();
    for (auto ch: channels)
        m_struct->append(ch, m_component_format);
}

size_t Bitmap::buffer_size() const {
    return hprod(m_size) * bytes_per_pixel();
}

size_t Bitmap::bytes_per_pixel() const {
    size_t result;
    switch (m_component_format) {
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
    return result * channel_count();
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

std::string Bitmap::to_string() const {
    std::ostringstream oss;
    oss << "Bitmap[" << std::endl
        << "  type = " << m_pixel_format << "," << std::endl
        << "  component_format = " << m_component_format << "," << std::endl
        << "  size = " << m_size << "," << std::endl
        << "  struct = " << string::indent(m_struct->to_string()) << "," << std::endl;

    std::vector<std::string> keys = m_metadata.property_names();
    if (!keys.empty()) {
        oss << "  metadata = {" << std::endl;
        for (std::vector<std::string>::const_iterator it = keys.begin(); it != keys.end(); ) {
            std::string value = m_metadata.as_string(*it);
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
        << "  data = [ " << util::mem_string(buffer_size()) << " of image data ]" << std::endl
        << "]";
    return oss.str();
}

// -----------------------------------------------------------------------------
//   OpenEXR bitmap I/O
// -----------------------------------------------------------------------------

class EXRIStream : public Imf::IStream {
public:
    EXRIStream(Stream *stream) : IStream(stream->to_string().c_str()),
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
    EXROStream(Stream *stream) : OStream(stream->to_string().c_str()),
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
void Bitmap::read_openexr(Stream *stream) {
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
            m_metadata.set_string(name, sattr->value());
        else if (typeName == "int" &&
            (iattr = header.findTypedAttribute<Imf::IntAttribute>(name.c_str())))
            m_metadata.set_long(name, iattr->value());
        else if (typeName == "float" &&
            (fattr = header.findTypedAttribute<Imf::FloatAttribute>(name.c_str())))
            m_metadata.set_float(name, (Float) fattr->value());
        else if (typeName == "double" &&
            (dattr = header.findTypedAttribute<Imf::DoubleAttribute>(name.c_str())))
            m_metadata.set_float(name, (Float) dattr->value());
        else if (typeName == "v3f" &&
            (vattr = header.findTypedAttribute<Imf::V3fAttribute>(name.c_str()))) {
            Imath::V3f vec = vattr->value();
            m_metadata.set_vector3f(name, Vector3f(vec.x, vec.y, vec.z));
        } else if (typeName == "m44f" &&
            (mattr = header.findTypedAttribute<Imf::M44fAttribute>(name.c_str()))) {
            Throw("read_openexr(): m44f type is not handled yet");
/*
            Matrix4x4 M;
            for (int i=0; i<4; ++i)
                for (int j=0; j<4; ++j)
                    M(i, j) = mattr->value().x[i][j];
            m_metadata.setTransform(name, Transform(M));
            */
        }
    }

    const Imf::ChannelList &channels = header.channels();
    m_gamma = 1.0f;
    m_struct = new Struct();

    for (auto it = channels.begin(); it != channels.end(); ++it) {
        switch (it.channel().type) {
            case Imf::HALF: break;
            case Imf::FLOAT: break;
            case Imf::UINT: break;
            default:
        Throw("read_openexr(): Invalid component type (must be "
            "float16, float32, or uint32)");

        }

    }
    /* Just how big is this image? */
    Imath::Box2i dataWindow = file.header().dataWindow();

    m_size = Vector2s(dataWindow.max.x - dataWindow.min.x + 1,
                      dataWindow.max.y - dataWindow.min.y + 1);

    /* Compute pixel / row strides */
    size_t pixelStride = compSize * channel_count(),
           rowStride = pixelStride * m_size.x;

    /* Finally, allocate memory for it */
    m_data = static_cast<uint8_t *>(allocAligned(getBufferSize()));
    m_owns_data = true;
    char *ptr = (char *) m_data;

    ptr -= (dataWindow.min.x + dataWindow.min.y * m_size.x) * pixelStride;

    ref_vector<Bitmap> resampleBuffers((size_t) m_channel_count);
    ref<ReconstructionFilter> rfilter;

    /* Tell OpenEXR where the image data should be put */
    Imf::FrameBuffer frameBuffer;
    for (size_t i=0; i<sourceChannels.size(); ++i) {
        const char *channel_name = sourceChannels[i];
        const Imf::Channel &channel = channels[channel_name];
        Vector2i sampling(channel.xSampling, channel.ySampling);
        m_channel_names.push_back(channel_name);

        if (channel.type != pxType)
            Log(EError, "read_openexr(): file has multiple channel formats, this is unsupported!");

        if (sampling == Vector2i(1)) {
            /* This is a full resolution channel. Load the ordinary way */
            frameBuffer.insert(channel_name, Imf::Slice(pxType, ptr, pixelStride, rowStride));
            ptr += compSize;
        } else {
            /* Uh oh, this is a sub-sampled channel. We will need to scale it up */
            Vector2i channelSize(m_size.x / sampling.x, m_size.y / sampling.y);
            resampleBuffers[i] = new Bitmap(Bitmap::ELuminance, m_component_format, channelSize);
            uint8_t *resamplePtr = resampleBuffers[i]->getUInt8Data();
            resamplePtr -= (dataWindow.min.x/sampling.x + dataWindow.min.y/sampling.x * channelSize.x) * compSize;
            frameBuffer.insert(channel_name, Imf::Slice(pxType, (char *) resamplePtr,
                compSize, compSize*channelSize.x, sampling.x, sampling.y));
            ptr += compSize;
        }
    }

    Log(EDebug, "Loading a %ix%i OpenEXR file (%s format, %s encoding)",
        m_size.x, m_size.y, tfm::format.c_str(), encodingString.c_str());

    file.setFrameBuffer(frameBuffer);
    file.readPixels(dataWindow.min.y, dataWindow.max.y);
}
#endif


void Bitmap::write(EFileFormat format, const fs::path &path, int compression) const {
    ref<FileStream> fs = new FileStream(path, true);
    write(format, fs, compression);
}

void Bitmap::write(EFileFormat format, Stream *stream, int compression) const {
    switch (format) {
        case EJPEG:
            if (compression == -1)
                compression = 100;
            write_jpeg(stream, compression);
            break;
#if 0
        case EPNG:
            if (compression == -1)
                compression = 5;
            write_png(stream, compression);
            break;
#endif
        case EOpenEXR:  write_openexr(stream); break;
        default:
            Log(EError, "Bitmap::write(): Invalid file format!");
    }
}

void Bitmap::write_openexr(Stream *stream) const {
    Log(EDebug, "Writing a %ix%i OpenEXR file", m_size.x(), m_size.y());
    EPixelFormat pixel_format = m_pixel_format;

#if SPECTRUM_SAMPLES == 3
    if (pixel_format == ESpectrum)
        pixel_format = ERGB;
    if (pixel_format == ESpectrumAlpha)
        pixel_format = ERGBA;
#endif

    Properties metadata(m_metadata);
    if (!metadata.has_property("generated_by"))
        metadata.set_string("generated_by", "Mitsuba version " MTS_VERSION);

    std::vector<std::string> keys = metadata.property_names();

    Imf::Header header(m_size.x(), m_size.y());
    for (std::vector<std::string>::const_iterator it = keys.begin(); it != keys.end(); ++it) {
        Properties::EPropertyType type = metadata.property_type(*it);

        switch (type) {
            case Properties::EString:
                header.insert(it->c_str(), Imf::StringAttribute(metadata.string(*it)));
                break;
            case Properties::ELong:
                header.insert(it->c_str(), Imf::IntAttribute(metadata.int_(*it)));
                break;
            case Properties::EFloat:
                header.insert(it->c_str(), Imf::FloatAttribute((float) metadata.float_(*it)));
                break;
            case Properties::EPoint3f: {
                Point3f val = metadata.point3f(*it);
                header.insert(it->c_str(), Imf::V3fAttribute(
                    Imath::V3f((float) val.x(), (float) val.y(), (float) val.z())));
            }
            break;
            case Properties::ETransform: {
                Throw("write_openexr(): m44f type is not handled yet");
#if 0 // TODO
                Matrix4x4 val = metadata.transform(*it).getMatrix();
                header.insert(it->c_str(), Imf::M44fAttribute(Imath::M44f(
                    (float) val(0, 0), (float) val(0, 1), (float) val(0, 2), (float) val(0, 3),
                    (float) val(1, 0), (float) val(1, 1), (float) val(1, 2), (float) val(1, 3),
                    (float) val(2, 0), (float) val(2, 1), (float) val(2, 2), (float) val(2, 3),
                    (float) val(3, 0), (float) val(3, 1), (float) val(3, 2), (float) val(3, 3))));
#endif
            }
            break;
            default:
                header.insert(it->c_str(), Imf::StringAttribute(metadata.as_string(*it)));
                break;
        }
    }

    if (pixel_format == EXYZ || pixel_format == EXYZA) {
        Imf::addChromaticities(header, Imf::Chromaticities(
            Imath::V2f(1.0f, 0.0f),
            Imath::V2f(0.0f, 1.0f),
            Imath::V2f(0.0f, 0.0f),
            Imath::V2f(1.0f / 3.0f, 1.0f / 3.0f)));
    } else if (pixel_format == ERGB || pixel_format == ERGBA) {
        Imf::addChromaticities(header, Imf::Chromaticities());
    }

    Imf::PixelType compType;
    size_t compStride;
    if (m_component_format == Struct::EType::EFloat16) {
        compType = Imf::HALF;
        compStride = 2;
    } else if (m_component_format == Struct::EType::EFloat32) {
        compType = Imf::FLOAT;
        compStride = 4;
    } else if (m_component_format == Struct::EType::EUInt32) {
        compType = Imf::UINT;
        compStride = 4;
    } else {
        Log(EError, "writeOpenEXR(): Invalid component type (must be "
            "float16, float32, or uint32)");
        return;
    }
    /*
    if ((channel_count() > 0) && m_channel_names.size() != channel_count())
        Log(EWarn, "write_openexr(): 'channel_names' has the wrong number of entries (%i, expected %i), ignoring..!",
        (int) m_channel_names.size(), (int) channel_count());
    */
    bool explicit_channel_names = false;
    Imf::ChannelList &channels = header.channels();
    //if (m_channel_names.size() == channel_count()) {
    if ((channel_count() > 0)) {
        for (auto it = m_struct->begin(); it != m_struct->end(); ++it)
            channels.insert(it->name.c_str(), Imf::Channel(compType));
        explicit_channel_names = true;
    } else if (pixel_format == ELuminance || pixel_format == ELuminanceAlpha) {
        channels.insert("Y", Imf::Channel(compType));
    } else if (pixel_format == ERGB || pixel_format == ERGBA ||
               pixel_format == EXYZ || pixel_format == EXYZA) {
        channels.insert("R", Imf::Channel(compType));
        channels.insert("G", Imf::Channel(compType));
        channels.insert("B", Imf::Channel(compType));
#if 0 // TODO
    } else if (pixel_format == ESpectrum || pixel_format == ESpectrumAlpha) {
        for (int i = 0; i<SPECTRUM_SAMPLES; ++i) {
            std::pair<Float, Float> coverage = Spectrum::getBinCoverage(i);
            std::string name = tfm::format("%.2f-%.2fnm", coverage.first, coverage.second);
            channels.insert(name.c_str(), Imf::Channel(compType));
        }
    } else if (pixel_format == EMultiChannel) {
        for (int i = 0; i<getChannelCount(); ++i)
            channels.insert(tfm::format("%i", i).c_str(), Imf::Channel(compType));
#endif
    } else {
        Log(EError, "writeOpenEXR(): Invalid pixel format!");
        return;
    }

    if ((pixel_format == ELuminanceAlpha || pixel_format == ERGBA ||
         pixel_format == EXYZA /*|| pixel_format == ESpectrumAlpha*/) && !explicit_channel_names)
        channels.insert("A", Imf::Channel(compType));

    size_t pixelStride = channel_count() * compStride,
        rowStride = pixelStride * m_size.x();
    char *ptr = (char *) m_data.get();

    Imf::FrameBuffer frameBuffer;

    if (explicit_channel_names) {
        for (auto it = m_struct->begin(); it != m_struct->end(); ++it) {
            frameBuffer.insert(it->name.c_str(), Imf::Slice(compType, ptr, pixelStride, rowStride));

            ptr += compStride;
        }
    } else if (pixel_format == ELuminance || pixel_format == ELuminanceAlpha) {
        frameBuffer.insert("Y", Imf::Slice(compType, ptr, pixelStride, rowStride)); ptr += compStride;
    } else if (pixel_format == ERGB || pixel_format == ERGBA || pixel_format == EXYZ || pixel_format == EXYZA) {
        frameBuffer.insert("R", Imf::Slice(compType, ptr, pixelStride, rowStride)); ptr += compStride;
        frameBuffer.insert("G", Imf::Slice(compType, ptr, pixelStride, rowStride)); ptr += compStride;
        frameBuffer.insert("B", Imf::Slice(compType, ptr, pixelStride, rowStride)); ptr += compStride;
#if 0 // TODO
    } else if (pixel_format == ESpectrum || pixel_format == ESpectrumAlpha) {
        for (int i = 0; i<SPECTRUM_SAMPLES; ++i) {
            std::pair<Float, Float> coverage = Spectrum::getBinCoverage(i);
            std::string name = tfm::format("%.2f-%.2fnm", coverage.first, coverage.second);
            frameBuffer.insert(name.c_str(), Imf::Slice(compType, ptr, pixelStride, rowStride)); ptr += compStride;
        }
    } else if (pixel_format == EMultiChannel) {
        for (int i = 0; i<getChannelCount(); ++i) {
            frameBuffer.insert(tfm::format("%i", i).c_str(), Imf::Slice(compType, ptr, pixelStride, rowStride));
            ptr += compStride;
        }
#endif
    }

    if ((pixel_format == ELuminanceAlpha || pixel_format == ERGBA ||
         pixel_format == EXYZA /*|| pixel_format == ESpectrumAlpha*/) && !explicit_channel_names)
        frameBuffer.insert("A", Imf::Slice(compType, ptr, pixelStride, rowStride));

    EXROStream ostr(stream);
    Imf::OutputFile file(ostr, header);
    file.setFrameBuffer(frameBuffer);
    file.writePixels(m_size.y());
}

/* ========================== *
*   JPEG helper functions    *
* ========================== */

extern "C" {
    static const size_t jpeg_bufferSize = 0x8000;

    typedef struct {
        struct jpeg_source_mgr mgr;
        JOCTET * buffer;
        Stream *stream;
    } jbuf_in_t;

    typedef struct {
        struct jpeg_destination_mgr mgr;
        JOCTET * buffer;
        Stream *stream;
    } jbuf_out_t;

    METHODDEF(void) jpeg_init_source(j_decompress_ptr cinfo) {
        jbuf_in_t *p = (jbuf_in_t *) cinfo->src;
        p->buffer = new JOCTET[jpeg_bufferSize];
    }

    METHODDEF(boolean) jpeg_fill_input_buffer(j_decompress_ptr cinfo) {
        jbuf_in_t *p = (jbuf_in_t *) cinfo->src;
        size_t nBytes;

        p->stream->read(p->buffer, jpeg_bufferSize);
        nBytes = jpeg_bufferSize;

#if 0
        try {
            p->stream->read(p->buffer, jpeg_bufferSize);
            nBytes = jpeg_bufferSize;
        } catch (const EOFException &e) {
            nBytes = e.getCompleted();
            if (nBytes == 0) {
                /* Insert a fake EOI marker */
                p->buffer[0] = (JOCTET) 0xFF;
                p->buffer[1] = (JOCTET) JPEG_EOI;
                nBytes = 2;
            }
        }
#endif
        cinfo->src->bytes_in_buffer = nBytes;
        cinfo->src->next_input_byte = p->buffer;
        return TRUE;
    }

    METHODDEF(void) jpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes) {
        if (num_bytes > 0) {
            while (num_bytes > (long) cinfo->src->bytes_in_buffer) {
                num_bytes -= (long) cinfo->src->bytes_in_buffer;
                jpeg_fill_input_buffer(cinfo);
            }
            cinfo->src->next_input_byte += (size_t) num_bytes;
            cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
        }
    }

    METHODDEF(void) jpeg_term_source(j_decompress_ptr cinfo) {
        jbuf_in_t *p = (jbuf_in_t *) cinfo->src;
        delete[] p->buffer;
    }

    METHODDEF(void) jpeg_init_destination(j_compress_ptr cinfo) {
        jbuf_out_t *p = (jbuf_out_t *) cinfo->dest;

        p->buffer = new JOCTET[jpeg_bufferSize];
        p->mgr.next_output_byte = p->buffer;
        p->mgr.free_in_buffer = jpeg_bufferSize;
    }

    METHODDEF(boolean) jpeg_empty_output_buffer(j_compress_ptr cinfo) {
        jbuf_out_t *p = (jbuf_out_t *) cinfo->dest;
        p->stream->write(p->buffer, jpeg_bufferSize);
        p->mgr.next_output_byte = p->buffer;
        p->mgr.free_in_buffer = jpeg_bufferSize;
        return 1;
    }

    METHODDEF(void) jpeg_term_destination(j_compress_ptr cinfo) {
        jbuf_out_t *p = (jbuf_out_t *) cinfo->dest;
        p->stream->write(p->buffer,
                         jpeg_bufferSize - p->mgr.free_in_buffer);
        delete[] p->buffer;
        p->mgr.free_in_buffer = 0;
    }

    METHODDEF(void) jpeg_error_exit(j_common_ptr cinfo) throw(std::runtime_error) {
        char msg[JMSG_LENGTH_MAX];
        (*cinfo->err->format_message) (cinfo, msg);
        Log(EError, "Critcal libjpeg error: %s", msg);
    }
};

void Bitmap::write_jpeg(Stream *stream, int quality) const {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    jbuf_out_t jbuf;

    int components = 0;
    if (m_pixel_format == ELuminance)
        components = 1;
    else if (m_pixel_format == ERGB)
        components = 3;
    else
        Log(EError, "writeJPEG(): Invalid pixel format!");

    if (m_component_format != Struct::EType::EUInt8)
        Log(EError, "writeJPEG(): Invalid component format!");

    memset(&jbuf, 0, sizeof(jbuf_out_t));
    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = jpeg_error_exit;
    jpeg_create_compress(&cinfo);

    cinfo.dest = (struct jpeg_destination_mgr *) &jbuf;
    jbuf.mgr.init_destination = jpeg_init_destination;
    jbuf.mgr.empty_output_buffer = jpeg_empty_output_buffer;
    jbuf.mgr.term_destination = jpeg_term_destination;
    jbuf.stream = stream;

    cinfo.image_width = m_size.x();
    cinfo.image_height = m_size.y();
    cinfo.input_components = components;
    cinfo.in_color_space = components == 1 ? JCS_GRAYSCALE : JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    Log(ETrace, "Writing a %ix%i JPEG file", m_size.x(), m_size.y());

    /* Write scanline by scanline */
    for (int i = 0; i<m_size.y(); ++i) {
        uint8_t *source = m_data.get() + i*m_size.x()*cinfo.input_components;
        jpeg_write_scanlines(&cinfo, &source, 1);
    }

    /* Release the libjpeg data structures */
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
}

/* ========================== *
*    PNG helper functions    *
* ========================== */

#if 0
static void png_flush_data(png_structp png_ptr) {
    png_voidp flush_io_ptr = png_get_io_ptr(png_ptr);
    ((Stream *) flush_io_ptr)->flush();
}

static void png_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
    png_voidp read_io_ptr = png_get_io_ptr(png_ptr);
    ((Stream *) read_io_ptr)->read(data, length);
}

static void png_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
    png_voidp write_io_ptr = png_get_io_ptr(png_ptr);
    ((Stream *) write_io_ptr)->write(data, length);
}

static void png_error_func(png_structp png_ptr, png_const_charp msg) {
    Log(EError, "Fatal libpng error: %s\n", msg);
    exit(-1);
}

static void png_warn_func(png_structp png_ptr, png_const_charp msg) {
    if (strstr(msg, "iCCP: known incorrect sRGB profile") != NULL)
        return;
    Log(EWarn, "libpng warning: %s\n", msg);
}

void Bitmap::write_png(Stream *stream, int compression) const {
    png_structp png_ptr;
    png_infop info_ptr;
    volatile png_bytepp rows = NULL;

    Log(EDebug, "Writing a %ix%i PNG file", m_size.x(), m_size.y());

    int colorType, bitDepth;
    switch (m_pixel_format) {
        case ELuminance: colorType = PNG_COLOR_TYPE_GRAY; break;
        case ELuminanceAlpha: colorType = PNG_COLOR_TYPE_GRAY_ALPHA; break;
        case ERGB: colorType = PNG_COLOR_TYPE_RGB; break;
        case ERGBA: colorType = PNG_COLOR_TYPE_RGBA; break;
        default:
            Log(EError, "writePNG(): Unsupported bitmap type!");
            return;
    }

    switch (m_component_format) {
        //case Struct::EType::EBitmask: bitDepth = 1; break;
        case Struct::EType::EUInt8: bitDepth = 8; break;
        case Struct::EType::EUInt16: bitDepth = 16; break;
        default:
            Log(EError, "writePNG(): Unsupported component type!");
            return;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, &png_error_func, &png_warn_func);
    if (png_ptr == NULL)
        Log(EError, "Error while creating PNG data structure");

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
        Log(EError, "Error while creating PNG information structure");
    }

    /* Error handling */
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        Log(EError, "Error writing the PNG file");
    }

    png_set_write_fn(png_ptr, stream, (png_rw_ptr) png_write_data, (png_flush_ptr) png_flush_data);
    png_set_compression_level(png_ptr, compression);

    png_text *text = NULL;

    Properties metadata(m_metadata);
    if (!metadata.has_property("generated_by"))
        metadata.set_string("generated_by", "Mitsuba version " MTS_VERSION);

    std::vector<std::string> keys = metadata.property_names();
    std::vector<std::string> values(keys.size());

    text = new png_text[keys.size()];
    memset(text, 0, sizeof(png_text) * keys.size());

    for (size_t i = 0; i<keys.size(); ++i) {
        values[i] = metadata.as_string(keys[i]);
        text[i].key = const_cast<char *>(keys[i].c_str());
        text[i].text = const_cast<char *>(values[i].c_str());
        text[i].compression = PNG_TEXT_COMPRESSION_NONE;
    }

    png_set_text(png_ptr, info_ptr, text, (int) keys.size());

    if (m_gamma == -1)
        png_set_sRGB_gAMA_and_cHRM(png_ptr, info_ptr, PNG_sRGB_INTENT_ABSOLUTE);
    else
        png_set_gAMA(png_ptr, info_ptr, 1 / m_gamma);

    if (m_component_format == Struct::EType::EUInt16 && Stream::host_byte_order() == Stream::ELittleEndian)
        png_set_swap(png_ptr); // Swap the byte order on little endian machines

    png_set_IHDR(png_ptr, info_ptr, m_size.x(), m_size.y(), bitDepth,
                 colorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    rows = new png_bytep[m_size.y()];

    size_t rowBytes = png_get_rowbytes(png_ptr, info_ptr);
    Assert(rowBytes == getBufferSize() / m_size.y());
    for (int i = 0; i<m_size.y(); i++)
        rows[i] = &m_data[rowBytes * i];

    png_write_image(png_ptr, rows);
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    if (text)
        delete[] text;
    delete[] rows;
}
#endif

MTS_IMPLEMENT_CLASS(Bitmap, Object)
NAMESPACE_END(mitsuba)
