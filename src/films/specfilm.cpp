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
-------------------------------------------

.. pluginparameters::

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
 * - high_quality_edges
   - |bool|
   - If set to |true|, regions slightly outside of the film plane will also be sampled. This may
     improve the image quality at the edges, especially when using very large reconstruction
     filters. In general, this is not needed though. (Default: |false|, i.e. disabled)
 * - (Nested plugin)
   - :paramtype:`rfilter`
   - Reconstruction filter that should be used by the film. (Default: :monosp:`gaussian`, a windowed
     Gaussian filter)
 * - (Nested plugins)
   - |spectrum|
   - One or several Sensor Response Functions (SRF) used to compute different spectral bands

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

The following XML snippet describes a film that writes the previously shown 3-channel OpenEXR file with
each channel containing the sensor response to each defined spectral band (it is possible to load a
spectrum from a file, see the :ref:`Spectrum definition <color-spectra>` section).
Notice that in this example, each band contains the spectral sensitivity of one of the ``rgb`` channels.

.. code-block:: xml

    <film type="specfilm">
        <integer  name="width" value="1920"/>
        <integer  name="height" value="1080"/>
        <spectrum name="band1_red" filename="data_red.spd" />
        <spectrum name="band2_green" filename="data_green.spd" />
        <spectrum name="band3_blue" filename="data_blue.spd" />
    </film>

 */


template <typename Float, typename Spectrum>
class SpecFilm final : public Film<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Film, m_size, m_crop_size, m_crop_offset,
                    m_high_quality_edges, m_filter, m_flags, m_srf)
    MTS_IMPORT_TYPES(ImageBlock, Texture)
    using FloatStorage = DynamicBuffer<Float>;

    SpecFilm(const Properties &props) : Base(props) {

        if constexpr (!is_spectral_v<Spectrum>)
            Log(Error, "This film can only be used in Mitsuba variants that "
                       "perform a spectral simulation.");

        // Load all SRF and store both name and data
        for (auto &[name, obj] : props.objects(false)) {
            auto srf = dynamic_cast<Texture *>(obj.get());
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

        m_flags = FilmFlags::Spectral | FilmFlags::Special;

        compute_srf_sampling();
    }

    void compute_srf_sampling() {
        ScalarFloat resolution = ek::Infinity<ScalarFloat>;
        // Compute full range of wavelengths and resolution in the film
        for (auto srf : m_srfs) {
            m_range.x() = ek::min(m_range.x(), srf->wavelength_range().x());
            m_range.y() = ek::max(m_range.y(), srf->wavelength_range().y());
            resolution = ek::min(resolution, srf->spectral_resolution());
        }

        // Compute resolution of the discretized PDF used for sampling
        size_t n_points = (size_t) ek::ceil((m_range.y() - m_range.x()) / resolution);
        FloatStorage mis_data = ek::zero<FloatStorage>(n_points);
        Float mis_wavelengths = ek::linspace<Float>(m_range.x(), m_range.y(), n_points);

        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();
        si.wavelengths = mis_wavelengths;

        for (auto srf : m_srfs) {
            UnpolarizedSpectrum values = srf->eval(si);
            // Each wavelenght is duplicated with the size of the Spectrum
            // (default constructor while initialized with only a number)
            mis_data += values.x();
        }

        // Conversion needed because Properties::Float is always double
        using DoubleStorage = ek::float64_array_t<FloatStorage>;
        DoubleStorage mis_data_dbl = DoubleStorage(mis_data);

        auto && storage = ek::migrate(mis_data_dbl, AllocType::Host);
        if constexpr (ek::is_jit_array_v<Float>)
            ek::sync_thread();

        // Create new spectrum with the sampling information
        auto props = Properties("regular");
        props.set_pointer("values", storage.data());
        props.set_long("size", n_points);
        props.set_float("lambda_min", (double) m_range.x());
        props.set_float("lambda_max", (double) m_range.y());
        m_srf = PluginManager::instance()->create_object<Texture>(props);
    }

    size_t prepare(const std::vector<std::string>& channels) override {
        std::vector<std::string> sorted = channels;

        for (size_t i = 0; i < m_srfs.size(); ++i)
            sorted.insert(sorted.begin() + i, m_names[i]);
        sorted.insert(sorted.end(), "W");  // Add weight channel

        /* locked */ {
            m_channels = sorted;
            std::lock_guard<std::mutex> lock(m_mutex);
            m_storage = new ImageBlock(m_crop_size, m_channels.size());
            m_storage->set_offset(m_crop_offset);
            m_storage->clear();
        }

        std::sort(sorted.begin(), sorted.end());
        auto it = std::unique(sorted.begin(), sorted.end());
        if (it != sorted.end())
            Throw("Film::prepare(): duplicate channel name \"%s\"", *it);

        return m_channels.size();
    }

    ref<ImageBlock> create_block(const ScalarVector2u &size, bool normalize,
                                 bool border) override {
        return new ImageBlock(size == ScalarVector2u(0) ? m_crop_size : size,
                              m_channels.size(), m_filter.get(),
                              border || m_high_quality_edges /* border */,
                              normalize /* normalize */,
                              ek::is_llvm_array_v<Float> /* coalesce */,
                              false /* warn_negative */,
                              false /* warn_invalid */);
    }

    void prepare_sample(const UnpolarizedSpectrum &spec, const Wavelength &wavelengths,
                        Float* aovs, Mask /* active */) const override {
        aovs[m_channels.size() - 1] = 1.f;   // Set sample weight

        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();
        si.wavelengths = wavelengths;

        // The SRF is not necessarily normalized, cancel out multiplicative factors
        UnpolarizedSpectrum inv_spec = m_srf->eval(si);
        inv_spec = ek::select(ek::neq(inv_spec, 0.f), ek::rcp(inv_spec), 1.f);
        UnpolarizedSpectrum values = spec * inv_spec;

        for (size_t j = 0; j < m_srfs.size(); ++j) {
            UnpolarizedSpectrum weights = m_srfs[j]->eval(si);
            aovs[j] = ek::zero<Float>();

            for (size_t i = 0; i<Spectrum::Size; ++i)
                aovs[j] = ek::fmadd(weights[i], values[i], aovs[j]);

            aovs[j] *= 1.f / Spectrum::Size;
        }
    }

    void put_block(const ImageBlock *block) override {
        Assert(m_storage != nullptr);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_storage->put_block(block);
    }

    TensorXf develop(bool raw = false) const override {
        if (!m_storage)
            Throw("No storage allocated, was prepare() called first?");

        if (raw) {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_storage->tensor();
        }

        if constexpr (ek::is_jit_array_v<Float>) {
            Float data;
            uint32_t source_ch;
            size_t pixel_count;
            ScalarVector2i size;

            /* locked */ {
                std::lock_guard<std::mutex> lock(m_mutex);
                data         = m_storage->tensor().array();
                size         = m_storage->size();
                source_ch    = (uint32_t) m_storage->channel_count();
                pixel_count  = ek::hprod(m_storage->size());
            }

            // Number of channels of the target tensor
            uint32_t target_ch = (uint32_t) m_channels.size() - 1;

            // Index vectors referencing pixels & channels of the output image
            UInt32 idx         = ek::arange<UInt32>(pixel_count * target_ch),
                   pixel_idx   = idx / target_ch,
                   channel_idx = idx - pixel_idx * target_ch;

            /* Index vectors referencing source pixels/weights as follows:
                 values_idx = R1, G1, B1, R2, G2, B2 (for RGB response functions)
                 weight_idx = W1, W1, W1, W2, W2, W2 */
            UInt32 values_idx = ek::fmadd(pixel_idx, source_ch, channel_idx),
                   weight_idx = ek::fmadd(pixel_idx, source_ch, (uint32_t) (m_channels.size() - 1));

            // Gather the pixel values from the image data buffer
            Float weight = ek::gather<Float>(data, weight_idx),
                  values = ek::gather<Float>(data, values_idx);

            // Perform the weight division unless the weight is zero
            values /= ek::select(ek::eq(weight, 0.f), 1.f, weight);

            size_t shape[3] = { (size_t) size.y(), (size_t) size.x(),
                                target_ch };

            return TensorXf(values, 3, shape);
        } else {
            ref<Bitmap> source = bitmap();
            ScalarVector2i size = source->size();
            size_t width = source->channel_count() * ek::hprod(size);
            auto data = ek::load<DynamicBuffer<Float>>(source->data(), width);

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
        auto &&storage = ek::migrate(m_storage->tensor().array(), AllocType::Host);

        if constexpr (ek::is_jit_array_v<Float>)
            ek::sync_thread();

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
        ek::schedule(m_storage->tensor());
    };

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SpecFilm[" << std::endl
            << "  size = " << m_size        << "," << std::endl
            << "  crop_size = " << m_crop_size   << "," << std::endl
            << "  crop_offset = " << m_crop_offset << "," << std::endl
            << "  high_quality_edges = " << m_high_quality_edges << "," << std::endl
            << "  filter = " << m_filter << "," << std::endl
            << "  file_format = " << m_file_format << "," << std::endl
            << "  pixel_format = " << m_pixel_format << "," << std::endl
            << "  component_format = " << m_component_format << "," << std::endl
            << "  sensor response functions = (" << std::endl;
        for (size_t c=0; c<m_srfs.size(); ++c)
            oss << m_srfs[c] << std::endl;
        oss << " )]";
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
    std::vector<ref<Texture>> m_srfs;
    std::vector<std::string> m_names;
    ScalarVector2f m_range { ek::Infinity<ScalarFloat>, -ek::Infinity<ScalarFloat> };
};

MTS_IMPLEMENT_CLASS_VARIANT(SpecFilm, Film)
MTS_EXPORT_PLUGIN(SpecFilm, "Spectral Bands Film")
NAMESPACE_END(mitsuba)
