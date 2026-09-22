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

NAMESPACE_BEGIN(mitsuba)

/**!

.. _film-specfilm:

Spectral film (:monosp:`specfilm`)
----------------------------------

.. pluginparameters::
 :extra-rows: 6

 * - width, height
   - |int|
   - Width and height of the camera sensor in pixels. (Default: 768, 576)

 * - component_format
   - |string|
   - Specifies the desired floating point component format of output images. The options are
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
                   m_filter, m_srf, m_file_format,
                   m_component_format, m_base_channels, alloc_storage)
    MI_IMPORT_TYPES(ImageBlock, Texture)
    using FloatStorage = DynamicBuffer<Float>;

    SpecFilm(const Properties &props) : Base(props) {
        if constexpr (!is_spectral_v<Spectrum>)
            Log(Error, "This film can only be used in Mitsuba variants that "
                       "perform a spectral simulation.");

        // Load all SRF and store both name and data
        for (auto &prop : props) {
            if (prop.type() == Properties::Type::Spectrum) {
                m_srfs.push_back(props.get_texture<Texture>(prop.name()));
                m_base_channels.push_back(std::string(prop.name()));
            } else if (Texture *srf = prop.try_get<Texture>()) {
                m_srfs.push_back(srf);
                m_base_channels.push_back(std::string(prop.name()));
            }
        }

        if (m_srfs.size() == 0)
            Log(Error, "At least one SRF should be defined");

        compute_srf_sampling();

        alloc_storage();
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        for (size_t i=0; i<m_srfs.size(); ++i)
            cb->put(m_base_channels[i], m_srfs[i], ParamFlags::NonDifferentiable);
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

        auto && storage = dr::migrate(mis_data_dbl, JitBackend::None);
        if constexpr (dr::is_jit_v<Float>)
            dr::sync_thread();

        // Pass spectrum data using Properties::Spectrum
        std::vector<double> storage_vec(storage.size());
        for (size_t i = 0; i < storage.size(); ++i)
            storage_vec[i] = storage[i];

        Properties props("regular");
        props.set("value", Properties::Spectrum(std::move(storage_vec),
                                                (double) m_range.x(),
                                                (double) m_range.y()));

        m_srf = PluginManager::instance()->create_object<Texture>(props);
    }

    void prepare_sample(const UnpolarizedSpectrum &spec,
                        const Wavelength &wavelengths, Float *out,
                        Mask /* valid */, Mask /* active */) const override {
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.wavelengths = wavelengths;

        // The SRF is not necessarily normalized, cancel out multiplicative factors
        UnpolarizedSpectrum inv_spec = m_srf->eval(si);
        inv_spec = dr::select(inv_spec != 0.f, dr::rcp(inv_spec), 1.f);
        UnpolarizedSpectrum values = spec * inv_spec;

        for (size_t j = 0; j < m_srfs.size(); ++j) {
            UnpolarizedSpectrum weights = m_srfs[j]->eval(si);
            out[j] = dr::zeros<Float>();

            for (size_t i = 0; i<Spectrum::Size; ++i)
                out[j] = dr::fmadd(weights[i], values[i], out[j]);

            out[j] *= 1.f / Spectrum::Size;
        }
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SpecFilm[" << std::endl
            << "  size = " << m_size << "," << std::endl
            << "  crop_size = " << m_crop_size << "," << std::endl
            << "  crop_offset = " << m_crop_offset << "," << std::endl
            << "  sample_border = " << m_sample_border << "," << std::endl
            << "  filter = " << m_filter << "," << std::endl
            << "  file_format = " << m_file_format << "," << std::endl
            << "  component_format = " << m_component_format << "," << std::endl
            << "  film_srf = [" << std::endl << "    " << string::indent(m_srf, 4) << std::endl << "  ]," << std::endl
            << "  sensor response functions = (" << std::endl;
        for (size_t c=0; c<m_srfs.size(); ++c)
            oss << "    " << string::indent(m_srfs[c], 4) << std::endl;
        oss << "  )" << std::endl << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(SpecFilm)
protected:
    std::vector<ref<Texture>> m_srfs;
    ScalarVector2f m_range { dr::Infinity<ScalarFloat>, -dr::Infinity<ScalarFloat> };

    MI_TRAVERSE_CB(Base, m_srfs)
};

MI_EXPORT_PLUGIN(SpecFilm)
NAMESPACE_END(mitsuba)

