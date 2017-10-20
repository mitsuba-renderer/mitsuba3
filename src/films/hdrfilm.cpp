#include <mitsuba/mitsuba.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/imageblock.h>

NAMESPACE_BEGIN(mitsuba)

/*!\plugin{hdrfilm}{High dynamic range film}
* \order{1}
* \parameters{
*     \parameter{width, height}{\Integer}{
*       Width and height of the camera sensor in pixels
*       \default{768, 576}
*     }
*     \parameter{file_format}{\String}{
*       Denotes the desired output file format. The options
*       are \code{openexr} (for ILM's OpenEXR format),
*       \code{rgbe} (for Greg Ward's RGBE format),
*       or \code{pfm} (for the Portable Float Map format)
*       \default{\code{openexr}}
*     }
*     \parameter{pixel_format}{\String}{Specifies the desired pixel format
*         of output images. The options are \code{luminance},
*         \code{luminance_alpha}, \code{rgb}, \code{rgba}, \code{xyz},
*         \code{xyza}, \code{spectrum}, and \code{spectrum_alpha}.
*         For the \code{spectrum*} options, the number of written channels depends on
*         the value assigned to \code{SPECTRUM\_SAMPLES} during compilation
*         (see \secref{compiling} for details)
*         \default{\code{rgb}}
*     }
*     \parameter{component_format}{\String}{Specifies the desired floating
*         point component format of output images. The options are
*         \code{float16}, \code{float32}, or \code{uint32}.
*         \default{\code{float16}}
*     }
*     \parameter{crop_offset_x, crop_offset_y, crop_width, crop_height}{\Integer}{
*       These parameters can optionally be provided to select a sub-rectangle
*       of the output. In this case, Mitsuba will only render the requested
*       regions. \default{Unused}
*     }
*     \parameter{attach_log}{\Boolean}{Mitsuba can optionally attach
*         the entire rendering log file as a metadata field so that this
*         information is permanently saved.
*         \default{\code{true}, i.e. attach it}
*     }
*     \parameter{banner}{\Boolean}{Include a small Mitsuba banner in the
*         output image? \default{\code{true}}
*     }
*     \parameter{high_quality_edges}{\Boolean}{
*        If set to \code{true}, regions slightly outside of the film
*        plane will also be sampled. This may improve the image
*        quality at the edges, especially when using very large
*        reconstruction filters. In general, this is not needed though.
*        \default{\code{false}, i.e. disabled}
*     }
*     \parameter{\Unnamed}{\RFilter}{Reconstruction filter that should
*     be used by the film. \default{\code{gaussian}, a windowed Gaussian filter}}
* }
*
* This is the default film plugin that is used when none is explicitly
* specified. It stores the captured image as a high dynamic range OpenEXR file
* and tries to preserve the rendering as much as possible by not performing any
* kind of post processing, such as gamma correction---the output file
* will record linear radiance values.
*
* When writing OpenEXR files, the film will either produce a luminance, luminance/alpha,
* RGB(A), XYZ(A) tristimulus, or spectrum/spectrum-alpha-based bitmap having a
* \code{float16}, \code{float32}, or \code{uint32}-based internal representation
* based on the chosen parameters.
* The default configuration is RGB with a \code{float16} component format,
* which is appropriate for most purposes. Note that the spectral output options
* only make sense when using a custom build of Mitsuba that has spectral
* rendering enabled (this is not the case for the downloadable release builds).
* For OpenEXR files, Mitsuba also supports fully general multi-channel output;
* refer to the \pluginref{multichannel} plugin for details on how this works.
*
* The plugin can also write RLE-compressed files in the Radiance RGBE format
* pioneered by Greg Ward (set \code{file_format=rgbe}), as well as the
* Portable Float Map format (set \code{file_format=pfm}).
* In the former case,
* the \code{component_format} and \code{pixel_format} parameters are ignored,
* and the output is ``\code{float8}''-compressed RGB data.
* PFM output is restricted to \code{float32}-valued images using the
* \code{rgb} or \code{luminance} pixel formats.
* Due to the superior accuracy and adoption of OpenEXR, the use of these
* two alternative formats is discouraged however.
*
* When RGB(A) output is selected, the measured spectral power distributions are
* converted to linear RGB based on the CIE 1931 XYZ color matching curves and
* the ITU-R Rec. BT.709-3 primaries with a D65 white point.
*
* \begin{xml}[caption=Instantiation of a film that writes a full-HD RGBA OpenEXR file without the Mitsuba banner]
* <film type="hdrfilm">
*     <string name="pixel_format" value="rgba"/>
*     <integer name="width" value="1920"/>
*     <integer name="height" value="1080"/>
*     <boolean name="banner" value="false"/>
* </film>
* \end{xml}
*
* \subsubsection*{Render-time annotations:}
* \label{sec:film-annotations}
* The \pluginref{ldrfilm} and \pluginref{hdrfilm} plugins support a
* feature referred to as \emph{render-time annotations} to facilitate
* record keeping.
* Annotations are used to embed useful information inside a rendered image so
* that this information is later available to anyone viewing the image.
* Exemplary uses of this feature might be to store the frame or take number,
* rendering time, memory usage, camera parameters, or other relevant scene
* information.
*
* Currently, two different types are supported: a \code{metadata} annotation
* creates an entry in the metadata table of the image, which is preferable
* when the image contents should not be touched. Alternatively, a \code{label}
* annotation creates a line of text that is overlaid on top of the image. Note
* that this is only visible when opening the output file (i.e. the line is not
* shown in the interactive viewer).
* The syntax of this looks as follows:
*
* \begin{xml}
* <film type="hdrfilm">
*  <!-- Create a new metadata entry 'my_tag_name' and set it to the
*       value 'my_tag_value' -->
*  <string name="metadata['key_name']" value="Hello!"/>
*
*  <!-- Add the label 'Hello' at the image position X=50, Y=80 -->
*  <string name="label[50, 80]" value="Hello!"/>
* </film>
* \end{xml}
*
* The \code{value="..."} argument may also include certain keywords that will be
* evaluated and substituted when the rendered image is written to disk. A list all available
* keywords is provided in Table~\ref{tbl:film-keywords}.
*
* Apart from querying the render time,
* memory usage, and other scene-related information, it is also possible
* to `paste' an existing parameter that was provided to another plugin---for instance,
* the camera transform matrix would be obtained as \code{\$sensor['toWorld']}. The name of
* the active integrator plugin is given by \code{\$integrator['type']}, and so on.
* All of these can be mixed to build larger fragments, as following example demonstrates.
* The result of this annotation is shown in Figure~\ref{fig:annotation-example}.
* \begin{xml}[mathescape=false]
* <string name="label[10, 10]" value="Integrator: $integrator['type'],
*   $film['width']x$film['height'], $sampler['sampleCount'] spp,
*   render time: $scene['renderTime'], memory: $scene['memUsage']"/>
* \end{xml}
* \vspace{1cm}
* \renderings{
* \fbox{\includegraphics[width=.8\textwidth]{images/annotation_example}}\hfill\,
* \caption{\label{fig:annotation-example}A demonstration of the label annotation feature
*  given the example string shown above.}
* }
* \vspace{2cm}
* \begin{table}[htb]
* \centering
* \begin{savenotes}
* \begin{tabular}{ll}
* \toprule
* \code{\$scene['renderTime']}& Image render time, use \code{renderTimePrecise} for more digits.\\
* \code{\$scene['memUsage']}& Mitsuba memory usage\footnote{The definition of this quantity unfortunately
* varies a bit from platform to platform. On Linux and Windows, it denotes the total
* amount of allocated RAM and disk-based memory that is private to the process (i.e. not
* shared or shareable), which most intuitively captures the amount of memory required for
* rendering. On OSX, it denotes the working set size---roughly speaking, this is the
* amount of RAM apportioned to the process (i.e. excluding disk-based memory).}.
* Use \code{memUsagePrecise} for more digits.\\
* \code{\$scene['coreCount']}& Number of local and remote cores working on the rendering job\\
* \code{\$scene['blockSize']}& Block size used to parallelize up the rendering workload\\
* \code{\$scene['sourceFile']}& Source file name\\
* \code{\$scene['destFile']}& Destination file name\\
* \code{\$integrator['..']}& Copy a named integrator parameter\\
* \code{\$sensor['..']}& Copy a named sensor parameter\\
* \code{\$sampler['..']}& Copy a named sampler parameter\\
* \code{\$film['..']}& Copy a named film parameter\\
* \bottomrule
* \end{tabular}
* \end{savenotes}
* \caption{\label{tbl:film-keywords}A list of all special
* keywords supported by the annotation feature}
* \end{table}
*
*/
class HDRFilm : public Film {
public:
    HDRFilm(const Properties &props) : Film(props) {
        m_banner = props.bool_("banner", false);
        m_attach_log = props.bool_("attach_log", false);

        std::string file_format = string::to_lower(
            props.string("file_format", "openexr"));
        std::vector<std::string> pixel_formats = string::tokenize(string::to_lower(
            props.string("pixel_format", "rgb")), " ,");
        std::vector<std::string> channel_names = string::tokenize(
            props.string("channel_names", ""), ", ");
        std::string component_format = string::to_lower(
            props.string("component_format", "float16"));

        if (file_format == "openexr")
            m_file_format = Bitmap::EOpenEXR;
        else if (file_format == "rgbe")
            m_file_format = Bitmap::ERGBE;
        else if (file_format == "pfm")
            m_file_format = Bitmap::EPFM;
        else {
            Log(EError, "The \"file_format\" parameter must either be "
                        "equal to \"openexr\", \"pfm\", or \"rgbe\","
                        " found %s instead.", file_format);
        }

        if (pixel_formats.empty())
            Throw("At least one pixel format must be specified!");

        if ((pixel_formats.size() != 1
             && channel_names.size() != pixel_formats.size()) ||
            (pixel_formats.size() == 1 && channel_names.size() > 1)) {
            Throw("The number of channel names (%i) must match the number of"
                  " specified pixel formats (%i)!",
                  channel_names.size(), pixel_formats.size());
        }

        if (pixel_formats.size() != 1 && m_file_format != Bitmap::EOpenEXR) {
            Throw("General multi-channel output is only supported when writing"
                  " OpenEXR files! File format is %i.", m_file_format);
        }

        for (size_t i = 0; i < pixel_formats.size(); ++i) {
            std::string pixel_format = pixel_formats[i];
            std::string name = i < channel_names.size() ? (channel_names[i] + std::string(".")) : "";

            if (pixel_format == "luminance") {
                m_pixel_formats.push_back(Bitmap::EY);
                m_channel_names.push_back(name + "Y");
            }
            else if (pixel_format == "luminancealpha") {
                m_pixel_formats.push_back(Bitmap::EYA);
                m_channel_names.push_back(name + "Y");
                m_channel_names.push_back(name + "A");
            }
            else if (pixel_format == "rgb") {
                m_pixel_formats.push_back(Bitmap::ERGB);
                m_channel_names.push_back(name + "R");
                m_channel_names.push_back(name + "G");
                m_channel_names.push_back(name + "B");
            }
            else if (pixel_format == "rgba") {
                m_pixel_formats.push_back(Bitmap::ERGBA);
                m_channel_names.push_back(name + "R");
                m_channel_names.push_back(name + "G");
                m_channel_names.push_back(name + "B");
                m_channel_names.push_back(name + "A");
            }
            else if (pixel_format == "xyz") {
                m_pixel_formats.push_back(Bitmap::EXYZ);
                m_channel_names.push_back(name + "X");
                m_channel_names.push_back(name + "Y");
                m_channel_names.push_back(name + "Z");
            }
            else if (pixel_format == "xyza") {
                m_pixel_formats.push_back(Bitmap::EXYZA);
                m_channel_names.push_back(name + "X");
                m_channel_names.push_back(name + "Y");
                m_channel_names.push_back(name + "Z");
                m_channel_names.push_back(name + "A");
            }
            else {
                Throw("The \"pixel_format\" parameter must either be equal to "
                      "\"luminance\", \"luminance_alpha\", \"rgb\", \"rgba\", "
                      " \"xyz\", \"xyza\", \"spectrum\", or \"spectrum_alpha\"."
                      " Found %s.", pixel_format);
            }
        }

        if (component_format == "float16")
            m_component_format = Struct::EType::EFloat16;
        else if (component_format == "float32")
            m_component_format = Struct::EType::EFloat32;
        else if (component_format == "uint32")
            m_component_format = Struct::EType::EUInt32;
        else {
            Throw("The \"component_format\" parameter must either be "
                  "equal to \"float16\", \"float32\", or \"uint32\"."
                  " Found %s instead.", component_format);
        }

        if (m_file_format == Bitmap::ERGBE) {
            // RGBE output; override pixel & component format if necessary
            if (m_pixel_formats.size() != 1) {
                Throw("The RGBE format does not support general"
                      "multi-channel images!");
            }
            if (m_pixel_formats[0] != Bitmap::ERGB) {
                Log(EWarn, "The RGBE format only supports pixel_format=\"rgb\"."
                           " Overriding..");
                m_pixel_formats[0] = Bitmap::ERGB;
            }
            if (m_component_format != Struct::EType::EFloat32) {
                Log(EWarn, "The RGBE format only supports "
                           "component_format=\"float32\". Overriding..");
                m_component_format = Struct::EType::EFloat32;
            }
        }
        else if (m_file_format == Bitmap::EPFM) {
            // PFM output; override pixel & component format if necessary
            if (m_pixel_formats.size() != 1) {
                Throw("The PFM format does not support general"
                      " multi-channel images!");
            }
            if (m_pixel_formats[0] != Bitmap::ERGB
                && m_pixel_formats[0] != Bitmap::EY) {
                Log(EWarn, "The PFM format only supports pixel_format=\"rgb\""
                           " or \"luminance\". Overriding (setting to \"rgb\")..");
                m_pixel_formats[0] = Bitmap::ERGB;
            }
            if (m_component_format != Struct::EType::EFloat32) {
                Log(EWarn, "The PFM format only supports"
                           " component_format=\"float32\". Overriding..");
                m_component_format = Struct::EType::EFloat32;
            }
        }

        const std::vector<std::string> &keys = props.property_names();
        for (size_t i = 0; i < keys.size(); ++i) {
            std::string key = string::to_lower(keys[i]);
            key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());

            if ((string::starts_with(key, "metadata['")
                 && string::ends_with(key, "']")) ||
                (string::starts_with(key, "label[")
                 && string::ends_with(key, "]"))) {
                props.mark_queried(keys[i]);
            }
        }

        if (m_pixel_formats.size() == 1) {
            m_storage = new ImageBlock(Bitmap::ERGB, m_crop_size);
        } else {
            m_storage = new ImageBlock(
                Bitmap::EMultiChannel, m_crop_size, nullptr,
                (int)(MTS_WAVELENGTH_SAMPLES * m_pixel_formats.size() + 2));
        }
    }

    void clear() override {
        m_storage->clear();
    }

    void put(const ImageBlock *block) override {
        m_storage->put(block);
    }

    void set_bitmap(const Bitmap *bitmap) override {
        bitmap->convert(m_storage->bitmap());
    }

    void set_destination_file(const fs::path &dest_file,
                              uint32_t /*block_size*/) override {
        m_dest_file = dest_file;
    }

    void add_bitmap(const Bitmap *bitmap, Float multiplier) override {
        /* Currently, only accumulating spectrum-valued floating point images
        is supported. This function basically just exists to support the
        somewhat peculiar film updates done by BDPT */

        Vector2i size = bitmap->size();
        if (bitmap->component_format() != Struct::EType::EFloat ||
            bitmap->srgb_gamma() != 1.0f ||
            size != m_storage->size()    ||
            m_pixel_formats.size() != 1) {
            Throw("add_bitmap(): Unsupported bitmap format in new bitmap: %s",
                  bitmap->to_string());
        }

        size_t n_pixels = (size_t)size.x() * (size_t)size.y();
        const Float *source = static_cast<const Float *>(bitmap->data());
        Float *target = static_cast<Float *>(m_storage->bitmap()->data());
        for (size_t i = 0; i < n_pixels; ++i) {
            Float weight = target[MTS_WAVELENGTH_SAMPLES + 1];
            if (weight == 0)
                weight = target[MTS_WAVELENGTH_SAMPLES + 1] = 1;
            weight *= multiplier;
            for (size_t j = 0; j < MTS_WAVELENGTH_SAMPLES; ++j)
                (*target++) += (*source++ * weight);
            target += 2;
        }
    }

    bool develop(const Point2i  &source_offset,
                 const Vector2i &size,
                 const Point2i  &target_offset,
                 Bitmap *target) const override {
        const Bitmap *source = m_storage->bitmap();

        StructConverter cvt(source->struct_(), target->struct_());

        size_t source_bpp = source->bytes_per_pixel();
        size_t target_bpp = target->bytes_per_pixel();

        const uint8_t *source_data = source->uint8_data()
            + (source_offset.x() + source_offset.y() * source->width()) * source_bpp;
        uint8_t *target_data = target->uint8_data()
            + (target_offset.x() + target_offset.y() * target->width()) * target_bpp;

        if (unlikely(m_pixel_formats.size() != 1)) {
            NotImplementedError("Developping multi-channel HDR images.");
        } else if (size.x() == m_crop_size.x()
            && target->width() == m_storage->width()) {
            // Develop a connected part of the underlying buffer
            cvt.convert(size.x() * size.y(), source_data, target_data);
        } else {
            // Develop a rectangular subregion
            for (int i = 0; i < size.y(); ++i) {
                cvt.convert(size.x(), source_data, target_data);
                source_data += source->width() * source_bpp;
                target_data += target->width() * target_bpp;
            }
        }
        return true;
    }

    void develop(const Scene */*scene*/, Float /*renderTime*/) override {
        if (m_dest_file.empty())
            return;

        Log(EDebug, "Developing film ..");

        ref<Bitmap> bitmap;
        if (m_pixel_formats.size() == 1) {
            bitmap = m_storage->bitmap()->convert(m_pixel_formats[0],
                                                  m_component_format);
            // Set channels' names
            int idx = 0;
            for (auto c : *bitmap->struct_())
                c.name = m_channel_names[idx++];
        } else {
            NotImplementedError("Bitmap conversion for multi-channel HDR images.");
        }

        if (m_banner)
            NotImplementedError("Overlaying the Mitsuba watermark.")

        fs::path filename = m_dest_file;
        std::string proper_extension;
        if (m_file_format == Bitmap::EOpenEXR)
            proper_extension = ".exr";
        else if (m_file_format == Bitmap::ERGBE)
            proper_extension = ".rgbe";
        else
            proper_extension = ".pfm";

        std::string extension = string::to_lower(filename.extension().string());
        if (extension != proper_extension)
            filename.replace_extension(proper_extension);

        Log(EInfo, "Writing image to \"%s\" ..", filename.string());
        ref<FileStream> stream = new FileStream(filename, FileStream::EMode::ETruncReadWrite);

        // TODO: support annotations.
        if (m_attach_log) {
            NotImplementedError("Attaching the log file to the output image.");
        }

        bitmap->write(stream, m_file_format);
    }

    Bitmap *bitmap() override {
        return m_storage->bitmap();
    }

    bool has_alpha() const override {
        for (size_t i = 0; i < m_pixel_formats.size(); ++i) {
            if (m_pixel_formats[i] == Bitmap::EYA   ||
                m_pixel_formats[i] == Bitmap::ERGBA ||
                m_pixel_formats[i] == Bitmap::EXYZA)
                return true;
        }
        return false;
    }

    bool destination_exists(const fs::path &base_name) const override {
        std::string proper_extension;
        if (m_file_format == Bitmap::EOpenEXR)
            proper_extension = ".exr";
        else if (m_file_format == Bitmap::ERGBE)
            proper_extension = ".rgbe";
        else
            proper_extension = ".pfm";

        fs::path filename = base_name;

        std::string extension = string::to_lower(filename.extension().string());
        if (extension != proper_extension)
            filename.replace_extension(proper_extension);

        return fs::exists(filename);
    }

    virtual std::string to_string() const override {
        std::ostringstream oss;
        oss << "HDRFilm[" << std::endl
            << "  size = " << m_size        << "," << std::endl
            << "  crop_size = " << m_crop_size   << "," << std::endl
            << "  crop_offset = " << m_crop_offset << "," << std::endl
            << "  high_quality_edges = " << m_high_quality_edges << "," << std::endl
            << "  m_filter = " << m_filter << "," << std::endl
            << "  file_format = " << m_file_format << "," << std::endl
            << "  pixel_formats = " << m_pixel_formats << "," << std::endl
            << "  channel_names = " << m_channel_names << "," << std::endl
            << "  component_format = " << m_component_format << "," << std::endl
            << "  banner = " << m_banner << "," << std::endl
            << "  attach_log = " << m_attach_log << "," << std::endl
            << "  dest_file = " << m_dest_file << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()

protected:
    Bitmap::EFileFormat m_file_format;
    std::vector<Bitmap::EPixelFormat> m_pixel_formats;
    std::vector<std::string> m_channel_names;
    Struct::EType m_component_format;
    bool m_banner;
    bool m_attach_log;
    fs::path m_dest_file;
    ref<ImageBlock> m_storage;
};

MTS_IMPLEMENT_CLASS(HDRFilm, Film);
MTS_EXPORT_PLUGIN(HDRFilm, "HDR Film");
NAMESPACE_END(mitsuba)
