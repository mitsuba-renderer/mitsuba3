#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/imageblock.h>

#include <mutex>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _film-hdrfilm:

High dynamic range film (:monosp:`hdrfilm`)
-------------------------------------------

.. pluginparameters::

 * - width, height
   - |int|
   - Width and height of the camera sensor in pixels Default: 768, 576)
 * - file_format
   - |string|
   - Denotes the desired output file format. The options are :monosp:`openexr`
     (for ILM's OpenEXR format), :monosp:`rgbe` (for Greg Ward's RGBE format), or
     :monosp:`pfm` (for the Portable Float Map format). (Default: :monosp:`openexr`)
 * - pixel_format
   - |string|
   - Specifies the desired pixel format of output images. The options are :monosp:`luminance`,
     :monosp:`luminance_alpha`, :monosp:`rgb`, :monosp:`rgba`, :monosp:`xyz` and :monosp:`xyza`.
     (Default: :monosp:`rgb`)
 * - component_format
   - |string|
   - Specifies the desired floating  point component format of output images. The options are
     :monosp:`float16`, :monosp:`float32`, or :monosp:`uint32`. (Default: :monosp:`float16`)
 * - crop_offset_x, crop_offset_y, crop_width, crop_height
   - |int|
   - These parameters can optionally be provided to select a sub-rectangle
     of the output. In this case, only the requested regions
     will be rendered. (Default: Unused)
 * - high_quality_edges
   - |bool|
   - If set to |true|, regions slightly outside of the film plane will also be sampled. This may
     improve the image quality at the edges, especially when using very large reconstruction
     filters. In general, this is not needed though. (Default: |false|, i.e. disabled)
 * - (Nested plugin)
   - :paramtype:`rfilter`
   - Reconstruction filter that should be used by the film. (Default: :monosp:`gaussian`, a windowed
     Gaussian filter)

This is the default film plugin that is used when none is explicitly specified. It stores the
captured image as a high dynamic range OpenEXR file and tries to preserve the rendering as much as
possible by not performing any kind of post processing, such as gamma correction---the output file
will record linear radiance values.

When writing OpenEXR files, the film will either produce a luminance, luminance/alpha, RGB(A),
or XYZ(A) tristimulus bitmap having a :monosp:`float16`,
:monosp:`float32`, or :monosp:`uint32`-based internal representation based on the chosen parameters.
The default configuration is RGB with a :monosp:`float16` component format, which is appropriate for
most purposes.

For OpenEXR files, Mitsuba 2 also supports fully general multi-channel output; refer to
the :ref:`aov <integrator-aov>` or :ref:`stokes <integrator-stokes>` plugins for
details on how this works.

The plugin can also write RLE-compressed files in the Radiance RGBE format pioneered by Greg Ward
(set :monosp:`file_format=rgbe`), as well as the Portable Float Map format
(set :monosp:`file_format=pfm`). In the former case, the :monosp:`component_format` and
:monosp:`pixel_format` parameters are ignored, and the output is :monosp:`float8`-compressed RGB
data. PFM output is restricted to :monosp:`float32`-valued images using the :monosp:`rgb` or
:monosp:`luminance` pixel formats. Due to the superior accuracy and adoption of OpenEXR, the use of
these two alternative formats is discouraged however.

When RGB(A) output is selected, the measured spectral power distributions are
converted to linear RGB based on the CIE 1931 XYZ color matching curves and
the ITU-R Rec. BT.709-3 primaries with a D65 white point.

The following XML snippet describes a film that writes a full-HD RGBA OpenEXR file:

.. code-block:: xml

    <film type="hdrfilm">
        <string name="pixel_format" value="rgba"/>
        <integer name="width" value="1920"/>
        <integer name="height" value="1080"/>
    </film>

 */

template <typename Float, typename Spectrum>
class HDRFilm final : public Film<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Film, m_size, m_crop_size, m_crop_offset,
                    m_high_quality_edges, m_filter)
    MTS_IMPORT_TYPES(ImageBlock)

    HDRFilm(const Properties &props) : Base(props) {
        std::string file_format = string::to_lower(
            props.string("file_format", "openexr"));
        std::string pixel_format = string::to_lower(
            props.string("pixel_format", "rgb"));
        std::string component_format = string::to_lower(
            props.string("component_format", "float16"));

        if (file_format == "openexr" || file_format == "exr")
            m_file_format = Bitmap::FileFormat::OpenEXR;
        else if (file_format == "rgbe")
            m_file_format = Bitmap::FileFormat::RGBE;
        else if (file_format == "pfm")
            m_file_format = Bitmap::FileFormat::PFM;
        else {
            Throw("The \"file_format\" parameter must either be "
                  "equal to \"openexr\", \"pfm\", or \"rgbe\","
                  " found %s instead.", file_format);
        }

        if (pixel_format == "luminance" || is_monochromatic_v<Spectrum>) {
            m_pixel_format = Bitmap::PixelFormat::Y;
            if (pixel_format != "luminance")
                Log(Warn,
                    "Monochrome mode enabled, setting film output pixel format "
                    "to 'luminance' (was %s).",
                    pixel_format);
        } else if (pixel_format == "luminance_alpha")
            m_pixel_format = Bitmap::PixelFormat::YA;
        else if (pixel_format == "rgb")
            m_pixel_format = Bitmap::PixelFormat::RGB;
        else if (pixel_format == "rgba")
            m_pixel_format = Bitmap::PixelFormat::RGBA;
        else if (pixel_format == "xyz")
            m_pixel_format = Bitmap::PixelFormat::XYZ;
        else if (pixel_format == "xyza")
            m_pixel_format = Bitmap::PixelFormat::XYZA;
        else {
            Throw("The \"pixel_format\" parameter must either be equal to "
                  "\"luminance\", \"luminance_alpha\", \"rgb\", \"rgba\", "
                  " \"xyz\", \"xyza\". Found %s.",
                  pixel_format);
        }

        if (component_format == "float16")
            m_component_format = Struct::Type::Float16;
        else if (component_format == "float32")
            m_component_format = Struct::Type::Float32;
        else if (component_format == "uint32")
            m_component_format = Struct::Type::UInt32;
        else {
            Throw("The \"component_format\" parameter must either be "
                  "equal to \"float16\", \"float32\", or \"uint32\"."
                  " Found %s instead.", component_format);
        }

        if (m_file_format == Bitmap::FileFormat::RGBE) {
            if (m_pixel_format != Bitmap::PixelFormat::RGB) {
                Log(Warn, "The RGBE format only supports pixel_format=\"rgb\"."
                           " Overriding..");
                m_pixel_format = Bitmap::PixelFormat::RGB;
            }
            if (m_component_format != Struct::Type::Float32) {
                Log(Warn, "The RGBE format only supports "
                           "component_format=\"float32\". Overriding..");
                m_component_format = Struct::Type::Float32;
            }
        } else if (m_file_format == Bitmap::FileFormat::PFM) {
            // PFM output; override pixel & component format if necessary
            if (m_pixel_format != Bitmap::PixelFormat::RGB && m_pixel_format != Bitmap::PixelFormat::Y) {
                Log(Warn, "The PFM format only supports pixel_format=\"rgb\""
                           " or \"luminance\". Overriding (setting to \"rgb\")..");
                m_pixel_format = Bitmap::PixelFormat::RGB;
            }
            if (m_component_format != Struct::Type::Float32) {
                Log(Warn, "The PFM format only supports"
                           " component_format=\"float32\". Overriding..");
                m_component_format = Struct::Type::Float32;
            }
        }

        props.mark_queried("banner"); // no banner in Mitsuba 2
    }

    void prepare(const std::vector<std::string> &channels) override {
        if (channels.size() < 5)
            Throw("Film::prepare(): expects at least 5 channels (RGBAW)!");

        for (size_t i = 0; i < 5; ++i) {
            if (channels[i] != std::string(1, "RGBAW"[i])) {
                Throw("Film::prepare(): expects the first 5 channels to be RGBAW!");
            }
        }

        std::vector<std::string> sorted = channels;
        std::sort(sorted.begin(), sorted.end());
        auto it = std::unique(sorted.begin(), sorted.end());
        if (it != sorted.end())
            Throw("Film::prepare(): duplicate channel name \"%s\"", *it);

        /* locked */ {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_storage = new ImageBlock(m_crop_size, channels.size());
            m_storage->set_offset(m_crop_offset);
            m_storage->clear();
            m_channels = channels;
        }
    }

    void put(const ImageBlock *block) override {
        Assert(m_storage != nullptr);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_storage->put(block);
    }

    void overwrite_channel(const std::string &channel_name,
                           const Float &value) override {
        auto it = std::find(m_channels.begin(), m_channels.end(), channel_name);
        if (it == m_channels.end())
            Throw("Channel \"%s\" not found in channels: %s", channel_name,
                  m_channels);

        size_t idx = it - m_channels.begin();
        /* locked */ {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_storage->overwrite_channel(idx, value);
        }
    }

    TensorXf develop(bool raw = false) const override {
        if (!m_storage)
            Throw("No storage allocated, was prepare() called first?");

        if (raw) {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_storage->data();
        }

        if constexpr (ek::is_jit_array_v<Float>) {
            Float data;
            size_t ch, pixel_count;
            ScalarVector2i size;
            /* locked */ {
                std::lock_guard<std::mutex> lock(m_mutex);
                data        = m_storage->data();
                size        = m_storage->size();
                ch          = m_storage->channel_count();
                pixel_count = ek::hprod(m_storage->size());
            }

            bool to_xyz    = m_pixel_format == Bitmap::PixelFormat::XYZ ||
                             m_pixel_format == Bitmap::PixelFormat::XYZA;
            bool to_y      = m_pixel_format == Bitmap::PixelFormat::Y ||
                             m_pixel_format == Bitmap::PixelFormat::YA;
            bool has_alpha = m_pixel_format == Bitmap::PixelFormat::RGBA ||
                             m_pixel_format == Bitmap::PixelFormat::YA ||
                             m_pixel_format == Bitmap::PixelFormat::XYZA;
            bool has_aovs = ch > 5;

            uint32_t img_ch = to_y ? 1 : 3;
            uint32_t target_ch = ch - 5 + img_ch + (has_alpha ? 1 : 0);

            UInt32 i = ek::arange<UInt32>(pixel_count * target_ch);
            UInt32 i_channel = i % target_ch;
            UInt32 weight_idx = (i / target_ch) * ch + 4;
            UInt32 values_idx = (i / target_ch) * ch + i_channel;

            if (has_aovs) {
                // Apply index offset for AOV channels
                uint32_t aovs_channel = img_ch + (has_alpha ? 1 : 0);
                Mask aovs_mask = i_channel >= aovs_channel;
                values_idx[aovs_mask] += 5 - aovs_channel;
            }

            if (to_y) {
                // Apply index offset for AOV channels (skip the X channel)
                Mask Y_mask = ek::eq(i_channel, 0);
                values_idx[Y_mask] += 1;
            }

            if (has_alpha && to_y) {
                // Apply index offset for Alpha channel (skip the YZ channels)
                Mask alpha_mask = ek::eq(i_channel, img_ch);
                values_idx[alpha_mask] += 2;
            }

            // Gather the pixel values from the image data buffer
            Float weight = ek::gather<Float>(data, weight_idx);
            Float values = ek::gather<Float>(data, values_idx);

            if (to_xyz || to_y) {
                UInt32 in_idx  = ek::arange<UInt32>(pixel_count) * ch;
                Color3f rgb = Color3f(ek::gather<Float>(data, in_idx),
                                      ek::gather<Float>(data, in_idx + 1),
                                      ek::gather<Float>(data, in_idx + 2));

                UInt32 out_idx = ek::arange<UInt32>(pixel_count) * target_ch;
                if (to_y) {
                    ek::scatter(values, luminance(rgb), out_idx);
                } else {
                    Color3f xyz = srgb_to_xyz(rgb);
                    ek::scatter(values, xyz[0], out_idx);
                    ek::scatter(values, xyz[1], out_idx + 1);
                    ek::scatter(values, xyz[2], out_idx + 2);
                }
            }

            // Apply weight
            values = (values / weight) & (weight > 0.f);

            size_t shape[3] = { (size_t) size.y(), (size_t) size.x(), target_ch };
            return TensorXf(values, 3, shape);
        } else {
            ref<Bitmap> source = bitmap();

            // Convert to Float32 bitmap
            std::vector<std::string> channel_names;
            for (size_t i = 0; i < source->channel_count(); i++)
                channel_names.push_back(source->struct_()->operator[](i).name);
            ref<Bitmap> target = new Bitmap(
                source->pixel_format(),
                Struct::Type::Float32,
                source->size(),
                source->channel_count(),
                channel_names);
            source->convert(target);

            ScalarVector2i size = target->size();
            size_t width = target->channel_count() * ek::hprod(size);
            auto data = ek::load<DynamicBuffer<Float>>(target->data(), width);
            size_t shape[3] = { (size_t) target->height(),
                                (size_t) target->width(),
                                target->channel_count() };
            return TensorXf(data, 3, shape);
        }
    }

    ref<Bitmap> bitmap(bool raw = false) const override {
        if (!m_storage)
            Throw("No storage allocated, was prepare() called first?");

        std::lock_guard<std::mutex> lock(m_mutex);
        auto &&storage = ek::migrate(m_storage->data().array(), AllocType::Host);

        if constexpr (ek::is_jit_array_v<Float>)
            ek::sync_thread();

        ref<Bitmap> source = new Bitmap(
            m_channels.size() != 5 ? Bitmap::PixelFormat::MultiChannel
                                   : Bitmap::PixelFormat::RGBAW,
            struct_type_v<ScalarFloat>, m_storage->size(),
            m_storage->channel_count(), m_channels, (uint8_t *) storage.data());

        if (raw)
            return source;

        bool to_rgb    = m_pixel_format == Bitmap::PixelFormat::RGB ||
                         m_pixel_format == Bitmap::PixelFormat::RGBA;
        bool to_xyz    = m_pixel_format == Bitmap::PixelFormat::XYZ ||
                         m_pixel_format == Bitmap::PixelFormat::XYZA;
        bool to_y      = m_pixel_format == Bitmap::PixelFormat::Y ||
                         m_pixel_format == Bitmap::PixelFormat::YA;
        bool has_alpha = m_pixel_format == Bitmap::PixelFormat::RGBA ||
                         m_pixel_format == Bitmap::PixelFormat::YA ||
                         m_pixel_format == Bitmap::PixelFormat::XYZA;
        bool has_aovs  = m_channels.size() != 5;

        uint32_t img_ch = to_y ? 1 : 3;
        uint32_t aovs_channel = (has_aovs ? img_ch + (has_alpha ? 1 : 0) : 0);
        uint32_t target_ch = m_storage->channel_count() - 5 + aovs_channel;

        ref<Bitmap> target = new Bitmap(
            has_aovs ? Bitmap::PixelFormat::MultiChannel : m_pixel_format,
            m_component_format, m_storage->size(),
            has_aovs ? target_ch : 0);

        if (has_aovs) {
            source->struct_()->operator[](4).flags |= +Struct::Flags::Weight;

            for (size_t i = 0; i < target_ch; ++i) {
                Struct::Field &dest_field = target->struct_()->operator[](i);

                switch (i) {
                    case 0:
                        if (to_rgb) {
                            dest_field.name = "R";
                            break;
                        } else if (to_xyz) {
                            dest_field.name = "X";
                            dest_field.blend = {
                                { 0.412453f, "R" },
                                { 0.357580f, "G" },
                                { 0.180423f, "B" }
                            };
                            break;
                        } else if (to_y) {
                            dest_field.name = "Y";
                            dest_field.blend = {
                                { 0.212671f, "R" },
                                { 0.715160f, "G" },
                                { 0.072169f, "B" }
                            };
                            break;
                        }
                        [[fallthrough]];

                    case 1:
                        if (to_rgb) {
                            dest_field.name = "G";
                            break;
                        } else if (to_xyz) {
                            dest_field.name = "Y";
                            dest_field.blend = {
                                { 0.212671f, "R" },
                                { 0.715160f, "G" },
                                { 0.072169f, "B" }
                            };
                            break;
                        } else if (to_y && has_alpha) {
                            dest_field.name = "A";
                            break;
                        }
                        [[fallthrough]];

                    case 2:
                        if (to_rgb) {
                            dest_field.name = "B";
                            break;
                        } else if (to_xyz) {
                            dest_field.name = "Z";
                            dest_field.blend = {
                                { 0.019334f, "R" },
                                { 0.119193f, "G" },
                                { 0.950227f, "B" }
                            };
                            break;
                        }
                        [[fallthrough]];

                    case 3:
                        if ((to_rgb || to_xyz) && has_alpha) {
                            dest_field.name = "A";
                            break;
                        }
                        [[fallthrough]];

                    default:
                        dest_field.name = m_channels[5 + i - aovs_channel];
                        break;
                }
            }
        }

        source->convert(target);

        return target;
    }

    void write(const fs::path &path) const override {
        fs::path filename = path;
        std::string proper_extension;
        if (m_file_format == Bitmap::FileFormat::OpenEXR)
            proper_extension = ".exr";
        else if (m_file_format == Bitmap::FileFormat::RGBE)
            proper_extension = ".rgbe";
        else
            proper_extension = ".pfm";

        std::string extension = string::to_lower(filename.extension().string());
        if (extension != proper_extension)
            filename.replace_extension(proper_extension);

        #if !defined(_WIN32)
            Log(Info, "\U00002714  Developing \"%s\" ..", filename.string());
        #else
            Log(Info, "Developing \"%s\" ..", filename.string());
        #endif

        bitmap()->write(filename, m_file_format);
    }

    void schedule_storage() override {
        ek::schedule(m_storage->data());
    };

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "HDRFilm[" << std::endl
            << "  size = " << m_size        << "," << std::endl
            << "  crop_size = " << m_crop_size   << "," << std::endl
            << "  crop_offset = " << m_crop_offset << "," << std::endl
            << "  high_quality_edges = " << m_high_quality_edges << "," << std::endl
            << "  filter = " << m_filter << "," << std::endl
            << "  file_format = " << m_file_format << "," << std::endl
            << "  pixel_format = " << m_pixel_format << "," << std::endl
            << "  component_format = " << m_component_format << "," << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    Bitmap::FileFormat m_file_format;
    Bitmap::PixelFormat m_pixel_format;
    Struct::Type m_component_format;
    ref<ImageBlock> m_storage;
    mutable std::mutex m_mutex;
    std::vector<std::string> m_channels;
};

MTS_IMPLEMENT_CLASS_VARIANT(HDRFilm, Film)
MTS_EXPORT_PLUGIN(HDRFilm, "HDR Film")
NAMESPACE_END(mitsuba)
