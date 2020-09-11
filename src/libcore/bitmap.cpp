#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/profiler.h>
#include <unordered_map>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

#define __TBB_show_deprecation_message_task_H 1
#include <tbb/task.h>

#include <enoki/half.h>

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
#elif defined(__GNUG__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated"
#endif

#if defined(_MSC_VER)
#   pragma warning(disable : 4611) // interaction between '_setjmp' and C++ object destruction is non-portable
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
#elif defined(__GNUG__)
#  pragma GCC diagnostic pop
#endif

NAMESPACE_BEGIN(mitsuba)

Bitmap::Bitmap(PixelFormat pixel_format, Struct::Type component_format,
               const Vector2u &size, size_t channel_count, uint8_t *data)
    : m_data(data), m_pixel_format(pixel_format),
      m_component_format(component_format), m_size(size), m_owns_data(false) {

    if (m_component_format == Struct::Type::UInt8)
        m_srgb_gamma = true;  // sRGB by default
    else
        m_srgb_gamma = false; // Linear by default

    // By default assume alpha to be premultiplied (since that's how we render)
    m_premultiplied_alpha = true;

    rebuild_struct(channel_count);

    if (!m_data) {
        m_data = std::unique_ptr<uint8_t[]>(new uint8_t[buffer_size()]);

        m_owns_data = true;
    }
}

Bitmap::Bitmap(const Bitmap &bitmap)
    : Object(), m_pixel_format(bitmap.m_pixel_format),
      m_component_format(bitmap.m_component_format),
      m_size(bitmap.m_size),
      m_struct(new Struct(*bitmap.m_struct)),
      m_srgb_gamma(bitmap.m_srgb_gamma),
      m_owns_data(true) {
    size_t size = buffer_size();
    m_data = std::unique_ptr<uint8_t[]>(new uint8_t[size]);
    memcpy(m_data.get(), bitmap.m_data.get(), size);
}


Bitmap::Bitmap(Bitmap &&bitmap)
    : m_data(std::move(bitmap.m_data)),
      m_pixel_format(bitmap.m_pixel_format),
      m_component_format(bitmap.m_component_format),
      m_size(bitmap.m_size),
      m_struct(std::move(bitmap.m_struct)),
      m_srgb_gamma(bitmap.m_srgb_gamma),
      m_owns_data(bitmap.m_owns_data) {
}

Bitmap::Bitmap(Stream *stream, FileFormat format) {
    read(stream, format);
}

Bitmap::Bitmap(const fs::path &filename, FileFormat format) {
    ref<FileStream> fs = new FileStream(filename);
    read(fs, format);
}

Bitmap::~Bitmap() {
    if (!m_owns_data)
        m_data.release();
}

void Bitmap::set_srgb_gamma(bool value) {
    m_srgb_gamma = value;
    for (auto &field : *m_struct) {
        std::string suffix = field.name;
        auto it = suffix.rfind(".");
        if (it != std::string::npos)
            suffix = suffix.substr(it + 1);
        if (suffix != "A" && suffix != "W") {
            if (value)
                field.flags |= +Struct::Flags::Gamma;
            else
                field.flags &= ~Struct::Flags::Gamma;
        }
    }
}


void Bitmap::set_premultiplied_alpha(bool value) {
    m_premultiplied_alpha = value;
    for (auto &field : *m_struct) {
        std::string suffix = field.name;
        auto it = suffix.rfind(".");
        if (it != std::string::npos)
            suffix = suffix.substr(it + 1);
        if (suffix != "A" && suffix != "W") {
            if (value)
                field.flags |= +Struct::Flags::PremultipliedAlpha;
            else
                field.flags &= ~Struct::Flags::PremultipliedAlpha;
        }
    }
}

void Bitmap::clear() {
    memset(m_data.get(), 0, buffer_size());
}

void Bitmap::rebuild_struct(size_t channel_count) {
    std::vector<std::string> channels;

    switch (m_pixel_format) {
        case PixelFormat::Y:     channels = { "Y" };                    break;
        case PixelFormat::YA:    channels = { "Y", "A" };               break;
        case PixelFormat::RGB:   channels = { "R", "G", "B"};           break;
        case PixelFormat::RGBA:  channels = { "R", "G", "B", "A"};      break;
        case PixelFormat::XYZ:   channels = { "X", "Y", "Z"};           break;
        case PixelFormat::XYZA:  channels = { "X", "Y", "Z", "A"};      break;
        case PixelFormat::XYZAW: channels = { "X", "Y", "Z", "A", "W"}; break;
        case PixelFormat::MultiChannel: {
                for (size_t i = 0; i < channel_count; ++i)
                    channels.push_back(tfm::format("ch%i", i));
            }
            break;
        default: Throw("Unknown pixel format!");
    }

    if (channel_count != 0 && channel_count != channels.size())
        Throw("Bitmap::rebuild_struct(): channel count (%i) does not match "
              "pixel format (%s)!", channel_count, m_pixel_format);

    m_struct = new Struct();
    for (auto ch: channels) {
        uint32_t flags = +Struct::Flags::None;
        if (ch != "A" && ch != "W" && m_srgb_gamma)
            flags |= +Struct::Flags::Gamma;
        if (ch != "A" && ch != "W" && m_premultiplied_alpha)
            flags |= +Struct::Flags::PremultipliedAlpha;
        if (ch == "A")
            flags |= +Struct::Flags::Alpha;
        if (ch == "W")
            flags |= +Struct::Flags::Weight;
        if (Struct::is_integer(m_component_format))
            flags |= +Struct::Flags::Normalized;
        m_struct->append(ch, m_component_format, flags);
    }
}

size_t Bitmap::buffer_size() const {
    return pixel_count() * bytes_per_pixel();
}

size_t Bitmap::bytes_per_pixel() const {
    size_t result;
    switch (m_component_format) {
        case Struct::Type::Int8:
        case Struct::Type::UInt8:   result = 1; break;
        case Struct::Type::Int16:
        case Struct::Type::UInt16:  result = 2; break;
        case Struct::Type::Int32:
        case Struct::Type::UInt32:  result = 4; break;
        case Struct::Type::Int64:
        case Struct::Type::UInt64:  result = 8; break;
        case Struct::Type::Float16: result = 2; break;
        case Struct::Type::Float32: result = 4; break;
        case Struct::Type::Float64: result = 8; break;
        default: Throw("Unknown component format: %d", m_component_format);
    }
    return result * channel_count();
}

void Bitmap::vflip() {
    size_t row_size = buffer_size() / m_size.y();
    size_t half_height = m_size.y() / 2;
    uint8_t *temp = (uint8_t *) alloca(row_size);
    uint8_t *data = uint8_data();
    for (size_t i = 0, j = m_size.y() - 1; i < half_height; ++i) {
        memcpy(temp, data + i * row_size, row_size);
        memcpy(data + i * row_size, data + j * row_size, row_size);
        memcpy(data + j * row_size, temp, row_size);
        j--;
    }
}

template <typename Scalar, bool Filter,
          typename ReconstructionFilter = typename Bitmap::ReconstructionFilter>
static void
resample(Bitmap *target, const Bitmap *source,
         const ReconstructionFilter *rfilter_,
         const std::pair<FilterBoundaryCondition, FilterBoundaryCondition> bc,
         const std::pair<Scalar, Scalar> &clamp, ref<Bitmap> temp) {
    using Vector2u = typename Bitmap::Vector2u;

    ref<const ReconstructionFilter> rfilter(rfilter_);

    if (!rfilter) {
        // Upsample using a 2-lobed Lanczos reconstruction filter
        Properties props("lanczos");
        props.set_int("lobes", 2);
        rfilter = PluginManager::instance()->create_object<ReconstructionFilter>(props);
    }

    if (source->height() == target->height() &&
        source->width() == target->width() && !Filter) {
        memcpy(target->uint8_data(), source->uint8_data(), source->buffer_size());
        return;
    }

    size_t channels = source->channel_count();

    if (source->width() != target->width() || Filter) {
        // Re-sample horizontally
        Resampler<Scalar> r(rfilter, source->width(), target->width());
        r.set_boundary_condition(bc.first);
        r.set_clamp(clamp);

        // Create a bitmap for intermediate storage
        if (!temp) {
            if (source->height() == target->height() && !Filter) {
                // write directly to the output bitmap
                temp = target;
            } else {
                // otherwise: write to a temporary bitmap
                temp = new Bitmap(
                    source->pixel_format(), source->component_format(),
                    Vector2u(target->width(), source->height()), channels);
            }
        }

        tbb::parallel_for(
            tbb::blocked_range<size_t>(0, source->height(), 100),
            [&](const tbb::blocked_range<size_t> &range) {
                for (auto y = range.begin(); y != range.end(); ++y) {
                    const Scalar *s = (const Scalar *) source->uint8_data() +
                                      y * (size_t) source->width() * channels;
                    Scalar *t       = (Scalar *) temp->uint8_data() +
                                      y * (size_t) target->width() * channels;
                    r.resample(s, 1, t, 1, (uint32_t) channels);
                }
            }
        );

        // Now, read from the temporary bitmap
        source = temp;
    }

    if (source->height() != target->height() || Filter) {
        // Re-sample vertically
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
                    r.resample(s, source->width(), t, target->width(), (uint32_t) channels);
                }
            }
        );
    }
}

void Bitmap::resample(Bitmap *target, const ReconstructionFilter *rfilter,
                      const std::pair<FilterBoundaryCondition, FilterBoundaryCondition> &bc,
                      const std::pair<Float, Float> &clamp, Bitmap *temp) const {
    if (pixel_format() != target->pixel_format() ||
        component_format() != target->component_format() ||
        channel_count() != target->channel_count())
        Throw("Bitmap::resample(): Incompatible source and target bitmaps!");

    if (temp && (pixel_format() != temp->pixel_format() ||
                 component_format() != temp->component_format() ||
                 channel_count() != temp->channel_count() ||
                 temp->size() != Vector2u(target->width(), height())))
        Throw("Bitmap::resample(): Incompatible temporary bitmap specified!");

    switch (m_component_format) {
        case Struct::Type::Float16:
            mitsuba::resample<ek::half, false>(target, this, rfilter, bc,
                                                  clamp, temp);
            break;

        case Struct::Type::Float32:
            mitsuba::resample<float, false>(target, this, rfilter, bc,
                                            clamp, temp);
            break;

        case Struct::Type::Float64:
            mitsuba::resample<double, false>(target, this, rfilter, bc,
                                             clamp, temp);
            break;

        default:
            Throw("resample(): Unsupported component type! (must be float16/32/64)");
    }
}

ref<Bitmap> Bitmap::resample(const Vector2u &res, const ReconstructionFilter *rfilter,
                             const std::pair<FilterBoundaryCondition, FilterBoundaryCondition> &bc,
                             const std::pair<Float, Float> &bound) const {
    ref<Bitmap> result =
        new Bitmap(m_pixel_format, m_component_format, res, channel_count());
    result->m_struct = m_struct;
    result->m_metadata = m_metadata;
    resample(result, rfilter, bc, bound, nullptr);
    return result;
}

ref<Bitmap> Bitmap::convert(PixelFormat pixel_format,
                            Struct::Type component_format,
                            bool srgb_gamma, Bitmap::AlphaTransform alpha_transform) const {
    ref<Bitmap> result = new Bitmap(
        pixel_format, component_format, m_size,
        m_pixel_format == PixelFormat::MultiChannel ? m_struct->field_count() : 0);

    switch (alpha_transform) {
        case Bitmap::AlphaTransform::None:
            result->set_premultiplied_alpha(m_premultiplied_alpha);
            break;
        case Bitmap::AlphaTransform::Premultiply:
            result->set_premultiplied_alpha(true);
            break;
        case Bitmap::AlphaTransform::Unpremultiply:
            result->set_premultiplied_alpha(false);
            break;
    }

    result->m_metadata = m_metadata;
    result->set_srgb_gamma(srgb_gamma);
    convert(result);
    return result;
}

void Bitmap::convert(Bitmap *target) const {
    if (m_size != target->size())
        Throw("Bitmap::convert(): Incompatible target size!"
              " This: %s vs target: %s)", m_size, target->size());

    ref<Struct> target_struct = new Struct(*(target->struct_()));

    bool source_is_rgb = m_pixel_format == PixelFormat::RGB ||
                         m_pixel_format == PixelFormat::RGBA;
    bool source_is_xyz = m_pixel_format == PixelFormat::XYZ ||
                         m_pixel_format == PixelFormat::XYZA ||
                         m_pixel_format == PixelFormat::XYZAW;
    bool source_is_y   = m_pixel_format == PixelFormat::Y ||
                         m_pixel_format == PixelFormat::YA;

    for (auto &field: *target_struct) {
        if (m_struct->has_field(field.name))
            continue;
        if (!field.blend.empty())
            continue;

        std::string prefix = "";
        std::string suffix = field.name;
        auto it = suffix.rfind(".");
        if (it != std::string::npos) {
            prefix = suffix.substr(0, it + 1);
            suffix = suffix.substr(it + 1);
        }

        // Set alpha/weight to 1.0 by default
        if (suffix == "A" || suffix == "W") {
            field.default_ = 1.0;
            field.flags |= +Struct::Flags::Default;
            continue;
        }

        if (suffix == "R") {
            if (source_is_xyz) {
                field.blend = {
                    {  3.240479f, prefix + "X" },
                    { -1.537150f, prefix + "Y" },
                    { -0.498535f, prefix + "Z" }
                };
                continue;
            } else if (source_is_y) {
                field.name = prefix + "Y";
                continue;
            }
        } else if (suffix == "G") {
            if (source_is_xyz) {
                field.blend = {
                    { -0.969256, prefix + "X" },
                    {  1.875991, prefix + "Y" },
                    {  0.041556, prefix + "Z" }
                };
                continue;
            } else if (source_is_y) {
                field.name = prefix + "Y";
                continue;
            }
        } else if (suffix == "B") {
            if (source_is_xyz) {
                field.blend = {
                    {  0.055648, prefix + "X" },
                    { -0.204043, prefix + "Y" },
                    {  1.057311, prefix + "Z" }
                };
                continue;
            } else if (source_is_y) {
                field.name = prefix + "Y";
                continue;
            }
        } else if (suffix == "X") {
            if (source_is_rgb) {
                field.blend = {
                    { 0.412453, prefix + "R" },
                    { 0.357580, prefix + "G" },
                    { 0.180423, prefix + "B" }
                };
                continue;
            } else if (source_is_y) {
                field.blend = {{ 0.950456, prefix + "Y" }};
                continue;
            }
        } else if (suffix == "Y") {
            if (source_is_rgb) {
                field.blend = {
                    { 0.212671, prefix + "R" },
                    { 0.715160, prefix + "G" },
                    { 0.072169, prefix + "B" }
                };
                continue;
            } else if (source_is_y) {
                field.blend = {{ 1.0, prefix + "Y" }};
                continue;
            }
        } else if (suffix == "Z") {
            if (source_is_rgb) {
                field.blend = {
                    { 0.019334, prefix + "R" },
                    { 0.119193, prefix + "G" },
                    { 0.950227, prefix + "B" }
                };
                continue;
            } else if (source_is_y) {
                field.blend = {{ 1.08875, prefix + "Y" }};
                continue;
            }
        }

        Throw("Unable to convert %s to %s: don't know how to obtain channel \"%s\".",
              m_struct, target_struct, field.name);
    }

    StructConverter conv(m_struct, target_struct, true);
    bool rv = conv.convert_2d(m_size.x(), m_size.y(), uint8_data(), target->uint8_data());
    if (!rv)
        Throw("Bitmap::convert(): conversion kernel indicated a failure!");
}


void Bitmap::accumulate(const Bitmap *source,
                        Point2i source_offset,
                        Point2i target_offset,
                        Vector2i size) {
    if (unlikely(pixel_format()     != source->pixel_format()     ||
                 component_format() != source->component_format() ||
                 channel_count()    != source->channel_count()))
        Throw("Bitmap::accumulate(): source and target are incompatible!");

    switch (m_component_format) {
        case Struct::Type::UInt8:
            accumulate_2d((const uint8_t *) source->data(), source->size(), (uint8_t *) data(),
                          m_size, source_offset, target_offset, size, channel_count());
            break;

        case Struct::Type::UInt16:
            accumulate_2d((const uint16_t *) source->data(), source->size(), (uint16_t *) data(),
                          m_size, source_offset, target_offset, size, channel_count());
            break;

        case Struct::Type::UInt32:
            accumulate_2d((const uint32_t *) source->data(), source->size(), (uint32_t *) data(),
                          m_size, source_offset, target_offset, size, channel_count());
            break;

        case Struct::Type::Float16:
            accumulate_2d((const ek::half *) source->data(), source->size(), (ek::half *) data(),
                          m_size, source_offset, target_offset, size, channel_count());
            break;

        case Struct::Type::Float32:
            accumulate_2d((const float *) source->data(), source->size(), (float *) data(),
                          m_size, source_offset, target_offset, size, channel_count());
            break;

        case Struct::Type::Float64:
            accumulate_2d((const double *) source->data(), source->size(), (double *) data(),
                          m_size, source_offset, target_offset, size, channel_count());
            break;

        default:
            Throw("Unknown component format: %d", m_component_format);
    }
}

std::vector<std::pair<std::string, ref<Bitmap>>> Bitmap::split() const {
    std::vector<std::pair<std::string, ref<Bitmap>>> result;

    if (m_pixel_format != PixelFormat::MultiChannel) {
        result.emplace_back("<root>", const_cast<Bitmap *>(this));
        return result;
    }

    using FieldMap = std::unordered_multimap<
          std::string,
          std::pair<std::string, const Struct::Field *>>;

    FieldMap fields;
    for (size_t i = 0; i < m_struct->field_count(); ++i) {
        const Struct::Field &field = (*m_struct)[i];
        auto it = field.name.rfind(".");

        std::string prefix, suffix;
        if (it != std::string::npos) {
            prefix = field.name.substr(0, it);
            suffix = field.name.substr(it + 1, field.name.length());
        } else {
            prefix = "<root>";
            suffix = field.name;
        }
        fields.emplace(prefix, std::make_pair(suffix, &field));
    }

    for (auto it = fields.begin(); it != fields.end();) {
        std::string prefix = it->first;
        auto range = fields.equal_range(prefix);

        FieldMap::iterator r = fields.end(), g = fields.end(),
                           b = fields.end(), a = fields.end(),
                           x = fields.end(), y = fields.end(),
                           z = fields.end();

        for (auto it2 = range.first; it2 != range.second; ++it2) {
            if (it2->second.first == "R")
                r = it2;
            else if (it2->second.first == "G")
                g = it2;
            else if (it2->second.first == "B")
                b = it2;
            else if (it2->second.first == "A")
                a = it2;
            else if (it2->second.first == "X")
                x = it2;
            else if (it2->second.first == "Y")
                y = it2;
            else if (it2->second.first == "Z")
                z = it2;
        }

        bool has_rgb = r != fields.end() &&
                       g != fields.end() &&
                       b != fields.end(),
             has_xyz = x != fields.end() &&
                       y != fields.end() &&
                       z != fields.end(),
             has_y   = y != fields.end() && !has_xyz,
             has_a   = a != fields.end();

        if (has_rgb || has_xyz || has_y) {
            ref<Bitmap> target = new Bitmap(
                has_rgb
                ? (has_a ? PixelFormat::RGBA
                         : PixelFormat::RGB)
                : (has_xyz
                   ? (has_a ? PixelFormat::XYZA
                            : PixelFormat::XYZ)
                   : (has_a ? PixelFormat::YA
                            : PixelFormat::Y)),
                m_component_format,
                m_size
            );
            target->set_srgb_gamma(m_srgb_gamma);
            target->set_premultiplied_alpha(m_premultiplied_alpha);

            ref<Struct> target_struct = new Struct(*target->struct_());
            auto link_field = [&](const char *l, FieldMap::iterator it) {
                target_struct->field(l).name = it->second.second->name;
                fields.erase(it);
            };

            if (has_rgb) {
                link_field("R", r);
                link_field("G", g);
                link_field("B", b);
            } else if (has_xyz) {
                link_field("X", x);
                link_field("Y", y);
                link_field("Z", z);
            } else {
                link_field("Y", y);
            }
            if (has_a)
                link_field("A", a);

            StructConverter conv(m_struct, target_struct, true);
            bool rv = conv.convert_2d(m_size.x(), m_size.y(),
                                      uint8_data(), target->uint8_data());
            if (!rv)
                Throw("Bitmap::split(): conversion kernel indicated a failure!");
            result.push_back({ prefix, target });
        }

        it = range.second;
    }

    for (auto it = fields.begin(); it != fields.end(); ++it) {
        ref<Bitmap> target = new Bitmap(
            PixelFormat::Y,
            m_component_format,
            m_size
        );
        target->set_srgb_gamma(m_srgb_gamma);
        ref<Struct> target_struct = new Struct(*target->struct_());
        target_struct->field("Y").name = it->second.second->name;
        StructConverter conv(m_struct, target_struct, true);
        bool rv = conv.convert_2d(m_size.x(), m_size.y(),
                                  uint8_data(), target->uint8_data());
        if (!rv)
            Throw("Bitmap::split(): conversion kernel indicated a failure!");
        result.push_back({ it->first + "." + it->second.first, target });
    }

    std::sort(result.begin(), result.end(),
              [](const std::pair<std::string, ref<Bitmap>> &v1,
                 const std::pair<std::string, ref<Bitmap>> &v2) {
                  return v1.first < v2.first;
              });

    return result;
}

void Bitmap::read(Stream *stream, FileFormat format) {
    if (format == FileFormat::Auto)
        format = detect_file_format(stream);

    switch (format) {
        case FileFormat::BMP:     read_bmp(stream);     break;
        case FileFormat::JPEG:    read_jpeg(stream);    break;
        case FileFormat::OpenEXR: read_openexr(stream); break;
        case FileFormat::RGBE:    read_rgbe(stream);    break;
        case FileFormat::PFM:     read_pfm(stream);     break;
        case FileFormat::PPM:     read_ppm(stream);     break;
        case FileFormat::TGA:     read_tga(stream);     break;
        case FileFormat::PNG:     read_png(stream);     break;
        default:
            Throw("Bitmap: Unknown file format!");
    }
}

Bitmap::FileFormat Bitmap::detect_file_format(Stream *stream) {
    FileFormat format = FileFormat::Unknown;

    // Try to automatically detect the file format
    size_t pos = stream->tell();
    uint8_t start[8];
    stream->read(start, 8);

    if (start[0] == 'B' && start[1] == 'M') {
        format = FileFormat::BMP;
    } else if (start[0] == '#' && start[1] == '?') {
        format = FileFormat::RGBE;
    } else if (start[0] == 'P' && (start[1] == 'F' || start[1] == 'f')) {
        format = FileFormat::PFM;
    } else if (start[0] == 'P' && start[1] == '6') {
        format = FileFormat::PPM;
    } else if (start[0] == 0xFF && start[1] == 0xD8) {
        format = FileFormat::JPEG;
    } else if (png_sig_cmp(start, 0, 8) == 0) {
        format = FileFormat::PNG;
    } else if (Imf::isImfMagic((const char *) start)) {
        format = FileFormat::OpenEXR;
    } else {
        // Check for a TGAv2 file
        char footer[18];
        stream->seek(stream->size() - 18);
        stream->read(footer, 18);
        if (footer[17] == 0 && strncmp(footer, "TRUEVISION-XFILE.", 17) == 0)
            format = FileFormat::TGA;
    }
    stream->seek(pos);
    return format;
}

void Bitmap::write(const fs::path &path, FileFormat format, int quality) const {
    ref<FileStream> fs = new FileStream(path, FileStream::ETruncReadWrite);
    write(fs, format, quality);
}

void Bitmap::write(Stream *stream, FileFormat format, int quality) const {
    auto fs = dynamic_cast<FileStream *>(stream);

    if (format == FileFormat::Auto) {
        if (!fs)
            Throw("Bitmap::write(): can't decide file format based on filename "
                  "since the target stream is not a file stream");
        std::string extension = string::to_lower(fs->path().extension().string());
        if (extension == ".exr")
            format = FileFormat::OpenEXR;
        else if (extension == ".png")
            format = FileFormat::PNG;
        else if (extension == ".jpg" || extension == ".jpeg")
            format = FileFormat::JPEG;
        else if (extension == ".hdr" || extension == ".rgbe")
            format = FileFormat::RGBE;
        else if (extension == ".pfm")
            format = FileFormat::PFM;
        else if (extension == ".ppm")
            format = FileFormat::PPM;
        else
            Throw("Bitmap::write(): unsupported bitmap file extension \"%s\"",
                  extension);
    }

    Log(Debug, "Writing %s file \"%s\" (%ix%i, %s, %s) ..",
        format, fs ? fs->path().string() : "<stream>",
        m_size.x(), m_size.y(),
        m_pixel_format, m_component_format
    );

    switch (format) {
        case FileFormat::OpenEXR:
            write_openexr(stream, quality);
            break;

        case FileFormat::PNG:
            if (quality == -1)
                quality = 5;
            write_png(stream, quality);
            break;

        case FileFormat::JPEG:
            if (quality == -1)
                quality = 100;
            write_jpeg(stream, quality);
            break;

        case FileFormat::PPM:
            write_ppm(stream);
            break;

        case FileFormat::RGBE:
            write_rgbe(stream);
            break;

        case FileFormat::PFM:
            write_pfm(stream);
            break;

        default:
            Throw("Bitmap::write(): Invalid file format!");
    }
}

void Bitmap::write_async(const fs::path &path_, FileFormat format_, int quality_) const {
    class WriteTask : public tbb::task {
        ref<const Bitmap> bitmap;
        fs::path path;
        FileFormat format;
        int quality;

    public:
        WriteTask(const Bitmap *bitmap, fs::path path, FileFormat format, int quality)
            : bitmap(bitmap), path(path), format(format), quality(quality) { }

        tbb::task* execute() override {
            bitmap->write(path, format, quality);
            return nullptr;
        }
    };

    WriteTask *t = new (tbb::task::allocate_root())
        WriteTask(this, path_, format_, quality_);
    tbb::task::enqueue(*t);
}

bool Bitmap::operator==(const Bitmap &bitmap) const {
    if (m_pixel_format != bitmap.m_pixel_format ||
        m_component_format != bitmap.m_component_format ||
        m_size != bitmap.m_size ||
        m_srgb_gamma != bitmap.m_srgb_gamma ||
        m_premultiplied_alpha != bitmap.m_premultiplied_alpha ||
        *m_struct != *bitmap.m_struct ||
        m_metadata != bitmap.m_metadata)
        return false;
    return memcmp(uint8_data(), bitmap.uint8_data(), buffer_size()) == 0;
}

std::string Bitmap::to_string() const {
    std::ostringstream oss;
    oss << "Bitmap[" << std::endl
        << "  type = " << m_pixel_format << "," << std::endl
        << "  component_format = " << m_component_format << "," << std::endl
        << "  size = " << m_size << "," << std::endl
        << "  srgb_gamma = " << m_srgb_gamma << "," << std::endl
        << "  struct = " << string::indent(m_struct) << "," << std::endl;

    std::vector<std::string> keys = m_metadata.property_names();
    if (!keys.empty()) {
        oss << "  metadata = {" << std::endl;
        for (auto it = keys.begin(); it != keys.end(); ) {
            std::string value = m_metadata.as_string(*it);
            if (value.length() > 50) {
                value = value.substr(0, 50);
                if (value[0] == '\"')
                    value += '\"';
                value += ".. [truncated]";
            }
            oss << "    " << *it << " => " << value;
            if (++it != keys.end())
                oss << ",";
            oss << std::endl;
        }
        oss << "  }," << std::endl;
    }

    oss << "  data = [ " << util::mem_string(buffer_size()) << " of image data ]" << std::endl
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
    ScopedPhase phase(ProfilerPhase::BitmapRead);
    if (Imf::globalThreadCount() == 0)
        Imf::setGlobalThreadCount(std::min(8, util::core_count()));

    EXRIStream istr(stream);
    Imf::InputFile file(istr);

    const Imf::Header &header = file.header();
    const Imf::ChannelList &channels = header.channels();

    if (channels.begin() == channels.end())
        Throw("read_openexr(): Image does not contain any channels!");

    // Load metadata if present
    for (auto it = header.begin(); it != header.end(); ++it) {
        std::string name = it.name();
        const Imf::Attribute *attr = &it.attribute();
        std::string type_name = attr->typeName();

        if (type_name == "string") {
            auto v = static_cast<const Imf::StringAttribute *>(attr);
            m_metadata.set_string(name, v->value());
        } else if (type_name == "int") {
            auto v = static_cast<const Imf::IntAttribute *>(attr);
            m_metadata.set_long(name, v->value());
        } else if (type_name == "float") {
            auto v = static_cast<const Imf::FloatAttribute *>(attr);
            m_metadata.set_float(name, Float(v->value()));
        } else if (type_name == "double") {
            auto v = static_cast<const Imf::DoubleAttribute *>(attr);
            m_metadata.set_float(name, Float(v->value()));
        } else if (type_name == "v3f") {
            auto v = static_cast<const Imf::V3fAttribute *>(attr);
            Imath::V3f vec = v->value();
            m_metadata.set_array3f(name, Vector3f(vec.x, vec.y, vec.z));
        } else if (type_name == "m44f") {
            auto v = static_cast<const Imf::M44fAttribute *>(attr);
            Matrix4f M;
            for (size_t i = 0; i < 4; ++i)
                for (size_t j = 0; j < 4; ++j)
                    M(i, j) = v->value().x[i][j];
            m_metadata.set_transform(name, Transform4f(M));
        }
    }

    bool process_colors = false;
    m_srgb_gamma = false;
    m_premultiplied_alpha = true;
    m_pixel_format = PixelFormat::MultiChannel;
    m_struct = new Struct();
    Imf::PixelType pixel_type = channels.begin().channel().type;

    switch (pixel_type) {
        case Imf::HALF:  m_component_format = Struct::Type::Float16; break;
        case Imf::FLOAT: m_component_format = Struct::Type::Float32; break;
        case Imf::UINT:  m_component_format = Struct::Type::UInt32;  break;
        default: Throw("read_openexr(): Invalid component type (must be "
                       "float16, float32, or uint32)");
    }

    enum { Unknown, R, G, B, X, Y, Z, A, RY, BY, NumClasses };

    // Classification scheme for color channels
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

    // Assign a sorting key to color channels
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

    // Order channel names based on RGB/XYZ[A] suffix
    for (auto const &name: channels_sorted) {
        uint32_t flags = +Struct::Flags::None;
        // Tag alpha channels to be able to perform operations depending on alpha
        auto c_class = channel_class(name);
        if (c_class == A)
            flags |= +Struct::Flags::Alpha;

        // Currently we don't support alpha transformations for multi-layer images
        // So it is okay to just set this for all non-alpha channels
        if (c_class != A)
            flags |= +Struct::Flags::PremultipliedAlpha;
        m_struct->append(name, m_component_format, flags);
    }

    // Attempt to detect a standard combination of color channels
    m_pixel_format = PixelFormat::MultiChannel;
    bool luminance_chroma_format = false;
    if (found[R] && found[G] && found[B]) {
        if (m_struct->field_count() == 3)
            m_pixel_format = PixelFormat::RGB;
        else if (found[A] && m_struct->field_count() == 4)
            m_pixel_format = PixelFormat::RGBA;
    } else if (found[X] && found[Y] && found[Z]) {
        if (m_struct->field_count() == 3)
            m_pixel_format = PixelFormat::XYZ;
        else if (found[A] && m_struct->field_count() == 4)
            m_pixel_format = PixelFormat::XYZA;
    } else if (found[Y] && found[RY] && found[BY]) {
        if (m_struct->field_count() == 3)
            m_pixel_format = PixelFormat::RGB;
        else if (found[A] && m_struct->field_count() == 4)
            m_pixel_format = PixelFormat::RGBA;
        luminance_chroma_format = true;
    } else if (found[Y]) {
        if (m_struct->field_count() == 1)
            m_pixel_format = PixelFormat::Y;
        else if (found[A] && m_struct->field_count() == 2)
            m_pixel_format = PixelFormat::YA;
    }

    // Check if there is a chromaticity header entry
    Imf::Chromaticities file_chroma;
    if (Imf::hasChromaticities(file.header()))
        file_chroma = Imf::chromaticities(file.header());

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
    m_size = Vector2u(data_window.max.x - data_window.min.x + 1,
                      data_window.max.y - data_window.min.y + 1);

    // Compute pixel / row strides
    size_t pixel_stride = bytes_per_pixel(),
           row_stride = pixel_stride * m_size.x(),
           pixel_count = this->pixel_count();

    // Finally, allocate memory for it
    m_data = std::unique_ptr<uint8_t[]>(new uint8_t[row_stride * m_size.y()]);
    m_owns_data = true;

    using ResampleBuffer = std::pair<std::string, ref<Bitmap>>;
    std::vector<ResampleBuffer> resample_buffers;

    uint8_t *ptr = m_data.get() -
        (data_window.min.x + data_window.min.y * m_size.x()) * pixel_stride;

    // Tell OpenEXR where the image data should be put
    Imf::FrameBuffer framebuffer;
    for (auto const &field: *m_struct) {
        const Imf::Channel &channel = channels[field.name];
        Vector2i sampling(channel.xSampling, channel.ySampling);
        Imf::Slice slice;

        if (sampling == Vector2i(1)) {
            // This is a full resolution channel. Load the ordinary way
            slice = Imf::Slice(pixel_type, (char *) (ptr + field.offset),
                               pixel_stride, row_stride);
        } else {
            // Uh oh, this is a sub-sampled channel. We will need to scale it
            Vector2u channel_size = m_size / sampling;

            ref<Bitmap> bitmap = new Bitmap(PixelFormat::Y, m_component_format, channel_size);

            uint8_t *ptr_nested = bitmap->uint8_data() -
                (data_window.min.x / sampling.x() +
                 data_window.min.y / sampling.y() * (size_t) channel_size.x()) * field.size;

            slice = Imf::Slice(pixel_type, (char *) ptr_nested, field.size,
                               field.size * channel_size.x(), sampling.x(),
                               sampling.y());

            resample_buffers.emplace_back(field.name, std::move(bitmap));
        }

        framebuffer.insert(field.name, slice);
    }

    auto fs = dynamic_cast<FileStream *>(stream);
    Log(Debug, "Loading OpenEXR file \"%s\" (%ix%i, %s, %s) ..",
        fs ? fs->path().string() : "<stream>", m_size.x(), m_size.y(),
        m_pixel_format, m_component_format);

    file.setFrameBuffer(framebuffer);
    file.readPixels(data_window.min.y, data_window.max.y);

    for (auto &buf: resample_buffers) {
        Log(Debug, "Upsampling layer \"%s\" from %ix%i to %ix%i pixels",
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
        Log(Debug, "Converting from Luminance-Chroma to RGB format ..");
        Imath::V3f yw = Imf::RgbaYca::computeYw(file_chroma);

        auto convert = [&](auto *data) {
            using T = std::decay_t<decltype(*data)>;

            for (size_t j = 0; j < pixel_count; ++j) {
                Float Y  = (Float) data[0],
                      RY = (Float) data[1],
                      BY = (Float) data[2];

                if (std::is_integral<T>::value) {
                    Float scale = Float(1) / Float(std::numeric_limits<T>::max());
                    Y *= scale; RY *= scale; BY *= scale;
                }

                Float R = (RY + 1.f) * Y,
                      B = (BY + 1.f) * Y,
                      G = ((Y - R * yw.x - B * yw.z) / yw.y);

                if (std::is_integral<T>::value) {
                    Float scale = Float(std::numeric_limits<T>::max());
                    R *= R * scale + .5f;
                    G *= G * scale + .5f;
                    B *= B * scale + .5f;
                }

                data[0] = T(R); data[1] = T(G); data[2] = T(B);
                data += channel_count();
            }
        };

        switch (m_component_format) {
            case Struct::Type::Float16: convert((ek::half *) m_data.get()); break;
            case Struct::Type::Float32: convert((float *)       m_data.get()); break;
            case Struct::Type::UInt32:  convert((uint32_t*)     m_data.get()); break;
            default: Throw("Internal error!");
        }

        for (int i = 0; i < 3; ++i) {
            std::string &name = m_struct->operator[](i).name;
            name = set_suffix(name, std::string(1, "RGB"[i]));
        }
    }

    if (Imf::hasChromaticities(file.header()) &&
        (m_pixel_format == PixelFormat::RGB || m_pixel_format == PixelFormat::RGBA)) {

        Imf::Chromaticities itu_rec_b_709;
        Imf::Chromaticities xyz(Imath::V2f(1.f, 0.f), Imath::V2f(0.f, 1.f),
                                Imath::V2f(0.f, 0.f), Imath::V2f(1.f / 3.f, 1.f / 3.f));

        if (chroma_eq(file_chroma, itu_rec_b_709)) {
            // Already in the right space -- do nothing.
        } else if (chroma_eq(file_chroma, xyz)) {
            // This is an XYZ image
            m_pixel_format =
                m_pixel_format == PixelFormat::RGB ? PixelFormat::XYZ : PixelFormat::XYZA;
            for (int i = 0; i < 3; ++i) {
                std::string &name = m_struct->operator[](i).name;
                name = set_suffix(name, std::string(1, "XYZ"[i]));
            }
        } else {
            // Non-standard chromaticities. Special processing is required..
            process_colors = true;
        }
    }

    if (process_colors) {
        // Convert ITU-R Rec. BT.709 linear RGB
        Imath::M44f M = Imf::RGBtoXYZ(file_chroma, 1) *
                        Imf::XYZtoRGB(Imf::Chromaticities(), 1);

        Log(Debug, "Converting to sRGB color space ..");

        auto convert = [&](auto *data) {
            using T = std::decay_t<decltype(*data)>;

            for (size_t j = 0; j < pixel_count; ++j) {
                Float R = (Float) data[0],
                      G = (Float) data[1],
                      B = (Float) data[2];

                if (std::is_integral<T>::value) {
                    Float scale = Float(1) / Float(std::numeric_limits<T>::max());
                    R *= scale; G *= scale; B *= scale;
                }

                Imath::V3f rgb = Imath::V3f(float(R), float(G), float(B)) * M;
                R = Float(rgb[0]); G = Float(rgb[1]); B = Float(rgb[2]);

                if (std::is_integral<T>::value) {
                    Float scale = Float(std::numeric_limits<T>::max());
                    R *= R * scale + 0.5f;
                    G *= G * scale + 0.5f;
                    B *= B * scale + 0.5f;
                }

                data[0] = T(R); data[1] = T(G); data[2] = T(B);
                data += channel_count();
            }
        };

        switch (m_component_format) {
            case Struct::Type::Float16: convert((ek::half *) m_data.get()); break;
            case Struct::Type::Float32: convert((float *)       m_data.get()); break;
            case Struct::Type::UInt32:  convert((uint32_t*)     m_data.get()); break;
            default: Throw("Internal error!");
        }
    }
}

void Bitmap::write_openexr(Stream *stream, int quality) const {
    ScopedPhase phase(ProfilerPhase::BitmapWrite);
    if (Imf::globalThreadCount() == 0)
        Imf::setGlobalThreadCount(std::min(8, util::core_count()));

    PixelFormat pixel_format = m_pixel_format;

    Properties metadata(m_metadata);
    if (!metadata.has_property("generatedBy"))
        metadata.set_string("generatedBy", "Mitsuba version " MTS_VERSION);

    std::vector<std::string> keys = metadata.property_names();

    Imf::Header header(
        (int) m_size.x(),  // width
        (int) m_size.y(),  // height,
        1.f,               // pixelAspectRatio
        Imath::V2f(0, 0),  // screenWindowCenter,
        1.f,               // screenWindowWidth
        Imf::INCREASING_Y, // lineOrder
        quality <= 0 ? Imf::PIZ_COMPRESSION : Imf::DWAB_COMPRESSION // compression
    );

    if (quality > 0)
        Imf::addDwaCompressionLevel(header, float(quality));

    for (auto it = keys.begin(); it != keys.end(); ++it) {
        using Type = Properties::Type;

        Type type = metadata.type(*it);
        if (*it == "pixelAspectRatio" || *it == "screenWindowWidth" ||
            *it == "screenWindowCenter")
            continue;

        switch (type) {
            case Type::String:
                header.insert(it->c_str(), Imf::StringAttribute(metadata.string(*it)));
                break;
            case Type::Long:
                header.insert(it->c_str(), Imf::IntAttribute(metadata.int_(*it)));
                break;
            case Type::Float:
                if constexpr (std::is_same_v<Float, double>)
                    header.insert(it->c_str(), Imf::DoubleAttribute((double)metadata.float_(*it)));
                else
                    header.insert(it->c_str(), Imf::FloatAttribute(metadata.float_(*it)));
                break;
            case Type::Array3f: {
                    Vector3f val = metadata.array3f(*it);
                    header.insert(it->c_str(), Imf::V3fAttribute(
                        Imath::V3f((float) val.x(), (float) val.y(), (float) val.z())));
                }
                break;
            case Type::Transform: {
                    Matrix4f val = metadata.transform(*it).matrix;
                    header.insert(it->c_str(), Imf::M44fAttribute(Imath::M44f(
                        (float) val(0, 0), (float) val(0, 1),
                        (float) val(0, 2), (float) val(0, 3),
                        (float) val(1, 0), (float) val(1, 1),
                        (float) val(1, 2), (float) val(1, 3),
                        (float) val(2, 0), (float) val(2, 1),
                        (float) val(2, 2), (float) val(2, 3),
                        (float) val(3, 0), (float) val(3, 1),
                        (float) val(3, 2), (float) val(3, 3))));
                }
                break;
            default:
                header.insert(it->c_str(), Imf::StringAttribute(metadata.as_string(*it)));
                break;
        }
    }

    if (pixel_format == PixelFormat::XYZ || pixel_format == PixelFormat::XYZA) {
        Imf::addChromaticities(header, Imf::Chromaticities(
            Imath::V2f(1.f, 0.f),
            Imath::V2f(0.f, 1.f),
            Imath::V2f(0.f, 0.f),
            Imath::V2f(1.f / 3.f, 1.f / 3.f)));
    }

    size_t pixel_stride = m_struct->size(),
           row_stride = pixel_stride * m_size.x();

    Imf::ChannelList &channels = header.channels();
    Imf::FrameBuffer framebuffer;
    const uint8_t *ptr = uint8_data();
    for (auto field : *m_struct) {
        Imf::PixelType comp_type;
        switch (field.type) {
            case Struct::Type::Float32: comp_type = Imf::FLOAT; break;
            case Struct::Type::Float16: comp_type = Imf::HALF; break;
            case Struct::Type::UInt32: comp_type = Imf::UINT; break;
            default: Throw("Unexpected field type!");
        }

        Imf::Slice slice(comp_type, (char *) (ptr + field.offset), pixel_stride, row_stride);
        channels.insert(field.name, Imf::Channel(comp_type));
        framebuffer.insert(field.name, slice);
    }

    EXROStream ostr(stream);
    Imf::OutputFile file(ostr, header);
    file.setFrameBuffer(framebuffer);
    file.writePixels((int) m_size.y());
}

// -----------------------------------------------------------------------------
//   JPEG bitmap I/O
// -----------------------------------------------------------------------------

extern "C" {
    static const size_t jpeg_buffer_size = 0x8000;

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
        p->buffer = new JOCTET[jpeg_buffer_size];
    }

    METHODDEF(boolean) jpeg_fill_input_buffer(j_decompress_ptr cinfo) {
        jbuf_in_t *p = (jbuf_in_t *) cinfo->src;
        size_t bytes_read = 0;

        try {
            p->stream->read(p->buffer, jpeg_buffer_size);
            bytes_read = jpeg_buffer_size;
        } catch (const EOFException &e) {
            bytes_read = e.gcount();
            if (bytes_read == 0) {
                // Insert a fake EOI marker
                p->buffer[0] = (JOCTET) 0xFF;
                p->buffer[1] = (JOCTET) JPEG_EOI;
                bytes_read = 2;
            }
        }

        cinfo->src->bytes_in_buffer = bytes_read;
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

        p->buffer = new JOCTET[jpeg_buffer_size];
        p->mgr.next_output_byte = p->buffer;
        p->mgr.free_in_buffer = jpeg_buffer_size;
    }

    METHODDEF(boolean) jpeg_empty_output_buffer(j_compress_ptr cinfo) {
        jbuf_out_t *p = (jbuf_out_t *) cinfo->dest;
        p->stream->write(p->buffer, jpeg_buffer_size);
        p->mgr.next_output_byte = p->buffer;
        p->mgr.free_in_buffer = jpeg_buffer_size;
        return TRUE;
    }

    METHODDEF(void) jpeg_term_destination(j_compress_ptr cinfo) {
        jbuf_out_t *p = (jbuf_out_t *) cinfo->dest;
        p->stream->write(p->buffer,
                         jpeg_buffer_size - p->mgr.free_in_buffer);
        delete[] p->buffer;
        p->mgr.free_in_buffer = 0;
    }

    METHODDEF(void) jpeg_error_exit(j_common_ptr cinfo) {
        char msg[JMSG_LENGTH_MAX];
        (*cinfo->err->format_message) (cinfo, msg);
        Throw("Critcal libjpeg error: %s", msg);
    }
};

void Bitmap::read_jpeg(Stream *stream) {
    ScopedPhase phase(ProfilerPhase::BitmapRead);
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    jbuf_in_t jbuf;

    memset(&jbuf, 0, sizeof(jbuf_in_t));

    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = jpeg_error_exit;
    jpeg_create_decompress(&cinfo);
    cinfo.src = (struct jpeg_source_mgr *) &jbuf;
    jbuf.mgr.init_source = jpeg_init_source;
    jbuf.mgr.fill_input_buffer = jpeg_fill_input_buffer;
    jbuf.mgr.skip_input_data = jpeg_skip_input_data;
    jbuf.mgr.term_source = jpeg_term_source;
    jbuf.mgr.resync_to_restart = jpeg_resync_to_restart;
    jbuf.stream = stream;

    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    m_size = Vector2u(cinfo.output_width, cinfo.output_height);
    m_component_format = Struct::Type::UInt8;
    m_srgb_gamma = true;
    m_premultiplied_alpha = false;

    switch (cinfo.output_components) {
        case 1: m_pixel_format = PixelFormat::Y; break;
        case 3: m_pixel_format = PixelFormat::RGB; break;
        default: Throw("read_jpeg(): Unsupported number of components!");
    }

    rebuild_struct();

    auto fs = dynamic_cast<FileStream *>(stream);
    Log(Debug, "Loading JPEG file \"%s\" (%ix%i, %s, %s) ..",
        fs ? fs->path().string() : "<stream>", m_size.x(), m_size.y(),
        m_pixel_format, m_component_format);

    size_t row_stride =
        (size_t) cinfo.output_width * (size_t) cinfo.output_components;

    m_data = std::unique_ptr<uint8_t[]>(new uint8_t[buffer_size()]);
    m_owns_data = true;

    std::unique_ptr<uint8_t*[]> scanlines(new uint8_t*[m_size.y()]);

    for (size_t i = 0; i < m_size.y(); ++i)
        scanlines[i] = uint8_data() + row_stride*i;

    // Process scanline by scanline
    int counter = 0;
    while (cinfo.output_scanline < cinfo.output_height)
        counter += jpeg_read_scanlines(&cinfo, scanlines.get() + counter,
            (JDIMENSION) (m_size.y() - cinfo.output_scanline));

    // Release the libjpeg data structures
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
}

void Bitmap::write_jpeg(Stream *stream, int quality) const {
    ScopedPhase phase(ProfilerPhase::BitmapWrite);
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    jbuf_out_t jbuf;

    int components = 0;
    if (m_pixel_format == PixelFormat::Y)
        components = 1;
    else if (m_pixel_format == PixelFormat::RGB || m_pixel_format == PixelFormat::XYZ)
        components = 3;
    else
        Throw("write_jpeg(): Unsupported pixel format!");

    if (m_component_format != Struct::Type::UInt8)
        Throw("write_jpeg(): Unsupported component format %s, expected %s",
              m_component_format, Struct::Type::UInt8);

    memset(&jbuf, 0, sizeof(jbuf_out_t));
    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = jpeg_error_exit;
    jpeg_create_compress(&cinfo);

    cinfo.dest = (struct jpeg_destination_mgr *) &jbuf;
    jbuf.mgr.init_destination = jpeg_init_destination;
    jbuf.mgr.empty_output_buffer = jpeg_empty_output_buffer;
    jbuf.mgr.term_destination = jpeg_term_destination;
    jbuf.stream = stream;

    cinfo.image_width = (JDIMENSION) m_size.x();
    cinfo.image_height = (JDIMENSION) m_size.y();
    cinfo.input_components = components;
    cinfo.in_color_space = components == 1 ? JCS_GRAYSCALE : JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    if (quality == 100) {
        // Disable chroma subsampling
        cinfo.comp_info[0].v_samp_factor = 1;
        cinfo.comp_info[0].h_samp_factor = 1;
    }

    jpeg_start_compress(&cinfo, TRUE);

    // Write scanline by scanline
    for (size_t i = 0; i < m_size.y(); ++i) {
        uint8_t *source =
            m_data.get() + i * m_size.x() * cinfo.input_components;
        jpeg_write_scanlines(&cinfo, &source, 1);
    }

    // Release the libjpeg data structures
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
}

// -----------------------------------------------------------------------------
//   PNG bitmap I/O
// -----------------------------------------------------------------------------

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

static void png_error_func(png_structp, png_const_charp msg) {
    Throw("Fatal libpng error: %s\n", msg);
}

static void png_warn_func(png_structp, png_const_charp msg) {
    if (strstr(msg, "iCCP: known incorrect sRGB profile") != nullptr)
        return;
    Log(Warn, "libpng warning: %s\n", msg);
}

void Bitmap::read_png(Stream *stream) {
    ScopedPhase phase(ProfilerPhase::BitmapRead);
    png_bytepp rows = nullptr;

    // Create buffers
    png_structp png_ptr = png_create_read_struct(
        PNG_LIBPNG_VER_STRING, nullptr, &png_error_func, &png_warn_func);
    if (png_ptr == nullptr)
        Throw("read_png(): Unable to create PNG data structure");

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == nullptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        Throw("read_png(): Unable to create PNG information structure");
    }

    // Error handling
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        delete[] rows;
        Throw("read_png(): Error reading the PNG file!");
    }

    // Set read helper function
    png_set_read_fn(png_ptr, stream, (png_rw_ptr) png_read_data);

    int bit_depth, color_type, interlace_type, compression_type, filter_type;
    png_read_info(png_ptr, info_ptr);
    png_uint_32 width = 0, height = 0;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                 &interlace_type, &compression_type, &filter_type);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr); // Expand 1-, 2- and 4-bit grayscale

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr); // Always expand indexed files

    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr); // Expand transparency to a proper alpha channel

    // Request various transformations from libpng as necessary
    #if defined(LITTLE_ENDIAN)
        if (bit_depth == 16)
            png_set_swap(png_ptr); // Swap the byte order on little endian machines
    #endif

    // Update the information based on the transformations
    png_read_update_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
        &color_type, &interlace_type, &compression_type, &filter_type);
    m_size = Vector2u(width, height);

    switch (color_type) {
        case PNG_COLOR_TYPE_GRAY: m_pixel_format = PixelFormat::Y; break;
        case PNG_COLOR_TYPE_GRAY_ALPHA: m_pixel_format = PixelFormat::YA; break;
        case PNG_COLOR_TYPE_RGB: m_pixel_format = PixelFormat::RGB; break;
        case PNG_COLOR_TYPE_RGB_ALPHA: m_pixel_format = PixelFormat::RGBA; break;
        default: Throw("read_png(): Unknown color type %i", color_type); break;
    }

    switch (bit_depth) {
        case 8: m_component_format = Struct::Type::UInt8; break;
        case 16: m_component_format = Struct::Type::UInt16; break;
        default: Throw("read_png(): Unsupported bit depth: %i", bit_depth);
    }

    m_srgb_gamma = true;
    m_premultiplied_alpha = false;

    rebuild_struct();

    // Load any string-valued metadata
    int text_idx = 0;
    png_textp text_ptr;
    png_get_text(png_ptr, info_ptr, &text_ptr, &text_idx);

    for (int i = 0; i < text_idx; ++i, text_ptr++)
        m_metadata.set_string(text_ptr->key, text_ptr->text);

    auto fs = dynamic_cast<FileStream *>(stream);
    Log(Debug, "Loading PNG file \"%s\" (%ix%i, %s, %s) ..",
        fs ? fs->path().string() : "<stream>", m_size.x(), m_size.y(),
        m_pixel_format, m_component_format);

    size_t size = buffer_size();
    m_data = std::unique_ptr<uint8_t[]>(new uint8_t[size]);
    m_owns_data = true;

    rows = new png_bytep[m_size.y()];
    size_t row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    Assert(row_bytes == size / m_size.y());

    for (size_t i = 0; i < m_size.y(); i++)
        rows[i] = uint8_data() + i * row_bytes;

    png_read_image(png_ptr, rows);
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

    delete[] rows;
}

void Bitmap::write_png(Stream *stream, int compression) const {
    ScopedPhase phase(ProfilerPhase::BitmapWrite);
    png_structp png_ptr;
    png_infop info_ptr;
    volatile png_bytepp rows = nullptr;

    int color_type, bit_depth;
    switch (m_pixel_format) {
        case PixelFormat::Y: color_type = PNG_COLOR_TYPE_GRAY; break;
        case PixelFormat::YA: color_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
        case PixelFormat::RGB: color_type = PNG_COLOR_TYPE_RGB; break;
        case PixelFormat::RGBA: color_type = PNG_COLOR_TYPE_RGBA; break;
        default:
            Throw("write_png(): Unsupported pixel format!");
            return;
    }

    switch (m_component_format) {
        case Struct::Type::UInt8: bit_depth = 8; break;
        case Struct::Type::UInt16: bit_depth = 16; break;
        default:
            Throw("write_png(): Unsupported component type!");
            return;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                      &png_error_func, &png_warn_func);
    if (png_ptr == nullptr)
        Throw("Error while creating PNG data structure");

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == nullptr) {
        png_destroy_write_struct(&png_ptr, nullptr);
        Throw("Error while creating PNG information structure");
    }

    // Error handling
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        Throw("Error writing the PNG file");
    }

    png_set_write_fn(png_ptr, stream, (png_rw_ptr) png_write_data,
                     (png_flush_ptr) png_flush_data);
    png_set_compression_level(png_ptr, compression);

    png_text *text = nullptr;

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

    if (m_srgb_gamma)
        png_set_sRGB_gAMA_and_cHRM(png_ptr, info_ptr, PNG_sRGB_INTENT_ABSOLUTE);

    png_set_IHDR(png_ptr, info_ptr, (uint32_t) m_size.x(),
                 (uint32_t) m_size.y(), bit_depth, color_type,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    #if defined(LITTLE_ENDIAN)
        if (m_component_format == Struct::Type::UInt16 || m_component_format == Struct::Type::Int16)
            png_set_swap(png_ptr); // Swap the byte order on little endian machines
    #endif

    rows = new png_bytep[m_size.y()];

    size_t row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    Assert(row_bytes == buffer_size() / m_size.y());
    for (size_t i = 0; i < m_size.y(); i++)
        rows[i] = &m_data[row_bytes * i];

    png_write_image(png_ptr, rows);
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    delete[] text;
    delete[] rows;
}

// -----------------------------------------------------------------------------
//   PNG bitmap I/O
// -----------------------------------------------------------------------------

void Bitmap::read_ppm(Stream *stream) {
    ScopedPhase phase(ProfilerPhase::BitmapRead);
    int field = 0, n_chars = 0;

    std::string fields[4];

    while (field < 4) {
        char c;
        stream->read(c);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (n_chars != 0) {
                n_chars = 0;
                ++field;
            }
        } else {
            fields[field] += c;
            ++n_chars;
        }
    }

    if (fields[0] != "P6")
        Throw("read_ppm(): invalid format!");

    unsigned long long int_values[3];
    for (int i = 0; i < 3; ++i) {
        char *end_ptr = NULL;
        int_values[i]  = strtoull(fields[i + 1].c_str(), &end_ptr, 10);
        if (*end_ptr != '\0')
            Throw("read_ppm(): unable to parse the file header!");
    }

    m_size = Vector2u(int_values[0], int_values[1]);
    m_pixel_format = PixelFormat::RGB;
    m_srgb_gamma = true;
    m_premultiplied_alpha = false;
    m_component_format = int_values[2] <= 0xFF ? Struct::Type::UInt8 : Struct::Type::UInt16;
    rebuild_struct();

    auto fs = dynamic_cast<FileStream *>(stream);
    Log(Debug, "Loading PPM file \"%s\" (%ix%i, %s, %s) ..",
        fs ? fs->path().string() : "<stream>", m_size.x(), m_size.y(),
        m_pixel_format, m_component_format);

    size_t size = buffer_size();
    m_data = std::unique_ptr<uint8_t[]>(new uint8_t[size]);
    m_owns_data = true;
    stream->read(uint8_data(), size);
}

void Bitmap::write_ppm(Stream *stream) const {
    ScopedPhase phase(ProfilerPhase::BitmapWrite);
    if (m_pixel_format != PixelFormat::RGB ||
        (m_component_format != Struct::Type::UInt8 && m_component_format != Struct::Type::UInt16))
        Throw("write_ppm(): Only 8 or 16-bit RGB images are supported");
    stream->write_line(
        tfm::format("P6\n%i\n%i\n%i", m_size.x(), m_size.y(),
                    m_component_format == Struct::Type::UInt8 ? 0xFF : 0xFFFF));
    stream->write(uint8_data(), buffer_size());
}

// -----------------------------------------------------------------------------
//   RGBE bitmap I/O
// -----------------------------------------------------------------------------

/* The following is based on code by Bruce Walter */
namespace {
    inline void rgbe_from_float(float *data, uint8_t rgbe[4]) {
        /* Find the largest contribution */
        float max = std::max(std::max(data[0], data[1]), data[2]);
        if (max < 1e-32f) {
            rgbe[0] = rgbe[1] = rgbe[2] = rgbe[3] = 0;
        } else {
            int e;
            /* Extract exponent and convert the fractional part into
               the [0..255] range. Afterwards, divide by max so that
               any color component multiplied by the result will be in [0,255] */
            max = std::frexp(max, &e) * 256.f / max;
            rgbe[0] = (uint8_t) (data[0] * max);
            rgbe[1] = (uint8_t) (data[1] * max);
            rgbe[2] = (uint8_t) (data[2] * max);
            rgbe[3] = (uint8_t) (e + 128); // Exponent value in bias format
        }
    }

    inline void rgbe_to_float(uint8_t rgbe[4], float *data) {
        if (rgbe[3]) { // nonzero pixel
            float f = std::ldexp(1.f, (int) rgbe[3] - (128 + 8));
            for (int i  = 0; i < 3; ++i)
                data[i] = rgbe[i] * f;
        } else {
            memset(data, 0, sizeof(float) * 3);
        }
    }

    /* The code below is only needed for the run-length encoded files.
       Run length encoding adds considerable complexity but does
       save some space.  For each scanline, each channel (r,g,b,e) is
       encoded separately for better compression. */
    inline void rgbe_write_rle(Stream *stream, uint8_t *data, size_t num_bytes) {
        size_t cur = 0;
        uint8_t buf[2];

        while (cur < num_bytes) {
            size_t beg_run = cur;
            // find next run of length at least 4 if one exists
            size_t run_count = 0, old_run_count = 0;
            while (run_count < 4 && beg_run < num_bytes) {
                beg_run += run_count;
                old_run_count = run_count;
                run_count = 1;
                while ((beg_run + run_count < num_bytes) && (run_count < 127) &&
                       (data[beg_run] == data[beg_run + run_count]))
                    run_count++;
            }
            // if data before next big run is a short run then write it as such
            if (old_run_count > 1 && old_run_count == beg_run - cur) {
                buf[0] = (uint8_t) (128 + old_run_count);  // write short run
                buf[1] = data[cur];
                stream->write(buf, 2);
                cur = beg_run;
            }
            // write out bytes until we reach the start of the next run
            while (cur < beg_run) {
                size_t nonrun_count = beg_run - cur;
                if (nonrun_count > 128)
                    nonrun_count = 128;
                buf[0] = (uint8_t) nonrun_count;
                stream->write(buf, 1);
                stream->write(&data[cur], nonrun_count);
                cur += nonrun_count;
            }
            // write out next run if one was found
            if (run_count >= 4) {
                buf[0] = (uint8_t) (128 + run_count);
                buf[1] = data[beg_run];
                stream->write(buf, 2);
                cur += run_count;
            }
        }
    }

    /// simple read routine.  will not correctly handle run length encoding
    inline void rgbe_read_pixels(Stream *stream, float *data, size_t num_pixels) {
        while (num_pixels-- > 0) {
            uint8_t rgbe[4];
            stream->read(rgbe, 4);
            rgbe_to_float(rgbe, data);
            data += 3;
        }
    }
}

void Bitmap::read_rgbe(Stream *stream) {
    ScopedPhase phase(ProfilerPhase::BitmapRead);
    std::string line = stream->read_line();

    if (line.length() < 2 || line[0] != '#' || line[1] != '?')
        Throw("read_rgbe(): Invalid header!");

    bool format_recognized = false;
    while (true) {
        line = stream->read_line();
        if (string::starts_with(line, "FORMAT=32-bit_rle_rgbe")) {
            format_recognized = true;
            m_pixel_format = PixelFormat::RGB;
        } else if (string::starts_with(line, "FORMAT=32-bit_rle_xyze")) {
            format_recognized = true;
            m_pixel_format = PixelFormat::XYZ;
        } else {
            auto tokens = string::tokenize(line);
            if (tokens.size() == 4 && tokens[0] == "-Y" && tokens[2] == "+X") {
                m_size.y() = (uint32_t) std::stoull(tokens[1]);
                m_size.x() = (uint32_t) std::stoull(tokens[3]);
                break;
            }
        }
    }

    if (!format_recognized)
        Throw("read_rgbe(): unrecognized format!");

    m_component_format = Struct::Type::Float32;
    m_srgb_gamma = false;
    m_premultiplied_alpha = false;

    rebuild_struct();
    m_data = std::unique_ptr<uint8_t[]>(new uint8_t[buffer_size()]);
    m_owns_data = true;

    auto fs = dynamic_cast<FileStream *>(stream);
    Log(Debug, "Loading RGBE file \"%s\" (%ix%i, %s, %s) ..",
        fs ? fs->path().string() : "<stream>", m_size.x(), m_size.y(),
        m_pixel_format, m_component_format);

    float *data = (float *) m_data.get();

    if (m_size.x() < 8 || m_size.x() > 0x7fff) {
        // Run length encoding is not allowed, so write uncompressed contents
        rgbe_read_pixels(stream, data, pixel_count());
        return;
    }

    std::unique_ptr<uint8_t[]> buffer(new uint8_t[4 * m_size.x()]);

    // Read in each successive scanline
    for (size_t y = 0; y < m_size.y(); ++y) {
        uint8_t rgbe[4];
        stream->read(rgbe, 4);

        if (rgbe[0] != 2 || rgbe[1] != 2 || rgbe[2] & 0x80) {
            // this file is not run length encoded
            rgbe_to_float(rgbe, data);
            rgbe_read_pixels(stream, data + 3, pixel_count() - 1);
            return;
        }

        if (size_t(((int) rgbe[2]) << 8 | rgbe[3]) != m_size.x())
            Throw("read_rgbe(): wrong scanline width!");

        uint8_t *ptr = buffer.get();

        // read each of the four channels for the scanline into the buffer
        for (size_t i = 0; i < 4; i++) {
            uint8_t *ptr_end = buffer.get() + (i + 1) * m_size.x();

            while (ptr < ptr_end) {
                uint8_t buf[2];
                stream->read(buf, 2);

                if (buf[0] > 128) {
                    // a run of the same value
                    int count = buf[0] - 128;
                    if (count == 0 || count > ptr_end - ptr)
                        Throw("read_rgbe(): bad scanline data!");

                    while (count-- > 0)
                        *ptr++ = buf[1];
                } else {
                    // a non-run
                    int count = buf[0];
                    if (count == 0 || count > ptr_end - ptr)
                        Throw("read_rgbe(): bad scanline data!");
                    *ptr++ = buf[1];
                    if (--count > 0)
                        stream->read(ptr, count);
                    ptr += count;
                }
            }
        }
        // now convert data from buffer into floats
        for (size_t i = 0; i < m_size.x(); i++) {
            rgbe[0] = buffer[i];
            rgbe[1] = buffer[m_size.x() + i];
            rgbe[2] = buffer[2 * m_size.x() + i];
            rgbe[3] = buffer[3 * m_size.x() + i];
            rgbe_to_float(rgbe, data);
            data += 3;
        }
    }
}

void Bitmap::write_rgbe(Stream *stream) const {
    ScopedPhase phase(ProfilerPhase::BitmapWrite);
    if (m_component_format != Struct::Type::Float32)
        Throw("write_rgbe(): component format must be Float32!");

    std::string format_name;
    if (m_pixel_format == PixelFormat::RGB || m_pixel_format == PixelFormat::RGBA)
        format_name = "32-bit_rle_rgbe";
    else if (m_pixel_format == PixelFormat::XYZ ||  m_pixel_format == PixelFormat::XYZA)
        format_name = "32-bit_rle_xyze";
    else
        Throw("write_rgbe(): pixel format must be PixelFormat::RGB[A] or PixelFormat::XYZ[A]!");

    stream->write_line("#?RGBE");

    std::vector<std::string> keys = m_metadata.property_names();
    for (auto key : keys) {
        stream->write_line(tfm::format("# Metadata [%s]:", key));
        std::istringstream iss(m_metadata.as_string(key));
        std::string buf;
        while (std::getline(iss, buf))
            stream->write_line(tfm::format("#   %s", buf));
    }

    stream->write_line(tfm::format("FORMAT=%s\n", format_name));
    stream->write_line(tfm::format("-Y %i +X %i", m_size.y(), m_size.x()));

    float *data = (float *) m_data.get();
    if (m_size.x() < 8 || m_size.x() > 0x7fff) {
        // Run length encoding is not allowed, so write uncompressed contents
        uint8_t rgbe[4];
        for (size_t i = 0, size = pixel_count(); i < size; ++i) {
            rgbe_from_float(data, rgbe);
            data += (m_pixel_format == PixelFormat::RGB) ? 3 : 4;
            stream->write(rgbe, 4);
        }
        return;
    }

    std::unique_ptr<uint8_t[]> buffer(new uint8_t[4 * m_size.x()]);
    for (size_t y = 0; y < m_size.y(); ++y) {
        uint8_t rgbe[4] = { 2, 2, (uint8_t) (m_size.x() >> 8),
                                  (uint8_t) (m_size.x() & 0xFF) };
        stream->write(rgbe, 4);

        for (size_t x = 0; x < m_size.x(); x++) {
            rgbe_from_float(data, rgbe);

            buffer[x]              = rgbe[0];
            buffer[m_size.x()+x]   = rgbe[1];
            buffer[2*m_size.x()+x] = rgbe[2];
            buffer[3*m_size.x()+x] = rgbe[3];

            data += m_pixel_format == PixelFormat::RGB ? 3 : 4;
        }

        /* Write out each of the four channels separately run length encoded.
           First red, then green, then blue, then exponent */
        for (size_t i = 0; i < 4; i++)
            rgbe_write_rle(stream, buffer.get() + i * m_size.x(), m_size.x());
    }
}

// -----------------------------------------------------------------------------
//   PFM bitmap I/O
// -----------------------------------------------------------------------------

void Bitmap::read_pfm(Stream *stream) {
    ScopedPhase phase(ProfilerPhase::BitmapRead);
    char header[3];
    stream->read(header, 3);
    if (header[0] != 'P' || !(header[1] == 'F' || header[1] == 'f'))
        Throw("read_pfm(): Invalid header!");

    bool color = header[1] == 'F';
    m_pixel_format = color ? PixelFormat::RGB : PixelFormat::Y;
    m_component_format = Struct::Type::Float32;
    m_srgb_gamma = false;
    m_premultiplied_alpha = false;

    Float scale_and_order;

    try {
        m_size.x() = (uint32_t) std::stoul(stream->read_token());
        m_size.y() = (uint32_t) std::stoul(stream->read_token());
        scale_and_order = Float(std::stod(stream->read_token()));
    } catch (const std::logic_error &) {
        Throw("Could not parse PFM header!");
    }

    rebuild_struct();

    size_t size_in_bytes = buffer_size();
    m_data = std::unique_ptr<uint8_t[]>(new uint8_t[size_in_bytes]);
    m_owns_data = true;

    auto fs = dynamic_cast<FileStream *>(stream);
    Log(Debug, "Loading PFM file \"%s\" (%ix%i, %s, %s) ..",
        fs ? fs->path().string() : "<stream>", m_size.x(), m_size.y(),
        m_pixel_format, m_component_format);

    size_t size = size_in_bytes / sizeof(float);
    float *data = (float *) uint8_data();

    auto byte_order = stream->byte_order();
    stream->set_byte_order(scale_and_order <= 0.f ? Stream::ELittleEndian : Stream::EBigEndian);

    try {
        stream->read_array(data, size);
    } catch (...) {
        stream->set_byte_order(byte_order);
        throw;
    }
    stream->set_byte_order(byte_order);

    float scale = ek::abs(scale_and_order);
    if (scale != 1) {
        for (size_t i = 0; i < size; ++i)
            data[i] *= scale;
    }
    vflip();
}

void Bitmap::write_pfm(Stream *stream) const {
    ScopedPhase phase(ProfilerPhase::BitmapWrite);
    if (m_component_format != Struct::Type::Float32)
        Throw("write_pfm(): component format must be Float32!");
    if (m_pixel_format != PixelFormat::RGB && m_pixel_format != PixelFormat::RGBA &&
        m_pixel_format != PixelFormat::Y)
        Throw("write_pfm(): pixel format must be RGB, RGBA, Y, or YA!");

    // Write the header
    std::ostringstream oss;
    stream->write_line(tfm::format(
        "P%c",
        (m_pixel_format == PixelFormat::RGB || m_pixel_format == PixelFormat::RGBA) ? 'F' : 'f'));
    stream->write_line(tfm::format("%u %u", m_size.x(), m_size.y()));

#if defined(LITTLE_ENDIAN)
    stream->write_line("-1");
#else
    stream->write_line("1");
#endif

    float *data = (float *) m_data.get();
    size_t ch = channel_count();

    if (m_pixel_format == PixelFormat::RGB || m_pixel_format == PixelFormat::Y) {
        size_t scanline = (size_t) m_size.x() * ch;

        for (size_t y = 0; y < m_size.y(); ++y)
            stream->write(data + scanline * (m_size.y() - 1 - y),
                          scanline * sizeof(float));
    } else {
        /* For convenience: also handle images with an alpha
           channel, but strip it out while saving the data */
        size_t scanline = (size_t) m_size.x() * ch;
        float *temp = (float *) alloca(scanline * sizeof(float));

        for (size_t y = 0; y < m_size.y(); ++y) {
            const float *source = data + scanline * (m_size.y() - 1 - y);
            float *dest         = temp;

            for (size_t x = 0; x < m_size.x(); ++x) {
                for (size_t j = 0; j < ch - 1; ++j)
                    *dest++ = *source++;
                source++;
            }

            stream->write(temp, sizeof(float) * m_size.x() * (ch - 1));
        }
    }
}

// -----------------------------------------------------------------------------
//   BMP bitmap I/O
// -----------------------------------------------------------------------------

void Bitmap::read_bmp(Stream *stream) {
    ScopedPhase phase(ProfilerPhase::BitmapRead);
    uint32_t bmp_offset, header_size;
    int32_t width, height;
    uint16_t nplanes, bpp;
    uint32_t compression_type;
    char magic[2];

    auto byte_order = stream->byte_order();
    stream->set_byte_order(Stream::ELittleEndian);
    try {
        stream->read(magic, 2);

        if (magic[0] != 'B' || magic[1] != 'M')
            Throw("read_bmp(): Invalid header identifier!");

        stream->skip(8);
        stream->read(bmp_offset);
        stream->read(header_size);
        stream->read(width);
        stream->read(height);
        stream->read(nplanes);
        stream->read(bpp);
        stream->read(compression_type);
        stream->skip(bmp_offset-34);

        if (header_size != 40 || nplanes != 1 || width <= 0)
            Throw("read_bmp(): Unsupported BMP format encountered!");

        if (compression_type != 0)
            Throw("read_bmp(): Compressed files are currently not supported!");

        m_size = Vector2u(width, ek::abs(height));
        m_component_format = Struct::Type::UInt8;
        m_srgb_gamma = true;
        m_premultiplied_alpha = true;

        switch (bpp) {
            case 8:  m_pixel_format = PixelFormat::Y; break;
            case 16: m_pixel_format = PixelFormat::YA; break;
            case 24: m_pixel_format = PixelFormat::RGB; break;
            case 32: m_pixel_format = PixelFormat::RGBA; break;
            default:
                Throw("read_bmp(): Invalid bit depth (%i)!", bpp);
        }

        rebuild_struct();

        size_t size = buffer_size();
        m_data = std::unique_ptr<uint8_t[]>(new uint8_t[size]);
        m_owns_data = true;

        auto fs = dynamic_cast<FileStream *>(stream);
        Log(Debug, "Loading BMP file \"%s\" (%ix%i, %s, %s) ..",
            fs ? fs->path().string() : "<stream>", m_size.x(), m_size.y(),
            m_pixel_format, m_component_format);
        size_t row_size = size / m_size.y();
        size_t padding = (size_t) (-(ek::ssize_t) row_size & 3);
        bool do_vflip = height > 0;
        uint8_t *ptr = uint8_data();

        for (size_t y = 0; y < m_size.y(); ++y) {
            size_t target_y = do_vflip ? (m_size.y() - y - 1) : y;
            stream->read(ptr + row_size * target_y, row_size);
            stream->skip(padding);
        }

        if (m_pixel_format == PixelFormat::RGB || m_pixel_format == PixelFormat::RGBA) {
            size_t channels = channel_count();
            for (size_t i = 0; i < size; i += channels)
                std::swap(ptr[i], ptr[i+2]);
        }
        stream->set_byte_order(byte_order);
    } catch (...) {
        stream->set_byte_order(byte_order);
        throw;
    }
}

void Bitmap::read_tga(Stream *stream) {
    ScopedPhase phase(ProfilerPhase::BitmapRead);
    auto byte_order = stream->byte_order();
    stream->set_byte_order(Stream::ELittleEndian);
    try {
        uint8_t header_size, image_type, color_type;
        stream->read(header_size);
        stream->read(image_type);
        stream->read(color_type);

        if (image_type != 0)
            Throw("read_tga(): indexed files are not supported!");
        if (color_type != 2 && color_type != 3 && color_type != 10 && color_type != 11)
            Throw("read_tga(): only grayscale & RGB[A] files are supported!");

        stream->skip(9);
        int16_t width, height;
        uint8_t bpp, descriptor;

        stream->read(width);
        stream->read(height);
        stream->read(bpp);
        stream->read(descriptor);
        stream->skip(header_size);

        m_size = Vector2u((uint32_t) width, (uint32_t) height);
        m_srgb_gamma = true;
        m_premultiplied_alpha = false;
        m_component_format = Struct::Type::UInt8;

        bool do_vflip = !(descriptor & (1 << 5));
        bool greyscale = color_type == 3 || color_type == 11;
        bool rle = color_type & 8;

        if ((bpp == 8 && !greyscale) || (bpp != 8 && greyscale))
            Throw("read_tga(): Invalid bit depth!");

        switch (bpp) {
            case 8:  m_pixel_format = PixelFormat::Y; break;
            case 24: m_pixel_format = PixelFormat::RGB; break;
            case 32: m_pixel_format = PixelFormat::RGBA; break;
            default:
                Throw("read_tga(): Invalid bit depth!");
        }

        rebuild_struct();

        auto fs = dynamic_cast<FileStream *>(stream);
        Log(Debug, "Loading TGA file \"%s\" (%ix%i, %s, %s) ..",
            fs ? fs->path().string() : "<stream>", m_size.x(), m_size.y(),
            m_pixel_format, m_component_format);

        size_t size = buffer_size(),
               row_size = size / m_size.y();

        m_data = std::unique_ptr<uint8_t[]>(new uint8_t[size]);
        m_owns_data = true;
        size_t channel_count = bpp / 8;

        if (!rle) {
            for (size_t y = 0; y < m_size.y(); ++y) {
                size_t target_y = do_vflip ? (m_size.y() - y - 1) : y;
                stream->read(uint8_data() + target_y * row_size, row_size);
            }
        } else {
            // Decode an RLE-encoded image
            uint8_t temp[4], *ptr = uint8_data(), *end = ptr + size;

            while (ptr != end) {
                uint8_t value;
                stream->read(value);

                if (value & 0x80) {
                    // Run length packet
                    size_t count = (value & 0x7F) + 1;
                    stream->read(temp, channel_count);
                    for (size_t i = 0; i < count; ++i)
                        for (size_t j = 0; j < channel_count; ++j)
                            *ptr++ = temp[j];
                } else {
                    // Raw packet
                    size_t count = channel_count * (value + 1);
                    stream->read(ptr, count);
                    ptr += count;
                }
            }
            if (do_vflip)
                vflip();
        }

        if (!greyscale) {
            uint8_t *ptr = uint8_data();
            // Convert BGR to RGB
            for (size_t i = 0; i < size; i += channel_count)
                std::swap(ptr[i], ptr[i + 2]);
        }

        stream->set_byte_order(byte_order);
    } catch (...) {
        stream->set_byte_order(byte_order);
        throw;
    }
}

std::ostream &operator<<(std::ostream &os, Bitmap::PixelFormat value) {
    switch (value) {
        case Bitmap::PixelFormat::Y:            os << "y"; break;
        case Bitmap::PixelFormat::YA:           os << "ya"; break;
        case Bitmap::PixelFormat::RGB:          os << "rgb"; break;
        case Bitmap::PixelFormat::RGBA:         os << "rgba"; break;
        case Bitmap::PixelFormat::XYZ:          os << "xyz"; break;
        case Bitmap::PixelFormat::XYZA:         os << "xyza"; break;
        case Bitmap::PixelFormat::XYZAW:        os << "xyzaw"; break;
        case Bitmap::PixelFormat::MultiChannel: os << "multichannel"; break;
        default: Throw("Unknown pixel format!");
    }
    return os;
}

std::ostream &operator<<(std::ostream &os, Bitmap::FileFormat value) {
    switch (value) {
        case Bitmap::FileFormat::PNG:     os << "PNG"; break;
        case Bitmap::FileFormat::OpenEXR: os << "OpenEXR"; break;
        case Bitmap::FileFormat::JPEG:    os << "JPEG"; break;
        case Bitmap::FileFormat::BMP:     os << "BMP"; break;
        case Bitmap::FileFormat::PFM:     os << "PFM"; break;
        case Bitmap::FileFormat::PPM:     os << "PPM"; break;
        case Bitmap::FileFormat::RGBE:    os << "RGBE"; break;
        case Bitmap::FileFormat::Auto:    os << "Auto"; break;
        default: Throw("Unknown file format!");
    }
    return os;
}

std::ostream &operator<<(std::ostream &os, Bitmap::AlphaTransform value) {
    switch (value) {
        case Bitmap::AlphaTransform::None:    os << "none";    break;
        case Bitmap::AlphaTransform::Premultiply:   os << "premultiply";   break;
        case Bitmap::AlphaTransform::Unpremultiply: os << "unpremultiply"; break;
        default: Throw("Unknown alpha transform!");
    }
    return os;
}

void Bitmap::static_initialization() {
    // No-op
}

void Bitmap::static_shutdown() {
    Imf::setGlobalThreadCount(0);
}

MTS_IMPLEMENT_CLASS(Bitmap, Object)

NAMESPACE_END(mitsuba)
