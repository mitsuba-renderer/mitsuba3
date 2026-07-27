// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
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
.. _film-tape:

Tape (:monosp:`tape`)
---------------------

.. pluginparameters::

 * - frequencies
   - |string|
   - Comma- or space-separated list of frequency band values in Hz. Each entry
     becomes one frequency band of the output; the number of entries defines the
     first (frequency) axis of the ETC. This parameter is required.

 * - time_bins
   - |int|
   - Number of time bins, i.e. the temporal resolution of the energy-time curve.
     Together with the integrator's ``max_time`` this sets the duration covered
     by each bin. (Default: 1)

 * - count
   - |bool|
   - If enabled, the film stores an additional ``count`` channel that records
     how many samples were written into each bin. This is mainly useful for
     debugging and normalization. (Default: |false|)

 * - file_format
   - |string|
   - Output file format when writing to disk: ``openexr``, ``pfm``, or ``rgbe``.
     (Default: ``openexr``)

 * - component_format
   - |string|
   - Numeric precision of the stored values: ``float16``, ``float32``, or
     ``uint32``. (Default: ``float16``)

This film records the **energy-time curve (ETC)**, the fundamental output of an
acoustic simulation: the sound energy arriving at the receiver as a function of
time, resolved per frequency band. It is the acoustic counterpart to an image
film and must be paired with a :ref:`microphone <sensor-microphone>` sensor.

The output is a tensor whose first axis corresponds to the frequency bands
listed in ``frequencies`` and whose second axis corresponds to the ``time_bins``
time bins. Each bin accumulates the energy of all sampled paths whose total
propagation time falls within that bin, so a single receiver produces one ETC
per frequency band rather than a two-dimensional image.

Unlike a conventional image film, the ``tape`` does not accept ``width`` or
``height``; the frequency axis is determined by ``frequencies`` and the time
axis by ``time_bins``.

.. tabs::
    .. code-tab:: xml
        :name: tape-film

        <film type="tape">
            <string name="frequencies" value="125, 250, 500, 1000, 2000, 4000"/>
            <integer name="time_bins" value="1000"/>
        </film>

    .. code-tab:: python

        'type': 'tape',
        'frequencies': '125, 250, 500, 1000, 2000, 4000',
        'time_bins': 1000,
*/

template <typename Float, typename Spectrum>
class Tape final : public Film<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Film, m_size, m_crop_size, m_crop_offset, m_sample_border,
                   m_filter, m_flags, set_crop_window, m_frequencies_spectrum)
    MI_IMPORT_TYPES(ImageBlock)

    Tape(const Properties &props) : Base(props) {
        if (props.has_property("width") || props.has_property("height"))
            Throw("Tape Plugin does not support (width, height). Set time_bins and specify a list of frequencies instead.");

        // load frequencies that should be rendered (-> copied from irregular spectrum)
        // note: mitsuba uses wavelengths, we use the same variable to track frequencies.
        // This is semantically inconsistent but no problem if we use frequencies everywhere consistently.
        if (props.type("frequencies") == Properties::Type::String) {
            std::vector<std::string> frequencies_str =
                string::tokenize(props.get<std::string_view>("frequencies"), " ,");

            std::vector<ScalarFloat> frequencies;
            frequencies.reserve(frequencies_str.size());

            for (size_t i = 0; i < frequencies_str.size(); ++i) {
                try {
                    frequencies.push_back(string::stof<ScalarFloat>(frequencies_str[i]));
                } catch (...) {
                    Throw("Could not parse floating point value '%s'", frequencies_str[i]);
                }
            }

            m_frequencies = frequencies;
            if (m_frequencies.size() == 1) {
                Log(Info, "Tape will store 1 frequency: %s", m_frequencies);
            } else {
                Log(Info, "Tape will store %i frequencies: %s", m_frequencies.size(), m_frequencies);
            }

            // load into spectrum for parallel evaluation
            m_frequencies_spectrum = DiscreteDistribution<Wavelength>(
                frequencies.data(), frequencies.size()
            );
        } else {
            NotImplementedError("Tape Plugin expects the 'frequencies' to be a string containing a comma-separated list of frequencies).");
        }

        m_size = ScalarVector2u(
            m_frequencies.size(),
            props.get<uint32_t>("time_bins", 1)
        );
        set_crop_window(ScalarPoint2u(0, 0), m_size);

        m_count = props.get<bool>("count", false);

        std::string file_format = string::to_lower(
            props.get<std::string_view>("file_format", "openexr"));
        std::string pixel_format = string::to_lower(
            props.get<std::string_view>("pixel_format", "MultiChannel"));
        std::string component_format = string::to_lower(
            props.get<std::string_view>("component_format", "float16"));

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

        if (component_format == "float16")
            m_component_format = sj::Type::Float16;
        else if (component_format == "float32")
            m_component_format = sj::Type::Float32;
        else if (component_format == "uint32")
            m_component_format = sj::Type::UInt32;
        else
            Throw("The \"component_format\" parameter must either be "
                  "equal to \"float16\", \"float32\", or \"uint32\"."
                  " Found %s instead.", component_format);

        // set flags
        m_flags = +FilmFlags::Special; // the film provides a special prepare method

        props.mark_queried("banner"); // no banner in Mitsuba 3
    }

    size_t base_channels_count() const override {
        return m_channels.size();
    }

    size_t prepare(const std::vector<std::string> & /*aovs*/) override {
        m_channels = { "values" };

        if (m_count) {
            // extra channel that counts the write accesses
            m_channels.push_back("count");
        }
        Log(Info, "Tape film will store %i channel(s): %s", m_channels.size(), m_channels);

        /* locked */ {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_storage = new ImageBlock(m_crop_size,
                                       m_crop_offset,
                                       (uint32_t) m_channels.size());
        }
        return (size_t) m_channels.size();
    }

    ref<ImageBlock> create_block(const ScalarVector2u &size, bool normalize,
                                 bool border) override {
        bool warn = !dr::is_jit_v<Float> && !is_spectral_v<Spectrum> &&
                    m_channels.size() <= 5;

        bool default_config = dr::all(size == ScalarVector2u(0));

        return new ImageBlock(default_config ? m_crop_size : size,
                              default_config ? m_crop_offset : ScalarPoint2u(0),
                              m_channels.size(),
                              m_filter.get(),
                              border /* border */,
                              normalize /* normalize */,
                              false /* coalesce */,
                              warn /* warn_negative */,
                              warn /* warn_invalid */,
                              true /* y_only */);
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
        Log(Info, "developing tape");
        if (!m_storage)
            Throw("No storage allocated, was prepare() called first?");

        if (raw) {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_storage->tensor();
        }

        if constexpr (dr::is_jit_v<Float>) {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_storage->tensor();
        } else {
            ref<Bitmap> source = bitmap();
            ScalarVector2i size = source->size();
            size_t width = source->channel_count() * dr::prod(size);
            auto data = dr::load<DynamicBuffer<ScalarFloat>>(source->data(), width);

            size_t shape[3] = { (size_t) source->height(),
                                (size_t) source->width(),
                                source->channel_count() };

            return TensorXf(data, 3, shape);
        }
    }

    ref<Bitmap> bitmap(bool /* raw */ = false) const override {
        if (!m_storage)
            Throw("No storage allocated, was prepare() called first?");

        std::lock_guard<std::mutex> lock(m_mutex);
        auto &&storage = dr::migrate(m_storage->tensor().array(), JitBackend::None);

        if constexpr (dr::is_jit_v<Float>)
            dr::sync_thread();

        return new Bitmap(
            Bitmap::PixelFormat::MultiChannel, struct_type_v<ScalarFloat>, m_storage->size(),
            m_storage->channel_count(), m_channels, (uint8_t *) storage.data());
    }


    void prepare_sample(const UnpolarizedSpectrum &spec,
                        const Wavelength & /* frequencies */,
                        Float* aovs, // result array. Length need to match number of channels.
                        Float weight,
                        Float /* alpha */,
                        Mask /* active */) const override {

        size_t channel_count = m_channels.size();

        if (spec.size() > 1)
            Throw("Tape only supports single spectrum values. Use an acoustic variant instead.");

        aovs[0] = weight * spec[0];

        if (m_count) {
            aovs[channel_count-1] = 1;
        }
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

        ref<Bitmap> source = bitmap();
        if (m_component_format != struct_type_v<ScalarFloat>) {
            // Mismatch between the current format and the one expected by the film
            // Conversion is necessary before saving to disk
            std::vector<std::string> channel_names;
            for (size_t i = 0; i < source->channel_count(); i++)
                channel_names.push_back(source->struct_()[i].name);
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
        oss << "Tape[" << std::endl
            << "  size = " << m_size << "," << std::endl
            << "  frequencies = " << m_frequencies << "," << std::endl
            << "  time_bins = " << m_size[1] << "," << std::endl
            << "  channels = " << m_channels << "," << std::endl
            // << "  crop_size = " << m_crop_size << "," << std::endl
            // << "  crop_offset = " << m_crop_offset << "," << std::endl
            // << "  sample_border = " << m_sample_border << "," << std::endl
            << "  filter = " << m_filter << "," << std::endl
            << "  file_format = " << m_file_format << "," << std::endl
            << "  component_format = " << m_component_format << "," << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(Tape)
protected:
    Bitmap::FileFormat m_file_format;
    sj::Type m_component_format;
    ref<ImageBlock> m_storage;
    mutable std::mutex m_mutex;
    std::vector<std::string> m_channels;
    std::vector<ScalarFloat> m_frequencies;
    bool m_count;

    MI_TRAVERSE_CB(Base, m_storage)
};

MI_EXPORT_PLUGIN(Tape)
NAMESPACE_END(mitsuba)
