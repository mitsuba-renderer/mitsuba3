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
     (Default: :monosp:`rgba`)
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
The default configuration is RGBA with a :monosp:`float16` component format, which is appropriate for
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

The following XML snippet discribes a film that writes a full-HD RGBA OpenEXR file:

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
            props.string("pixel_format", "rgba"));
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
        std::vector<std::string> channels_sorted = channels;
        channels_sorted.push_back("R");
        channels_sorted.push_back("G");
        channels_sorted.push_back("B");
        std::sort(channels_sorted.begin(), channels_sorted.end());
        for (size_t i = 1; i < channels_sorted.size(); ++i) {
            if (channels_sorted[i] == channels_sorted[i - 1])
                Throw("Film::prepare(): duplicate channel name \"%s\"", channels_sorted[i]);
        }

        m_storage = new ImageBlock(m_crop_size, channels.size());
        m_storage->set_offset(m_crop_offset);
        m_storage->clear();
        m_channels = channels;
    }

    void put(const ImageBlock *block) override {
        Assert(m_storage != nullptr);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_storage->put(block);
    }

    ImageBuffer develop(bool raw = false) override {
        if (raw)
            return m_storage->data();

        if constexpr (ek::is_jit_array_v<Float>) {
            Float data = m_storage->data().copy();
            size_t ch =  m_storage->channel_count();
            size_t pixel_count = ek::hprod(m_storage->size());

            UInt32 idx = ek::arange<UInt32>(pixel_count) * ch;
            Color3f rgb = xyz_to_srgb(Color3f(ek::gather<Float>(data, idx),
                                              ek::gather<Float>(data, idx + 1),
                                              ek::gather<Float>(data, idx + 2)));
            ek::scatter(data, rgb[0], idx);
            ek::scatter(data, rgb[1], idx + 1);
            ek::scatter(data, rgb[2], idx + 2);

            UInt32 i = ek::arange<UInt32>(pixel_count * (ch - 1));
            UInt32 weight_idx = i / (ch - 1) * ch + 4;
            UInt32 values_idx = (i / (ch - 1)) * ch + i % (ch - 1);

            // Offset indices for aovs channels (skip the W channel)
            if (ch > 5)
                values_idx[(i % (ch - 1)) > 3] += 1;

            Float weight = ek::gather<Float>(data, weight_idx);
            Float values = ek::gather<Float>(data, values_idx);

            ek::mask_t<Float> valid = weight > 0.f;

            return (values / weight) & valid;
        } else {
            Struct::Type component_format = m_component_format;
            m_component_format = Struct::Type::Float32;
            ref<Bitmap> b = bitmap(false);
            m_component_format = component_format;
            size_t size = b->channel_count() * b->pixel_count();
            return ek::load<ImageBuffer>(b->data(), size);
        }
    }

    ref<Bitmap> bitmap(bool raw = false) override {
        auto &&storage = ek::migrate(m_storage->data(), AllocType::Host);
        if constexpr (ek::is_jit_array_v<Float>)
            ek::sync_thread();

        ref<Bitmap> source = new Bitmap(
            m_channels.size() != 5 ? Bitmap::PixelFormat::MultiChannel
                                   : Bitmap::PixelFormat::XYZAW,
            struct_type_v<ScalarFloat>, m_storage->size(),
            m_storage->channel_count(), m_channels, (uint8_t *) storage.data());

        if (raw)
            return source;

        bool has_aovs = m_channels.size() != 5;

        ref<Bitmap> target = new Bitmap(
            has_aovs ? Bitmap::PixelFormat::MultiChannel : m_pixel_format,
            m_component_format, m_storage->size(),
            has_aovs ? (m_storage->channel_count() - 1) : 0);

        if (has_aovs) {
            for (size_t i = 0, j = 0; i < m_channels.size(); ++i, ++j) {
                Struct::Field &source_field = source->struct_()->operator[](i),
                              &dest_field   = target->struct_()->operator[](j);

                switch (i) {
                    case 0:
                        dest_field.name = "R";
                        dest_field.blend = {
                            {  3.240479f, "X" },
                            { -1.537150f, "Y" },
                            { -0.498535f, "Z" }
                        };
                        break;

                    case 1:
                        dest_field.name = "G";
                        dest_field.blend = {
                            { -0.969256, "X" },
                            {  1.875991, "Y" },
                            {  0.041556, "Z" }
                        };
                        break;

                    case 2:
                        dest_field.name = "B";
                        dest_field.blend = {
                            {  0.055648, "X" },
                            { -0.204043, "Y" },
                            {  1.057311, "Z" }
                        };
                        break;

                    case 4:
                        source_field.flags |= +Struct::Flags::Weight;
                        j--;
                        break;

                    default:
                        dest_field.name = m_channels[i];
                        break;
                }
            }
        }

        source->convert(target);

        return target;
    }

    void write(const fs::path &path) override {
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
    std::mutex m_mutex;
    std::vector<std::string> m_channels;
};

MTS_IMPLEMENT_CLASS_VARIANT(HDRFilm, Film)
MTS_EXPORT_PLUGIN(HDRFilm, "HDR Film")
NAMESPACE_END(mitsuba)
