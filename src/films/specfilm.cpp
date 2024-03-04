#include <mitsuba/core/bitmap.h>
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/imageblock.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/core/distr_1d.h>

#include <mutex>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _film-specfilm:

Spectral film (:monosp:`specfilm`)
----------------------------------

.. pluginparameters::
 :extra-rows: 6

 * - width, height
   - |int|
   - Width and height of the camera sensor in pixels Default: 768, 576)

 * - component_format
   - |string|
   - Specifies the desired floating  point component format of output images. The options are
     :monosp:`float16`, :monosp:`float32`, or :monosp:`uint32`. (Default: :monosp:`float16`)

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

 * - compensate
   - |bool|
   - If set to |true|, sample accumulation will be performed using Kahan-style
     error-compensated accumulation. This can be useful to avoid roundoff error
     when accumulating very many samples to compute reference solutions using
     single precision variants of Mitsuba. This feature is currently only supported
     in JIT variants and can make sample accumulation quite a bit more expensive.
     (Default: |false|, i.e. disabled)

 * - (Nested plugin)
   - :paramtype:`rfilter`
   - Reconstruction filter that should be used by the film. (Default: :monosp:`gaussian`, a windowed
     Gaussian filter)

 * - (Nested plugins)
   - |spectrum|
   - One or several Sensor Response Functions (SRF) used to compute different spectral bands
   - |exposed|

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

This plugin stores one or several spectral bands as a multichannel spectral image in a high dynamic
range OpenEXR file and tries to preserve the rendering as much as possible by not performing any
kind of post-processing, such as gamma correction---the output file will record linear radiance values.

Given one or several spectral sensor response functions (SRFs), the film will store in each channel
the captured radiance weighted by one of the SRFs (which do not have to be limited to the range of
the visible spectrum). The name of the channels in the final image appears given their alphabetical
order (not the location in the definition).

To reduce noise, this plugin implements two strategies: first, it creates a combined continuous
distribution with all the different SRFs using inverse transform sampling. Then it distributes
samples across all the spectral ranges of wavelengths covered by the SRFs. These strategies greatly
reduce the spectral noise that would appear if each channel were calculated independently.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/films/cbox_complete.png
   :caption: ``RGB`` spectral rendering
.. subfigure:: ../../resources/data/docs/images/films/band1_red.png
   :caption: ``CH-1``: :monosp:`band1_red`
.. subfigure:: ../../resources/data/docs/images/films/band2_green.png
   :caption: ``CH-2``: :monosp:`band2_green`
.. subfigure:: ../../resources/data/docs/images/films/band3_blue.png
   :caption: ``CH-3``: :monosp:`band3_blue`
.. subfigend::
   :label: fig-specfilm

The following snippet describes a film that writes the previously shown 3-channel OpenEXR file with
each channel containing the sensor response to each defined spectral band (it is possible to load a
spectrum from a file, see the :ref:`Spectrum definition <color-spectra>` section).
Notice that in this example, each band contains the spectral sensitivity of one of the ``rgb`` channels.

.. tabs::
    .. code-tab::  xml

        <film type="specfilm">
            <integer  name="width" value="1920"/>
            <integer  name="height" value="1080"/>
            <spectrum name="band1_red" filename="data_red.spd" />
            <spectrum name="band2_green" filename="data_green.spd" />
            <spectrum name="band3_blue" filename="data_blue.spd" />
        </film>

    .. code-tab:: python

        'type': 'specfilm',
        'width': 1920,
        'height': 1080,
        'band1_red': {
            'type': 'spectrum',
            'filename': 'data_red.spd'
        },
        'band1_green': {
            'type': 'spectrum',
            'filename': 'data_green.spd'
        },
        'band1_blue': {
            'type': 'spectrum',
            'filename': 'data_blue.spd'
        }

 */


template <typename Float, typename Spectrum>
class SpecFilm final : public Film<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Film, m_size, m_crop_size, m_crop_offset, m_sample_border,
                   m_filter, m_flags, m_srf, set_crop_window)
    MI_IMPORT_TYPES(ImageBlock, Texture)
    using FloatStorage = DynamicBuffer<Float>;

    SpecFilm(const Properties &props) : Base(props) {
        if constexpr (!is_spectral_v<Spectrum>)
            Log(Error, "This film can only be used in Mitsuba variants that "
                       "perform a spectral simulation.");

        // Load all SRF and store both name and data
        for (auto &[name, obj] : props.objects(false)) {
            Texture *srf = dynamic_cast<Texture *>(obj.get());
            if (srf != nullptr) {
                m_srfs.push_back(srf);
                m_names.push_back(name);
                props.mark_queried(name);
            }
        }

        if (m_srfs.size() == 0)
            Log(Error, "At least one SRF should be defined");

        std::string component_format = string::to_lower(
            props.string("component_format", "float16"));

        // The resulting bitmap is always OpenEXR MultiChannel
        m_file_format = Bitmap::FileFormat::OpenEXR;
        m_pixel_format = Bitmap::PixelFormat::MultiChannel;

        if (component_format == "float16")
            m_component_format = Struct::Type::Float16;
        else if (component_format == "float32")
            m_component_format = Struct::Type::Float32;
        else if (component_format == "uint32")
            m_component_format = Struct::Type::UInt32;
        else
            Throw("The \"component_format\" parameter must either be "
                  "equal to \"float16\", \"float32\", or \"uint32\"."
                  " Found %s instead.", component_format);

        m_compensate = props.get<bool>("compensate", false);

        m_flags = FilmFlags::Spectral | FilmFlags::Special;

        compute_srf_sampling();
    }

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
        for (size_t i=0; i<m_srfs.size(); ++i)
            callback->put_object(m_names[i], m_srfs[i].get(), +ParamFlags::NonDifferentiable);
    }

    void compute_srf_sampling() {
        ScalarFloat resolution = dr::Infinity<ScalarFloat>;
        // Compute full range of wavelengths and resolution in the film
        for (auto srf : m_srfs) {
            m_range.x() = dr::minimum(m_range.x(), srf->wavelength_range().x());
            m_range.y() = dr::maximum(m_range.y(), srf->wavelength_range().y());
            resolution = dr::minimum(resolution, srf->spectral_resolution());
        }

        // Compute resolution of the discretized PDF used for sampling
        size_t n_points = (size_t) dr::ceil((m_range.y() - m_range.x()) / resolution + 1);
        FloatStorage mis_data = dr::zeros<FloatStorage>(n_points);
        FloatStorage mis_wavelengths = dr::linspace<FloatStorage>(m_range.x(), m_range.y(), n_points);

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        // Each wavelength is duplicated with the size of the Spectrum (default
        // constructor while initialized with only a number)
        if constexpr (dr::is_jit_v<Float>) {
            si.wavelengths = mis_wavelengths;
            for (auto srf : m_srfs) {
                UnpolarizedSpectrum values = srf->eval(si);
                mis_data += values.x();
            }
        } else {
            for (size_t i = 0; i < n_points; ++i) {
                si.wavelengths = mis_wavelengths[i];
                for (auto srf : m_srfs) {
                    UnpolarizedSpectrum values = srf->eval(si);
                    mis_data[i] += values.x();
                }
            }
        }

        // Conversion needed because Properties::Float is always double
        using DoubleStorage = dr::float64_array_t<FloatStorage>;
        DoubleStorage mis_data_dbl = DoubleStorage(mis_data);

        auto && storage = dr::migrate(mis_data_dbl, AllocType::Host);
        if constexpr (dr::is_jit_v<Float>)
            dr::sync_thread();

        // Create new spectrum with the sampling information
        auto props = Properties("regular");
        props.set_pointer("values", storage.data());
        props.set_long("size", n_points);
        props.set_float("wavelength_min", (double) m_range.x());
        props.set_float("wavelength_max", (double) m_range.y());
        m_srf = PluginManager::instance()->create_object<Texture>(props);
    }

    size_t base_channels_count() const override {
        return m_srfs.size();
    }

    size_t prepare(const std::vector<std::string>& channels) override {
        std::vector<std::string> sorted = channels;

        for (size_t i = 0; i < m_srfs.size(); ++i)
            sorted.insert(sorted.begin() + i, m_names[i]);
        sorted.insert(sorted.end(), "W");  // Add weight channel

        /* locked */ {
            m_channels = sorted;
            std::lock_guard<std::mutex> lock(m_mutex);
            m_storage = new ImageBlock(m_crop_size, m_crop_offset,
                                       (uint32_t) m_channels.size());
        }

        std::sort(sorted.begin(), sorted.end());
        auto it = std::unique(sorted.begin(), sorted.end());
        if (it != sorted.end())
            Throw("Film::prepare(): duplicate channel name \"%s\"", *it);

        return m_channels.size();
    }

    ref<ImageBlock> create_block(const ScalarVector2u &size, bool normalize,
                                 bool border) override {
        bool default_config = dr::all(size == ScalarVector2u(0));

        return new ImageBlock(default_config ? m_crop_size : size,
                              default_config ? m_crop_offset : ScalarPoint2u(0),
                              (uint32_t) m_channels.size(), m_filter.get(),
                              border /* border */,
                              normalize /* normalize */,
                              dr::is_jit_v<Float> /* coalesce */,
                              m_compensate /* compensate */,
                              false /* warn_negative */,
                              false /* warn_invalid */);
    }

    void prepare_sample(const UnpolarizedSpectrum &spec, const Wavelength &wavelengths,
                        Float* aovs, Float weight, Float /* alpha */, Mask /* active */) const override {
        aovs[m_channels.size() - 1] = weight;   // Set sample weight

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.wavelengths = wavelengths;

        // The SRF is not necessarily normalized, cancel out multiplicative factors
        UnpolarizedSpectrum inv_spec = m_srf->eval(si);
        inv_spec = dr::select(inv_spec != 0.f, dr::rcp(inv_spec), 1.f);
        UnpolarizedSpectrum values = spec * inv_spec;

        for (size_t j = 0; j < m_srfs.size(); ++j) {
            UnpolarizedSpectrum weights = m_srfs[j]->eval(si);
            aovs[j] = dr::zeros<Float>();

            for (size_t i = 0; i<Spectrum::Size; ++i)
                aovs[j] = dr::fmadd(weights[i], values[i], aovs[j]);

            aovs[j] *= 1.f / Spectrum::Size;
        }
    }

    void put_block(const ImageBlock *block) override {
        Assert(m_storage != nullptr);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_storage->put_block(block);
    }
    
    void clear() override {
        if (m_storage)
            m_storage->clear();
    }

    TensorXf develop(bool raw = false) const override {
        if (!m_storage)
            Throw("No storage allocated, was prepare() called first?");

        if (raw) {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_storage->tensor();
        }

        if constexpr (dr::is_jit_v<Float>) {
            Float data;
            uint32_t source_ch;
            size_t pixel_count;
            ScalarVector2i size;

            /* locked */ {
                std::lock_guard<std::mutex> lock(m_mutex);
                data         = m_storage->tensor().array();
                size         = m_storage->size();
                source_ch    = (uint32_t) m_storage->channel_count();
                pixel_count  = dr::prod(m_storage->size());
            }

            // Number of channels of the target tensor
            uint32_t target_ch = (uint32_t) m_channels.size() - 1;

            // Index vectors referencing pixels & channels of the output image
            UInt32 idx         = dr::arange<UInt32>(pixel_count * target_ch),
                   pixel_idx   = idx / target_ch,
                   channel_idx = dr::fmadd(pixel_idx, uint32_t(-(int) target_ch), idx);

            /* Index vectors referencing source pixels/weights as follows:
                 values_idx = R1, G1, B1, R2, G2, B2 (for RGB response functions)
                 weight_idx = W1, W1, W1, W2, W2, W2 */
            UInt32 values_idx = dr::fmadd(pixel_idx, source_ch, channel_idx),
                   weight_idx = dr::fmadd(pixel_idx, source_ch, (uint32_t) (m_channels.size() - 1));

            // Gather the pixel values from the image data buffer
            Float weight = dr::gather<Float>(data, weight_idx),
                  values = dr::gather<Float>(data, values_idx);

            // Perform the weight division unless the weight is zero
            values /= dr::select(weight == 0.f, 1.f, weight);

            size_t shape[3] = { (size_t) size.y(), (size_t) size.x(),
                                target_ch };

            return TensorXf(values, 3, shape);
        } else {
            ref<Bitmap> source = bitmap();
            ScalarVector2i size = source->size();
            size_t width = source->channel_count() * dr::prod(size);
            auto data = dr::load<DynamicBuffer<Float>>(source->data(), width);

            size_t shape[3] = { (size_t) source->height(),
                                (size_t) source->width(),
                                source->channel_count() };

            return TensorXf(data, 3, shape);
        }
    }


    ref<Bitmap> bitmap(bool raw = false) const override {
        if (!m_storage)
            Throw("No storage allocated, was prepare() called first?");

        std::lock_guard<std::mutex> lock(m_mutex);
        auto &&storage = dr::migrate(m_storage->tensor().array(), AllocType::Host);

        if constexpr (dr::is_jit_v<Float>)
            dr::sync_thread();

        ref<Bitmap> source = new Bitmap(
            Bitmap::PixelFormat::MultiChannel,
            struct_type_v<ScalarFloat>, m_storage->size(),
            m_storage->channel_count(), m_channels, (uint8_t *) storage.data());

        if (raw)
            return source;

        ref<Bitmap> target = new Bitmap(
            Bitmap::PixelFormat::MultiChannel,
            struct_type_v<ScalarFloat>, m_storage->size(),
            m_storage->channel_count() - 1);

        source->struct_()->operator[](m_channels.size() - 1).flags |= +Struct::Flags::Weight;
        for (size_t i = 0; i < m_storage->channel_count() - 1; ++i) {
            Struct::Field &dest_field = target->struct_()->operator[](i);
            dest_field.name = m_channels[i];
        }

        source->convert(target);

        return target;
    }

    void write(const fs::path &path) const override {
        fs::path filename = path;
        std::string proper_extension = ".exr";

        std::string extension = string::to_lower(filename.extension().string());
        if (extension != proper_extension)
            filename.replace_extension(proper_extension);

        #if !defined(_WIN32)
            Log(Info, "\U00002714  Developing \"%s\" ..", filename.string());
        #else
            Log(Info, "Developing \"%s\" ..", filename.string());
        #endif

        ref<Bitmap> source = bitmap();
        if (m_component_format != struct_type_v<ScalarFloat>) {
            // Mismatch between the current format and the one expected by the film
            // Conversion is necessary before saving to disk
            std::vector<std::string> channel_names;
            for (size_t i = 0; i < source->channel_count(); i++)
                channel_names.push_back(source->struct_()->operator[](i).name);
            ref<Bitmap> target = new Bitmap(
                source->pixel_format(),
                m_component_format,
                source->size(),
                source->channel_count(),
                channel_names);
            source->convert(target);

            target->write(filename, m_file_format);
        } else {
            source->write(filename, m_file_format);
        }
    }

    void schedule_storage() override {
        dr::schedule(m_storage->tensor());
    };

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SpecFilm[" << std::endl
            << "  size = " << m_size << "," << std::endl
            << "  crop_size = " << m_crop_size << "," << std::endl
            << "  crop_offset = " << m_crop_offset << "," << std::endl
            << "  sample_border = " << m_sample_border << "," << std::endl
            << "  compensate = " << m_compensate << "," << std::endl
            << "  filter = " << m_filter << "," << std::endl
            << "  file_format = " << m_file_format << "," << std::endl
            << "  pixel_format = " << m_pixel_format << "," << std::endl
            << "  component_format = " << m_component_format << "," << std::endl
            << "  film_srf = [" << std::endl << "    " << string::indent(m_srf, 4) << std::endl << "  ]," << std::endl
            << "  sensor response functions = (" << std::endl;
        for (size_t c=0; c<m_srfs.size(); ++c)
            oss << "    " << string::indent(m_srfs[c], 4) << std::endl;
        oss << "  )" << std::endl << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
protected:
    Bitmap::FileFormat m_file_format;
    Bitmap::PixelFormat m_pixel_format;
    Struct::Type m_component_format;
    bool m_compensate;
    ref<ImageBlock> m_storage;
    mutable std::mutex m_mutex;
    std::vector<std::string> m_channels;
    std::vector<ref<Texture>> m_srfs;
    std::vector<std::string> m_names;
    ScalarVector2f m_range { dr::Infinity<ScalarFloat>, -dr::Infinity<ScalarFloat> };
};

MI_IMPLEMENT_CLASS_VARIANT(SpecFilm, Film)
MI_EXPORT_PLUGIN(SpecFilm, "Spectral Bands Film")
NAMESPACE_END(mitsuba)
