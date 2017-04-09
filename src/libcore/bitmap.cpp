#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/fstream.h>
#include <tbb/tbb.h>

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
#include <ImfCompressionAttribute.h>
#include <ImfVecAttribute.h>
#include <ImfMatrixAttribute.h>
#include <ImfVersion.h>
#include <ImfIO.h>
#include <ImathBox.h>

#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

NAMESPACE_BEGIN(mitsuba)

Bitmap::Bitmap(EPixelFormat pixel_format, Struct::EType component_format,
               const Vector2s &size, size_t channel_count, uint8_t *data)
    : m_data(data), m_pixel_format(pixel_format),
      m_component_format(component_format), m_size(size), m_owns_data(false) {

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
    read(stream, format);
}

Bitmap::Bitmap(const fs::path &filename, EFileFormat format) {
    ref<FileStream> fs = new FileStream(filename);
    read(fs, format);
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

template <typename Scalar, bool Filter>
static void
resample(Bitmap *target, const Bitmap *source,
         const ReconstructionFilter *rfilter_,
         const std::pair<Bitmap::EBoundaryCondition, Bitmap::EBoundaryCondition> bc,
         const std::pair<Scalar, Scalar> &clamp, ref<Bitmap> temp) {
    ref<const ReconstructionFilter> rfilter(rfilter_);

    if (!rfilter) {
        /* Upsample using a 2-lobed Lanczos reconstruction filter */
        Properties props("lanczos");
        props.set_int("lobes", 2);
        rfilter = PluginManager::instance()->create_object<ReconstructionFilter>(props);
    }

    if (source->height() == target->height() &&
        source->width() == target->width() && !Filter) {
        memcpy(target->uint8_data(), source->uint8_data(), source->buffer_size());
        return;
    }

    uint32_t channels = source->channel_count();

    if (source->width() != target->width() || Filter) {
        /* Re-sample horizontally */
        Resampler<Scalar> r(rfilter, source->width(), target->width());
        r.set_boundary_condition(bc.first);
        r.set_clamp(clamp);

        /* Create a bitmap for intermediate storage */
        if (!temp) {
            if (source->height() == target->height() && !Filter) {
                // write directly to the output bitmap
                temp = target;
            } else {
                // otherwise: write to a temporary bitmap
                temp = new Bitmap(
                    source->pixel_format(), source->component_format(),
                    Vector2s(target->width(), source->height()), channels);
            }
        }

        tbb::parallel_for(
            tbb::blocked_range<size_t>(0, source->height(), 100),
            [&](const tbb::blocked_range<size_t> &range) {
                for (auto y = range.begin(); y != range.end(); ++y) {
                    const Scalar *s = (const Scalar *) source->uint8_data() +
                                      y * source->width() * channels;
                    Scalar *t       = (Scalar *) temp->uint8_data() +
                                      y * target->width() * channels;
                    r.resample(s, 1, t, 1, channels);
                }
            }
        );

        /* Now, read from the temporary bitmap */
        source = temp;
    }

    if (source->height() != target->height() || Filter) {
        /* Re-sample vertically */
        Resampler<Scalar> r(rfilter, source->height(), target->height());
        r.set_boundary_condition(bc.second);
        r.set_clamp(clamp);

        tbb::parallel_for(
            tbb::blocked_range<size_t>(0, source->width(), 100),
            [&](const tbb::blocked_range<size_t> &range) {
                for (auto x = range.begin(); x != range.end(); ++x) {
                    const Scalar *s = (const Scalar *) source->uint8_data() +
                                      x * channels;
                    Scalar *t       = (Scalar *) target->uint8_data() +
                                      x * channels;
                    r.resample(s, source->width(), t, target->width(), channels);
                }
            }
        );
    }
}

void Bitmap::resample(Bitmap *target, const ReconstructionFilter *rfilter,
                      const std::pair<EBoundaryCondition, EBoundaryCondition> &bc,
                      const std::pair<Float, Float> &clamp, Bitmap *temp) const {

    if (pixel_format() != target->pixel_format() ||
        component_format() != target->component_format() ||
        channel_count() != target->channel_count())
        Throw("Bitmap::resample(): Incompatible source and target bitmaps!");

    if (temp && (pixel_format() != temp->pixel_format() ||
                 component_format() != temp->component_format() ||
                 channel_count() != temp->channel_count() ||
                 temp->size() != Vector2s(target->width(), height())))
        Throw("Bitmap::resample(): Incompatible temporary bitmap specified!");

    switch (m_component_format) {
        case Struct::EFloat16:
            mitsuba::resample<enoki::half, false>(target, this, rfilter, bc,
                                                  clamp, temp);
            break;

        case Struct::EFloat32:
            mitsuba::resample<float, false>(target, this, rfilter, bc,
                                            clamp, temp);
            break;

        case Struct::EFloat64:
            mitsuba::resample<double, false>(target, this, rfilter, bc,
                                             clamp, temp);
            break;

        default:
            Log(EError, "resample(): Unsupported component type! (must be float16/32/64)");
    }
}

ref<Bitmap> Bitmap::resample(const Vector2s &res, const ReconstructionFilter *rfilter,
                             const std::pair<EBoundaryCondition, EBoundaryCondition> &bc,
                             const std::pair<Float, Float> &bound) const {
    ref<Bitmap> result =
        new Bitmap(m_pixel_format, m_component_format, res, channel_count());
    result->m_struct = m_struct;
    result->m_metadata = m_metadata;
    resample(result, rfilter, bc, bound);
    return result;
}

void Bitmap::read(Stream *stream, EFileFormat format) {
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
    switch (format) {
        //case EBMP:     read_bmp(stream); break;
        //case EJPEG:    read_jpeg(stream); break;
        case EOpenEXR: read_openexr(stream); break;
        //case ERGBE:    read_rgbe(stream); break;
        //case EPFM:     read_pfm(stream); break;
        //case EPPM:     read_ppm(stream); break;
        //case ETGA:     read_tga(stream); break;
        //case EPNG:     read_png(stream); break;
        default:
            Throw("Bitmap: Invalid file format!");
    }
}

void Bitmap::write(const fs::path &path, EFileFormat format, int quality) const {
    ref<FileStream> fs = new FileStream(path, FileStream::ETruncReadWrite);
    write(fs, format, quality);
}

void Bitmap::write(Stream *stream, EFileFormat format, int quality) const {
    auto fs = dynamic_cast<FileStream *>(stream);

    if (format == EAuto) {
        if (!fs)
            Throw("Bitmap::write(): can't decide file format based on filename "
                  "since the target stream is not a file stream");
        std::string extension = string::to_lower(fs->path().extension());
        if (extension == ".exr")
            format = EOpenEXR;
        else if (extension == ".png")
            format = EPNG;
        else if (extension == ".jpg" || extension == ".jpeg")
            format = EJPEG;
        else if (extension == ".hdr" || extension == ".rgbe")
            format = ERGBE;
        else if (extension == ".pfm")
            format = EPFM;
        else if (extension == ".ppm")
            format = EPPM;
        else
            Throw("Bitmap::write(): unsupported bitmap file extension \"%s\"",
                  extension);
    }

    Log(EDebug, "Writing %s file \"%s\" .. (%ix%i, %s, %s)",
        format, fs ? fs->path().string() : "<stream>",
        m_size.x(), m_size.y(),
        m_pixel_format, m_component_format
    );

    switch (format) {
        case EJPEG:
            if (quality == -1)
                quality = 100;
            write_jpeg(stream, quality);
            break;
#if 0
        case EPNG:
            if (quality == -1)
                quality = 5;
            write_png(stream, quality);
            break;
#endif
        case EOpenEXR:  write_openexr(stream, quality); break;
        default:
            Log(EError, "Bitmap::write(): Invalid file format!");
    }
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
        for (auto it = keys.begin(); it != keys.end(); ) {
            std::string value = m_metadata.as_string(*it);
            if (value.length() > 50)
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

    bool read(char *c, int n) override {
        m_stream->read(c, n);
        return m_stream->tell() == m_size;
    }

    Imf::Int64 tellg() override {
        return m_stream->tell()-m_offset;
    }

    void seekg(Imf::Int64 pos) override {
        m_stream->seek((size_t) pos + m_offset);
    }

    void clear() override { }
private:
    ref<Stream> m_stream;
    size_t m_offset, m_size;
};

class EXROStream : public Imf::OStream {
public:
    EXROStream(Stream *stream) : OStream(stream->to_string().c_str()),
        m_stream(stream) { }

    void write(const char *c, int n) override { m_stream->write(c, n); }
    Imf::Int64 tellp() override { return m_stream->tell(); }
    void seekp(Imf::Int64 pos) override { m_stream->seek((size_t) pos); }
private:
    ref<Stream> m_stream;
};

void Bitmap::read_openexr(Stream *stream) {
    if (Imf::globalThreadCount() == 0)
        Imf::setGlobalThreadCount(std::min(8, util::core_count()));

    EXRIStream istr(stream);
    Imf::InputFile file(istr);

    const Imf::Header &header = file.header();
    const Imf::ChannelList &channels = header.channels();

    if (channels.begin() == channels.end())
        Throw("read_openexr(): Image does not contain any channels!");

    /* Load metadata if present */
    for (auto it = header.begin(); it != header.end(); ++it) {
        const char *name = it.name();
        const Imf::Attribute *attr = &it.attribute();

        /* StringAttribute */ {
            auto v = dynamic_cast<const Imf::StringAttribute *>(attr);
            if (v) m_metadata.set_string(name, v->value());
        }
        /* IntAttribute */ {
            auto v = dynamic_cast<const Imf::IntAttribute *>(attr);
            if (v) m_metadata.set_long(name, v->value());
        }
        /* FloatAttribute */ {
            auto v = dynamic_cast<const Imf::FloatAttribute *>(attr);
            if (v) m_metadata.set_float(name, Float(v->value()));
        }
        /* DoubleAttribute */ {
            auto v = dynamic_cast<const Imf::DoubleAttribute *>(attr);
            if (v) m_metadata.set_float(name, Float(v->value()));
        }
        /* V3fAttribute */ {
            auto v = dynamic_cast<const Imf::V3fAttribute *>(attr);
            if (v) {
                Imath::V3f vec = v->value();
                m_metadata.set_vector3f(name, Vector3f(vec.x, vec.y, vec.z));
            }
        }
        /* M44fAttribute */ {
            auto v = dynamic_cast<const Imf::M44fAttribute *>(attr);
            if (v) {
                Matrix4f M;
                for (size_t i = 0; i < 4; ++i)
                    for (size_t j = 0; j < 4; ++j)
                        M(i, j) = v->value().x[i][j];
                m_metadata.set_transform(name, Transform(M));
            }
        }
    }

    bool process_colors = false;
    m_gamma = 1.0f;
    m_pixel_format = EMultiChannel;
    m_struct = new Struct();
    Imf::PixelType pixel_type = channels.begin().channel().type;

    switch (pixel_type) {
        case Imf::HALF:  m_component_format = Struct::EFloat16; break;
        case Imf::FLOAT: m_component_format = Struct::EFloat32; break;
        case Imf::UINT:  m_component_format = Struct::EUInt32;  break;
        default: Throw("read_openexr(): Invalid component type (must be "
                       "float16, float32, or uint32)");
    }

    enum { Unknown, R, G, B, X, Y, Z, A, RY, BY, NumClasses };

    /* Classification scheme for color channels */
    auto channel_class = [](std::string name) -> uint8_t {
        auto it = name.rfind(".");
        if (it != std::string::npos)
            name = name.substr(it + 1);
        name = string::to_lower(name);
        if (name == "r") return R;
        if (name == "g") return G;
        if (name == "b") return B;
        if (name == "x") return X;
        if (name == "y") return Y;
        if (name == "z") return Z;
        if (name == "ry") return RY;
        if (name == "by") return BY;
        if (name == "a") return A;
        return Unknown;
    };

    /* Assign a sorting key to color channels */
    auto channel_key = [&](std::string name) -> std::string {
        uint8_t class_ = channel_class(name);
        if (class_ == Unknown)
            return name;
        auto it = name.rfind(".");
        char suffix('0' + class_);
        if (it != std::string::npos)
            name = name.substr(0, it) + "." + suffix;
        else
            name = suffix;
        return name;
    };

    bool found[NumClasses] = { false };
    std::vector<std::string> channels_sorted;
    for (auto it = channels.begin(); it != channels.end(); ++it) {
        std::string name(it.name());
        found[channel_class(name)] = true;
        channels_sorted.push_back(name);
    }

    std::sort(
        channels_sorted.begin(), channels_sorted.end(),
        [&](auto const &v0, auto const &v1) {
            return channel_key(v0) < channel_key(v1);
        }
    );

    /* Order channel names based on RGB/XYZ[A] suffix */
    for (auto const &name: channels_sorted)
        m_struct->append(name, m_component_format);

    /* Attempt to detect a standard combination of color channels */
    m_pixel_format = EMultiChannel;
    bool luminance_chroma_format = false;
    if (found[R] && found[G] && found[B]) {
        if (m_struct->field_count() == 3)
            m_pixel_format = ERGB;
        else if (found[A] && m_struct->field_count() == 4)
            m_pixel_format = ERGBA;
    } else if (found[X] && found[Y] && found[Z]) {
        if (m_struct->field_count() == 3)
            m_pixel_format = EXYZ;
        else if (found[A] && m_struct->field_count() == 4)
            m_pixel_format = EXYZA;
    } else if (found[Y] && found[RY] && found[BY]) {
        if (m_struct->field_count() == 3)
            m_pixel_format = ERGB;
        else if (found[A] && m_struct->field_count() == 4)
            m_pixel_format = ERGBA;
        luminance_chroma_format = true;
    } else if (found[Y]) {
        if (m_struct->field_count() == 1)
            m_pixel_format = ELuminance;
        else if (found[A] && m_struct->field_count() == 2)
            m_pixel_format = ELuminanceAlpha;
    }

    /* Check if there is a chromaticity header entry */
    Imf::Chromaticities file_chroma = Imf::chromaticities(file.header());

    auto chroma_eq = [](const Imf::Chromaticities &a,
                        const Imf::Chromaticities &b) {
        return (a.red  - b.red).length2()  + (a.green - b.green).length2() +
               (a.blue - b.blue).length2() + (a.white - b.white).length2() < 1e-6f;
    };

    auto set_suffix = [](std::string name, const std::string &suffix) {
        auto it = name.rfind(".");
        if (it != std::string::npos)
            name = name.substr(0, it) + "." + suffix;
        else
            name = suffix;
        return name;
    };

    Imath::Box2i data_window = file.header().dataWindow();
    m_size = Vector2s(data_window.max.x - data_window.min.x + 1,
                      data_window.max.y - data_window.min.y + 1);

    /* Compute pixel / row strides */
    size_t pixel_stride = bytes_per_pixel(),
           row_stride = pixel_stride * m_size.x(),
           pixel_count = hprod(m_size);

    /* Finally, allocate memory for it */
    m_data = std::unique_ptr<uint8_t[], enoki::aligned_deleter>(
        enoki::alloc<uint8_t>(row_stride * m_size.y()));
    m_owns_data = true;

    using ResampleBuffer = std::pair<std::string, ref<Bitmap>>;
    std::vector<ResampleBuffer> resample_buffers;

    uint8_t *ptr = m_data.get() -
        (data_window.min.x + data_window.min.y * m_size.x()) * pixel_stride;

    /* Tell OpenEXR where the image data should be put */
    Imf::FrameBuffer framebuffer;
    for (auto const &field: *m_struct) {
        const Imf::Channel &channel = channels[field.name];
        Vector2i sampling(channel.xSampling, channel.ySampling);
        Imf::Slice slice;

        if (sampling == Vector2i(1)) {
            /* This is a full resolution channel. Load the ordinary way */
            slice = Imf::Slice(pixel_type, (char *) (ptr + field.offset),
                               pixel_stride, row_stride);
        } else {
            /* Uh oh, this is a sub-sampled channel. We will need to scale it */
            Vector2s channel_size = m_size / sampling;

            ref<Bitmap> bitmap = new Bitmap(Bitmap::ELuminance,
                                            m_component_format, channel_size);

            uint8_t *ptr_nested = bitmap->uint8_data() -
                (data_window.min.x / sampling.x() +
                 data_window.min.y / sampling.y() * channel_size.x()) * field.size;

            slice = Imf::Slice(pixel_type, (char *) ptr_nested, field.size,
                               field.size * channel_size.x(), sampling.x(),
                               sampling.y());

            resample_buffers.emplace_back(field.name, std::move(bitmap));
        }

        framebuffer.insert(field.name, slice);
    }

    auto fs = dynamic_cast<FileStream *>(stream);

    Log(EDebug, "Loading OpenEXR file \"%s\" (%ix%i, %s, %s) ..",
        fs ? fs->path().string() : "<stream>", m_size.x(), m_size.y(),
        m_pixel_format, m_component_format);

    file.setFrameBuffer(framebuffer);
    file.readPixels(data_window.min.y, data_window.max.y);

    for (auto &buf: resample_buffers) {
        Log(EDebug, "Upsampling layer \"%s\" from %ix%i to %ix%i pixels",
            buf.first, buf.second->width(), buf.second->height(),
            m_size.x(), m_size.y());

        buf.second = buf.second->resample(m_size);
        const Struct::Field &field = m_struct->field(buf.first);

        size_t comp_size = field.size;
        uint8_t *dst = uint8_data() + field.offset;
        uint8_t *src = buf.second->uint8_data();

        for (size_t j=0; j<pixel_count; ++j) {
            memcpy(dst, src, comp_size);
            src += comp_size;
            dst += pixel_stride;
        }

        buf.second = nullptr;
    }

    if (luminance_chroma_format) {
        Log(EDebug, "Converting from Luminance-Chroma to RGB format ..");
        Imath::V3f yw = Imf::RgbaYca::computeYw(file_chroma);

        auto convert = [&](auto *data) {
            using T = std::decay_t<decltype(*data)>;

            for (size_t j = 0; j < pixel_count; ++j) {
                Float Y  = (Float) data[0],
                      RY = (Float) data[1],
                      BY = (Float) data[2];

                if (std::is_integral<T>::value) {
                    Float scale = Float(1) / std::numeric_limits<T>::max();
                    Y *= scale; RY *= scale; BY *= scale;
                }

                Float R = (RY + 1) * Y,
                      B = (BY + 1) * Y,
                      G = ((Y - R * yw.x - B * yw.z) / yw.y);

                if (std::is_integral<T>::value) {
                    Float scale = std::numeric_limits<T>::max();
                    R *= R * scale + 0.5f;
                    G *= G * scale + 0.5f;
                    B *= B * scale + 0.5f;
                }

                data[0] = T(R); data[1] = T(G); data[2] = T(B);
                data += channel_count();
            }
        };

        switch (m_component_format) {
            case Struct::EFloat16: convert((enoki::half *) m_data.get()); break;
            case Struct::EFloat32: convert((float *)       m_data.get()); break;
            case Struct::EUInt32:  convert((uint32_t*)     m_data.get()); break;
            default: Throw("Internal error!");
        }

        for (int i = 0; i < 3; ++i) {
            std::string &name = m_struct->operator[](i).name;
            name = set_suffix(name, std::string(1, "RGB"[i]));
        }
    }

    if (Imf::hasChromaticities(file.header()) &&
        (m_pixel_format == ERGB || m_pixel_format == ERGBA)) {

        Imf::Chromaticities itu_rec_b_709;
        Imf::Chromaticities xyz(Imath::V2f(1.0f, 0.0f), Imath::V2f(0.0f, 1.0f),
                                Imath::V2f(0.0f, 0.0f), Imath::V2f(1.0f / 3.0f, 1.0f / 3.0f));

        if (chroma_eq(file_chroma, itu_rec_b_709)) {
            /* Already in the right space -- do nothing. */
        } else if (chroma_eq(file_chroma, xyz)) {
            /* This is an XYZ image */
            m_pixel_format = m_pixel_format == ERGB ? EXYZ : EXYZA;
            for (int i = 0; i < 3; ++i) {
                std::string &name = m_struct->operator[](i).name;
                name = set_suffix(name, std::string(1, "XYZ"[i]));
            }
        } else {
            /* Non-standard chromaticities. Special processing is required.. */
            process_colors = true;
        }
    }

    if (process_colors) {
        /* Convert ITU-R Rec. BT.709 linear RGB */
        Imath::M44f M = Imf::RGBtoXYZ(file_chroma, 1) *
                        Imf::XYZtoRGB(Imf::Chromaticities(), 1);

        Log(EDebug, "Converting to sRGB color space ..");

        auto convert = [&](auto *data) {
            using T = std::decay_t<decltype(*data)>;

            for (size_t j = 0; j < pixel_count; ++j) {
                Float R = (Float) data[0],
                      G = (Float) data[1],
                      B = (Float) data[2];

                if (std::is_integral<T>::value) {
                    Float scale = Float(1) / std::numeric_limits<T>::max();
                    R *= scale; G *= scale; B *= scale;
                }

                Imath::V3f rgb = Imath::V3f(float(R), float(G), float(B)) * M;
                R = Float(rgb[0]); G = Float(rgb[1]); B = Float(rgb[2]);

                if (std::is_integral<T>::value) {
                    Float scale = std::numeric_limits<T>::max();
                    R *= R * scale + 0.5f;
                    G *= G * scale + 0.5f;
                    B *= B * scale + 0.5f;
                }

                data[0] = T(R); data[1] = T(G); data[2] = T(B);
                data += channel_count();
            }
        };

        switch (m_component_format) {
            case Struct::EFloat16: convert((enoki::half *) m_data.get()); break;
            case Struct::EFloat32: convert((float *)       m_data.get()); break;
            case Struct::EUInt32:  convert((uint32_t*)     m_data.get()); break;
            default: Throw("Internal error!");
        }
    }
}

void Bitmap::write_openexr(Stream *stream, int quality) const {
    if (Imf::globalThreadCount() == 0)
        Imf::setGlobalThreadCount(std::min(8, util::core_count()));

    EPixelFormat pixel_format = m_pixel_format;

    Properties metadata(m_metadata);
    if (!metadata.has_property("generated_by"))
        metadata.set_string("generated_by", "Mitsuba version " MTS_VERSION);

    std::vector<std::string> keys = metadata.property_names();

    Imf::Header header(
        m_size.x(), // width
        m_size.y(), // height,
        1.f, // pixelAspectRatio
        Imath::V2f(0, 0), // screenWindowCenter,
        1.f, // screenWindowWidth
        Imf::INCREASING_Y, // lineOrder
        quality <= 0 ? Imf::PIZ_COMPRESSION : Imf::DWAB_COMPRESSION // compression
    );

    if (quality > 0)
        Imf::addDwaCompressionLevel(header, float(quality));

    for (auto it = keys.begin(); it != keys.end(); ++it) {
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
                    Matrix4f val = metadata.transform(*it).matrix();
                    header.insert(it->c_str(), Imf::M44fAttribute(Imath::M44f(
                        (float) val(0, 0), (float) val(0, 1), (float) val(0, 2), (float) val(0, 3),
                        (float) val(1, 0), (float) val(1, 1), (float) val(1, 2), (float) val(1, 3),
                        (float) val(2, 0), (float) val(2, 1), (float) val(2, 2), (float) val(2, 3),
                        (float) val(3, 0), (float) val(3, 1), (float) val(3, 2), (float) val(3, 3))));
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
    }

    Imf::PixelType comp_type;
    size_t comp_stride;
    if (m_component_format == Struct::EFloat16) {
        comp_type = Imf::HALF;
        comp_stride = 2;
    } else if (m_component_format == Struct::EFloat32) {
        comp_type = Imf::FLOAT;
        comp_stride = 4;
    } else if (m_component_format == Struct::EUInt32) {
        comp_type = Imf::UINT;
        comp_stride = 4;
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
            channels.insert(it->name.c_str(), Imf::Channel(comp_type));
        explicit_channel_names = true;
    } else if (pixel_format == ELuminance || pixel_format == ELuminanceAlpha) {
        channels.insert("Y", Imf::Channel(comp_type));
    } else if (pixel_format == ERGB || pixel_format == ERGBA ||
               pixel_format == EXYZ || pixel_format == EXYZA) {
        channels.insert("R", Imf::Channel(comp_type));
        channels.insert("G", Imf::Channel(comp_type));
        channels.insert("B", Imf::Channel(comp_type));
#if 0 // TODO
    } else if (pixel_format == EMultiChannel) {
        for (int i = 0; i<getChannelCount(); ++i)
            channels.insert(tfm::format("%i", i).c_str(), Imf::Channel(comp_type));
#endif
    } else {
        Log(EError, "writeOpenEXR(): Invalid pixel format!");
        return;
    }

    if ((pixel_format == ELuminanceAlpha || pixel_format == ERGBA ||
         pixel_format == EXYZA /*|| pixel_format == ESpectrumAlpha*/) && !explicit_channel_names)
        channels.insert("A", Imf::Channel(comp_type));

    size_t pixel_stride = channel_count() * comp_stride,
        row_stride = pixel_stride * m_size.x();
    char *ptr = (char *) m_data.get();

    Imf::FrameBuffer framebuffer;

    if (explicit_channel_names) {
        for (auto it = m_struct->begin(); it != m_struct->end(); ++it) {
            framebuffer.insert(it->name.c_str(), Imf::Slice(comp_type, ptr, pixel_stride, row_stride));

            ptr += comp_stride;
        }
    } else if (pixel_format == ELuminance || pixel_format == ELuminanceAlpha) {
        framebuffer.insert("Y", Imf::Slice(comp_type, ptr, pixel_stride, row_stride)); ptr += comp_stride;
    } else if (pixel_format == ERGB || pixel_format == ERGBA || pixel_format == EXYZ || pixel_format == EXYZA) {
        framebuffer.insert("R", Imf::Slice(comp_type, ptr, pixel_stride, row_stride)); ptr += comp_stride;
        framebuffer.insert("G", Imf::Slice(comp_type, ptr, pixel_stride, row_stride)); ptr += comp_stride;
        framebuffer.insert("B", Imf::Slice(comp_type, ptr, pixel_stride, row_stride)); ptr += comp_stride;
#if 0 // TODO
    } else if (pixel_format == ESpectrum || pixel_format == ESpectrumAlpha) {
        for (int i = 0; i<SPECTRUM_SAMPLES; ++i) {
            std::pair<Float, Float> coverage = Spectrum::getBinCoverage(i);
            std::string name = tfm::format("%.2f-%.2fnm", coverage.first, coverage.second);
            framebuffer.insert(name.c_str(), Imf::Slice(comp_type, ptr, pixel_stride, row_stride)); ptr += comp_stride;
        }
    } else if (pixel_format == EMultiChannel) {
        for (int i = 0; i<getChannelCount(); ++i) {
            framebuffer.insert(tfm::format("%i", i).c_str(), Imf::Slice(comp_type, ptr, pixel_stride, row_stride));
            ptr += comp_stride;
        }
#endif
    }

    if ((pixel_format == ELuminanceAlpha || pixel_format == ERGBA ||
         pixel_format == EXYZA /*|| pixel_format == ESpectrumAlpha*/) && !explicit_channel_names)
        framebuffer.insert("A", Imf::Slice(comp_type, ptr, pixel_stride, row_stride));

    EXROStream ostr(stream);
    Imf::OutputFile file(ostr, header);
    file.setFrameBuffer(framebuffer);
    file.writePixels(m_size.y());
}

// -----------------------------------------------------------------------------

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

    //METHODDEF(void) jpeg_init_source(j_decompress_ptr cinfo) {
        //jbuf_in_t *p = (jbuf_in_t *) cinfo->src;
        //p->buffer = new JOCTET[jpeg_bufferSize];
    //}

    //METHODDEF(boolean) jpeg_fill_input_buffer(j_decompress_ptr cinfo) {
        //jbuf_in_t *p = (jbuf_in_t *) cinfo->src;
        //size_t nBytes;

        //p->stream->read(p->buffer, jpeg_bufferSize);
        //nBytes = jpeg_bufferSize;

        //try {
            //p->stream->read(p->buffer, jpeg_bufferSize);
            //nBytes = jpeg_bufferSize;
        //} catch (const EOFException &e) {
            //nBytes = e.getCompleted();
            //if (nBytes == 0) {
                //[> Insert a fake EOI marker <]
                //p->buffer[0] = (JOCTET) 0xFF;
                //p->buffer[1] = (JOCTET) JPEG_EOI;
                //nBytes = 2;
            //}
        //}

        //cinfo->src->bytes_in_buffer = nBytes;
        //cinfo->src->next_input_byte = p->buffer;
        //return TRUE;
    //}

    //METHODDEF(void) jpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes) {
        //if (num_bytes > 0) {
            //while (num_bytes > (long) cinfo->src->bytes_in_buffer) {
                //num_bytes -= (long) cinfo->src->bytes_in_buffer;
                //jpeg_fill_input_buffer(cinfo);
            //}
            //cinfo->src->next_input_byte += (size_t) num_bytes;
            //cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
        //}
    //}

    //METHODDEF(void) jpeg_term_source(j_decompress_ptr cinfo) {
        //jbuf_in_t *p = (jbuf_in_t *) cinfo->src;
        //delete[] p->buffer;
    //}

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
        Log(EError, "write_jpeg(): Unsupported pixel format!");

    if (m_component_format != Struct::EUInt8)
        Log(EError, "write_jpeg(): Unsupported component format!");

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
    for (size_t i = 0; i < m_size.y(); ++i) {
        uint8_t *source =
            m_data.get() + i * m_size.x() * cinfo.input_components;
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
        //case Struct::EBitmask: bitDepth = 1; break;
        case Struct::EUInt8: bitDepth = 8; break;
        case Struct::EUInt16: bitDepth = 16; break;
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

    if (m_component_format == Struct::EUInt16 && Stream::host_byte_order() == Stream::ELittleEndian)
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


std::ostream &operator<<(std::ostream &os, Bitmap::EPixelFormat value) {
    switch (value) {
        case Bitmap::ELuminance:      os << "luminance"; break;
        case Bitmap::ELuminanceAlpha: os << "luminance-alpha"; break;
        case Bitmap::ERGB:            os << "rgb"; break;
        case Bitmap::ERGBA:           os << "rgba"; break;
        case Bitmap::EXYZ:            os << "xyz"; break;
        case Bitmap::EXYZA:           os << "xyza"; break;
        case Bitmap::EMultiChannel:   os << "multichannel"; break;
        default: Throw("Unknown pixel format!");
    }
    return os;
}


std::ostream &operator<<(std::ostream &os, Bitmap::EFileFormat value) {
    switch (value) {
        case Bitmap::EPNG:     os << "PNG"; break;
        case Bitmap::EOpenEXR: os << "OpenEXR"; break;
        case Bitmap::EJPEG:    os << "JPEG"; break;
        case Bitmap::EBMP:     os << "BMP"; break;
        case Bitmap::EPFM:     os << "PFM"; break;
        case Bitmap::EPPM:     os << "PPM"; break;
        case Bitmap::ERGBE:    os << "RGBE"; break;
        case Bitmap::EAuto:    os << "Auto"; break;
        default: Throw("Unknown file format!");
    }
    return os;
}

void Bitmap::static_initialization() {
    /* No-op */
}

void Bitmap::static_shutdown() {
    Imf::setGlobalThreadCount(0);
}

MTS_IMPLEMENT_CLASS(Bitmap, Object)
NAMESPACE_END(mitsuba)
