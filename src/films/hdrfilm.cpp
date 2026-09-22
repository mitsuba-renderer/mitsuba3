#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/imageblock.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _film-hdrfilm:

High dynamic range film (:monosp:`hdrfilm`)
-------------------------------------------

.. pluginparameters::
 :extra-rows: 8

 * - width, height
   - |int|
   - Width and height of the camera sensor in pixels. (Default: 768, 576)

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
   - Specifies the desired floating point component format of output images (when saving to disk).
     The options are :monosp:`float16`, :monosp:`float32`, or :monosp:`uint32`.
     (Default: :monosp:`float16`)

 * - crop_offset_x, crop_offset_y, crop_width, crop_height
   - |int|
   - These parameters can optionally be provided to select a sub-rectangle
     of the output. In this case, only the requested regions
     will be rendered. (Default: Unused)

 * - sample_border
   - |bool|
   - If set to |true|, regions slightly outside of the film plane will also be sampled. This may
     improve the image quality at the edges, especially when using very large reconstruction
     filters. In general, this is not needed though. (Default: |false|, i.e. disabled)

 * - (Nested plugin)
   - :paramtype:`rfilter`
   - Reconstruction filter that should be used by the film. (Default: :monosp:`gaussian`, a windowed
     Gaussian filter)

 * - size
   - ``Vector2u``
   - Width and height of the camera sensor in pixels
   - |exposed|

 * - crop_size
   - ``Vector2u``
   - Size of the sub-rectangle of the output in pixels
   - |exposed|

 * - crop_offset
   - ``Point2u``
   - Offset of the sub-rectangle of the output in pixels
   - |exposed|

This is the default film plugin that is used when none is explicitly specified. It stores the
captured image as a high dynamic range OpenEXR file and tries to preserve the rendering as much as
possible by not performing any kind of post processing, such as gamma correction---the output file
will record linear radiance values.

When writing OpenEXR files, the film will either produce a luminance, luminance/alpha, RGB(A),
or XYZ(A) tristimulus bitmap having a :monosp:`float16`,
:monosp:`float32`, or :monosp:`uint32`-based internal representation based on the chosen parameters.
The default configuration is RGB with a :monosp:`float16` component format, which is appropriate for
most purposes.

For OpenEXR files, Mitsuba 3 also supports fully general multi-channel output; refer to
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

.. tabs::
    .. code-tab::  xml

        <film type="hdrfilm">
            <string name="pixel_format" value="rgba"/>
            <integer name="width" value="1920"/>
            <integer name="height" value="1080"/>
        </film>

    .. code-tab:: python

        'type': 'hdrfilm',
        'pixel_format': 'rgba',
        'width': 1920,
        'height': 1080

 */

/// The pixel formats of the film and the base channels of each
struct HDRFilmFormat {
    const char *name;
    Bitmap::PixelFormat format;
    const char *channels;
    bool alpha;
};

static constexpr HDRFilmFormat hdrfilm_formats[] = {
    { "luminance",       Bitmap::PixelFormat::Y,    "Y",    false },
    { "luminance_alpha", Bitmap::PixelFormat::YA,   "YA",   true  },
    { "rgb",             Bitmap::PixelFormat::RGB,  "RGB",  false },
    { "rgba",            Bitmap::PixelFormat::RGBA, "RGBA", true  },
    { "xyz",             Bitmap::PixelFormat::XYZ,  "XYZ",  false },
    { "xyza",            Bitmap::PixelFormat::XYZA, "XYZA", true  }
};

template <typename Float, typename Spectrum>
class HDRFilm final : public Film<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Film, m_size, m_crop_size, m_crop_offset, m_sample_border,
                   m_filter, m_file_format,
                   m_component_format, m_base_channels, alloc_storage)
    MI_IMPORT_TYPES(ImageBlock)

    HDRFilm(const Properties &props) : Base(props) {
        std::string pixel_format = string::to_lower(
            props.get<std::string_view>("pixel_format", "rgb"));

        if (is_monochromatic_v<Spectrum> && pixel_format != "luminance" &&
            pixel_format != "luminance_alpha") {
            Log(Warn, "Monochrome mode enabled, setting film output pixel "
                      "format to 'luminance' (was %s).", pixel_format);
            pixel_format = "luminance";
        }

        m_format = nullptr;
        for (const HDRFilmFormat &format : hdrfilm_formats)
            if (pixel_format == format.name)
                m_format = &format;

        if (!m_format)
            Throw("The \"pixel_format\" parameter must either be equal to "
                  "\"luminance\", \"luminance_alpha\", \"rgb\", \"rgba\", "
                  " \"xyz\", \"xyza\". Found %s.", pixel_format);

        if (m_file_format == Bitmap::FileFormat::RGBE) {
            if (m_format->format != Bitmap::PixelFormat::RGB) {
                Log(Warn, "The RGBE format only supports pixel_format=\"rgb\"."
                           " Overriding..");
                m_format = &hdrfilm_formats[2];
            }
            if (m_component_format != sj::Type::Float32) {
                Log(Warn, "The RGBE format only supports "
                           "component_format=\"float32\". Overriding..");
                m_component_format = sj::Type::Float32;
            }
        } else if (m_file_format == Bitmap::FileFormat::PFM) {
            if (m_format->format != Bitmap::PixelFormat::RGB &&
                m_format->format != Bitmap::PixelFormat::Y) {
                Log(Warn, "The PFM format only supports pixel_format=\"rgb\""
                           " or \"luminance\". Overriding (setting to \"rgb\")..");
                m_format = &hdrfilm_formats[2];
            }
            if (m_component_format != sj::Type::Float32) {
                Log(Warn, "The PFM format only supports"
                           " component_format=\"float32\". Overriding..");
                m_component_format = sj::Type::Float32;
            }
        }

        for (const char *p = m_format->channels; *p; ++p)
            m_base_channels.push_back(std::string(1, *p));

        props.mark_queried("banner"); // no banner in Mitsuba 3

        alloc_storage();
    }

    void prepare_sample(const UnpolarizedSpectrum &spec,
                        const Wavelength &wavelengths, Float *out,
                        Mask valid, Mask active) const override {
        char color = m_format->channels[0];
        if (color == 'R') {
            Base::prepare_sample(spec, wavelengths, out, valid, active);
            return;
        }

        Color3f xyz;
        if constexpr (is_spectral_v<Spectrum>)
            xyz = spectrum_to_xyz(spec, wavelengths, active);
        else if constexpr (is_monochromatic_v<Spectrum>)
            xyz = srgb_to_xyz(Color3f(spec.x()), active);
        else
            xyz = srgb_to_xyz(spec, active);

        if (color == 'Y') {
            *out++ = xyz.y();
        } else {
            *out++ = xyz.x();
            *out++ = xyz.y();
            *out++ = xyz.z();
        }

        if (m_format->alpha)
            *out = dr::select(valid, Float(1.f), Float(0.f));
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "HDRFilm[" << std::endl
            << "  size = " << m_size << "," << std::endl
            << "  crop_size = " << m_crop_size << "," << std::endl
            << "  crop_offset = " << m_crop_offset << "," << std::endl
            << "  sample_border = " << m_sample_border << "," << std::endl
            << "  filter = " << m_filter << "," << std::endl
            << "  file_format = " << m_file_format << "," << std::endl
            << "  pixel_format = " << m_format->format << "," << std::endl
            << "  component_format = " << m_component_format << "," << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(HDRFilm)
protected:
    const HDRFilmFormat *m_format;

    MI_TRAVERSE_CB(Base)
};

MI_EXPORT_PLUGIN(HDRFilm)
NAMESPACE_END(mitsuba)

