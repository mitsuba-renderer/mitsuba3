#include <mitsuba/render/sunsky.h>
#include <drjit/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-timed_sunsky:

Timed sun and sky emitter (:monosp:`timed_sunsky`)
--------------------------------------------------

.. pluginparameters::

 * - turbidity
   - |float|
   - Atmosphere turbidity, must be within [1, 10] (Default: 3, clear sky in a temperate climate).
     Smaller turbidity values (∼ 1 − 2) produce an arctic-like clear blue sky,
     whereas larger values (∼ 8 − 10) create an atmosphere that is more typical
     of a warm, humid day.
   - |exposed|

 * - albedo
   - |spectrum|
   - Ground albedo, must be within [0, 1] for each wavelength/channel, (Default: 0.3).
     This cannot be spatially varying (e.g. have bitmap as type).
   - |exposed|

 * - latitude
   - |float|
   - Latitude of the location in degrees (Default: 35.689, Tokyo's latitude).
   - |exposed|

 * - longitude
   - |float|
   - Longitude of the location in degrees (Default: 139.6917, Tokyo's longitude).
   - |exposed|

 * - timezone
   - |float|
   - Timezone of the location in hours (Default: 9).
   - |exposed|

 * - window_start_time
   - |float|
   - Start hour for the daily average (Default: 7).
   - |exposed|

 * - window_end_time
   - |float|
   - Final hour for the daily average (Default: 19).
   - |exposed|

 * - start_year
   - |int|
   - Year of the start of the average (Default: 2025).
   - |exposed|

 * - start_month
   - |int|
   - Month of the start of the average (Default: 01).
   - |exposed|

 * - start_day
   - |int|
   - Day of the start of the average (Default: 01).
   - |exposed|

 * - end_year
   - |int|
   - Year of the end of the average (Default: start_year + 1).
   - |exposed|

 * - end_month
   - |int|
   - Month of the end of the average (Default: start_month).
   - |exposed|

 * - end_day
   - |int|
   - Day of the end of the average (Default: start_day).
   - |exposed|

 * - sun_scale
   - |float|
   - Scale factor for the sun radiance (Default: 1).
     Can be used to turn the sun off (by setting it to 0).
   - |exposed|

 * - sky_scale
   - |float|
   - Scale factor for the sky radiance (Default: 1).
     Can be used to turn the sky off (by setting it to 0).
   - |exposed|

 * - sun_aperture
   - |float|
   - Aperture angle of the sun in degrees (Default: 0.5338, normal sun aperture).

 * - shutter_open
   - |float|
   - Shutter opening time (Default: 0).
     Used to vary sunsky appearance

 * - shutter_close
   - |float|
   - Shutter closing time (Default: 1).
     Used to vary sunsky appearance

 * - complex_sun
   - |bool|
   - Use a more complex sun model that adds gradients to the sun disk (Default: false).
     This is more expensive to evaluate, but produces a more realistic sun appearance.
     Both implementations integrate to the same total power.

 * - to_world
   - |transform|
   - Specifies an optional emitter-to-world transformation.  (Default: none, i.e. emitter space = world space)
   - |exposed|


This emitter represents a sun and sky environment emitter for a dynamic time interval
(where time is passed as attribute of the various query records).
It is particularly useful for applications like architectural visualization or horticultural studies,
where the goal is to simulate the lighting conditions over multiple days, months, years, or
even longer, rather than the lighting at a specific instant. If the goal is to
render using the sunsky background emitter at a fixed point in time,
please take a look at the :ref:`sunsky <Sunsky Emitter>` that is optimised and more efficient for that.

The local reference frame of this emitter is Z-up and X being towards the north direction.
This behaviour can be changed with the ``to_world`` parameter.

The plugin works by dynamically computing the Hosek-Wilkie sun :cite:`HosekSun2013` and sky model
:cite:`HosekSky2012` for the given time and direction of the ray/sample.
The time parameter is controlled by the ``shutter_open`` and ``shutter_close``
parameters that should thus be the same as the sensor's.

**Render with default settings and HDR film yielding an average over a year:**

.. image:: https://d38rqfq1h7iukm.cloudfront.net/media/uploads/wjakob/2025/09/15/sunsky/emitter_timed_sunsky.jpg
   :width: 80%


.. tabs::
    .. code-tab:: xml
        :name: timed_sunsky-light

        <emitter type="timed_sunsky">
            <integer name="start_year" value="2026"/>
        </emitter>

    .. code-tab:: python

        'type': 'timed_sunsky',
        'start_year': 2026

.. warning::

    - Note that attaching a ``timed_sunsky`` emitter to the scene introduces physical units
      into the rendering process of Mitsuba 3, which is ordinarily a unitless system.
      Specifically, the evaluated spectral radiance has units of power (:math:`W`) per
      unit area (:math:`m^{-2}`) per steradian (:math:`sr^{-1}`) per unit wavelength
      (:math:`nm^{-1}`). As a consequence, your scene should be modeled in meters for
      this plugin to work properly.

    - The sun is an intense light source that subtends a tiny solid angle. This can
      be a problem for certain rendering techniques (e.g. path tracing), which produce
      high variance output (i.e. noise in renderings) when the scene also contains
      specular or glossy or materials.

    - Please be aware that given certain parameters, the sun's radiance is
      ill-represented by the linear sRGB color space. Whether Mitsuba is rendering in
      spectral or RGB mode, if the final output is an sRGB image, it can happen that
      it contains negative pixel values or be over-saturated. These results are left
      un-clamped to let the user post-process the image to their liking, without
      losing information.

    - Note that this emitter is dependent on a valid sensor shutter open and close
      time. The sensor's defaults being 0 and 0 respectively,
      this emitter will not see the time vary. Please set a valid shutter
      open and close time and pass the same time parameters to this plugin.

*/
template <typename Float, typename Spectrum>
class TimedSunskyEmitter final: public BaseSunskyEmitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BaseSunskyEmitter,
        m_turbidity, m_sky_scale, m_sun_scale, m_albedo,
        m_sun_half_aperture, m_sky_rad_dataset, m_tgmm_tables,
        m_sky_params_dataset, m_sun_radiance,
        CHANNEL_COUNT, m_to_world, m_sky_irrad_dataset, m_sun_irrad_dataset
    )

    using typename Base::FloatStorage;

    using typename Base::SkyRadData;
    using typename Base::SkyParamsData;

    using typename Base::USpec;
    using typename Base::USpecUInt32;
    using typename Base::USpecMask;
    using typename Base::FullSpectrum;

    using SamplingTexture = dr::Texture<Float, 1>;
    using SunIrradTexture = dr::Texture<Float, 1>;

    MI_IMPORT_TYPES()

    TimedSunskyEmitter(const Properties &props) : Base(props) {
        m_shutter_open          = props.get<ScalarFloat>("shutter_open", 0.f);
        m_inv_shutter_open_time = props.get<ScalarFloat>("shutter_close", 1.f) - m_shutter_open;

        if (m_inv_shutter_open_time < 0)
            Log(Error, "Shutter opening time must be less than or equal to the shutter "
                       "closing time!");

        m_inv_shutter_open_time = 1.f / m_inv_shutter_open_time;

        ScalarFloat window_start_time = props.get<ScalarFloat>("window_start_time", 7.f);
        if (window_start_time < 0.f || window_start_time > 24.f)
            Log(Error, "Start hour: %f is out of range [0, 24]", window_start_time);
        m_window_start_time = window_start_time;

        ScalarFloat window_end_time = props.get<ScalarFloat>("window_end_time", 19.f);
        if (window_end_time < 0.f || window_end_time > 24.f)
            Log(Error, "Start hour: %f is out of range [0, 24]", window_end_time);
        m_window_end_time = window_end_time;

        if (window_start_time > window_end_time)
            Log(Error, "The given start time is greater than the end time");

        m_location = LocationRecord<Float>{props};
        DateTimeRecord<ScalarFloat> start_date, end_date;
        start_date.year = props.get<ScalarInt32>("start_year", 2025);
        start_date.month = props.get<ScalarInt32>("start_month", 1);
        start_date.day = props.get<ScalarInt32>("start_day", 1);

        end_date.year = props.get<ScalarInt32>("end_year", start_date.year + 1);
        end_date.month = props.get<ScalarInt32>("end_month", start_date.month);
        end_date.day = props.get<ScalarInt32>("end_day", start_date.day);

        m_start_date = start_date;
        m_end_date = end_date;
        dr::make_opaque(
            m_window_start_time, m_window_end_time,
            m_start_date, m_end_date, m_location
        );

        m_nb_days = DateTimeRecord<Float>::get_days_between(m_start_date, m_end_date, m_location);

        m_sky_rad = Base::bilinear_interp(m_sky_rad_dataset, m_albedo, m_turbidity);
        m_sky_params = Base::bilinear_interp(m_sky_params_dataset, m_albedo, m_turbidity);

        std::tie(m_sky_sampling_weight_tex, m_sun_irrad_tex) = update_irradiance_data();

        dr::eval(m_nb_days, m_sky_rad, m_sky_params, m_sky_sampling_weight_tex, m_sun_irrad_tex);
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("latitude",  m_location.latitude,  ParamFlags::NonDifferentiable);
        cb->put("longitude", m_location.longitude, ParamFlags::NonDifferentiable);
        cb->put("timezone",  m_location.timezone,  ParamFlags::NonDifferentiable);
        cb->put("window_start_time", m_window_start_time, ParamFlags::NonDifferentiable);
        cb->put("window_end_time", m_window_end_time, ParamFlags::NonDifferentiable);
        cb->put("start_year", m_start_date.year, ParamFlags::NonDifferentiable);
        cb->put("start_month", m_start_date.month, ParamFlags::NonDifferentiable);
        cb->put("start_day", m_start_date.day, ParamFlags::NonDifferentiable);
        cb->put("end_year", m_end_date.year, ParamFlags::NonDifferentiable);
        cb->put("end_month", m_end_date.month, ParamFlags::NonDifferentiable);
        cb->put("end_day", m_end_date.day, ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        Base::parameters_changed(keys);

        dr::make_opaque(
            m_location,
            m_window_start_time, m_window_end_time,
            m_start_date, m_end_date
        );

        m_nb_days = DateTimeRecord<Float>::get_days_between(m_start_date, m_end_date, m_location);

        m_sky_rad = Base::bilinear_interp(m_sky_rad_dataset, m_albedo, m_turbidity);
        m_sky_params = Base::bilinear_interp(m_sky_params_dataset, m_albedo, m_turbidity);

        std::tie(m_sky_sampling_weight_tex, m_sun_irrad_tex) = update_irradiance_data();

        dr::eval(m_nb_days, m_sky_rad, m_sky_params,
            m_sky_sampling_weight_tex, m_sun_irrad_tex);
    }

    std::string to_string() const override {
        std::string base_str = Base::to_string();
        std::ostringstream oss;
        oss << "TimedSunskyEmitter["
            << "\n\tWindow start time = " << m_window_start_time
            << "\n\tWindow end time = " << m_window_end_time
            << "\n\tStart year = " << m_start_date.year
            << "\n\tStart month = " << m_start_date.month
            << "\n\tStart day = " << m_start_date.day
            << "\n\tEnd year = " << m_end_date.year
            << "\n\tEnd month = " << m_end_date.month
            << "\n\tEnd day = " << m_end_date.day
            << "\n\tLocation = " << m_location.to_string()
            << base_str << "\n]";
        return oss.str();
    }

    MI_DECLARE_CLASS(TimedSunskyEmitter)

private:

    Point2f get_sun_angles(const Float& time) const override {
        DateTimeRecord<Float> date_time = dr::zeros<DateTimeRecord<Float>>();
        date_time.year = m_start_date.year;
        date_time.month = m_start_date.month;

        Float remaped_time = (time - m_shutter_open) * m_inv_shutter_open_time;

        Float day = remaped_time * (m_nb_days - dr::Epsilon<Float>);
        Int32 int_day = dr::floor2int<Int32>(day);

        date_time.day = m_start_date.day + int_day;
        date_time.hour = m_window_start_time + (m_window_end_time - m_window_start_time) * (day - int_day);

        const auto [sun_elevation, sun_azimuth] = Base::sun_coordinates(date_time, m_location);
        return {sun_azimuth, sun_elevation};
    }

    std::pair<SkyRadData, SkyParamsData>
    get_sky_datasets(const Float& sun_theta, const USpecUInt32& channel_idx, const USpecMask& active) const override {
        const Float sun_eta = 0.5f * dr::Pi<Float> - sun_theta;
        USpecMask active_dataset = active & (sun_eta >= 0.f);

        return std::make_pair(
            bezier_interp<SkyRadData>(m_sky_rad, channel_idx, sun_eta, active_dataset),
            bezier_interp<SkyParamsData>(m_sky_params, channel_idx, sun_eta, active_dataset)
        );
    }

    Float get_sky_sampling_weight(const Float& sun_theta, const Mask& active) const override {
        Mask valid_elevation = active & (sun_theta <= 0.5f * dr::Pi<Float>);
        Float sun_idx = 0.5f * dr::Pi<Float> - sun_theta;
              sun_idx = (dr::rad_to_deg(sun_idx) - 2.f) / 3.f;
              sun_idx /= ELEVATION_CTRL_PTS;

        Float res;
        m_sky_sampling_weight_tex->eval(dr::Array<Float, 1>(sun_idx), &res, valid_elevation);

        return dr::select(res == 0.f, 1.f, res);
    }


    USpec get_sun_irradiance(const Float& sun_theta, const USpecUInt32& channel_idx, const USpecMask& active) const override {
        USpecMask valid_elevation = active & (sun_theta <= 0.5f * dr::Pi<Float>);
        Float sun_idx = 0.5f * dr::Pi<Float> - sun_theta;
              sun_idx = (dr::rad_to_deg(sun_idx) - 2.f) / 3.f;
              sun_idx /= ELEVATION_CTRL_PTS;

        Float res[CHANNEL_COUNT] = { 0.f };
        m_sun_irrad_tex->eval(dr::Array<Float, 1>(sun_idx), res, dr::any(valid_elevation));

        USpec irradiance = 0.f;
        for (uint32_t channel = 0; channel < CHANNEL_COUNT; ++channel)
            dr::masked(irradiance, channel_idx == channel) = res[channel];

        return irradiance;
    }

    std::pair<UInt32, Float> sample_reuse_tgmm(const Float& sample, const Float& sun_theta, const Mask& active) const override {
        const auto [ lerp_w, tgmm_idx ] = Base::get_tgmm_data(sun_theta);

        Mask active_loop = active;
        Float last_cdf = 0.f, cdf = 0.f;
        UInt32 res_gaussian_idx = 0;
        for (uint32_t mixture_idx = 0; mixture_idx < 4; ++mixture_idx) {
            for (uint32_t gaussian_idx = 0; gaussian_idx < TGMM_COMPONENTS; ++gaussian_idx) {
                dr::masked(last_cdf, active_loop) = cdf;

                dr::masked(res_gaussian_idx, active_loop) =
                    tgmm_idx[mixture_idx] + gaussian_idx;

                Float gaussian_w = lerp_w[mixture_idx] * dr::gather<Float>(m_tgmm_tables,
                    res_gaussian_idx * TGMM_GAUSSIAN_PARAMS + (TGMM_GAUSSIAN_PARAMS - 1),
                    active_loop
                );

                // Gathered weight is 0 if inactive
                cdf += gaussian_w;

                active_loop &= cdf < sample;

            }
        }

        return std::make_pair(res_gaussian_idx, (sample - last_cdf) / (cdf - last_cdf));
    }

    std::pair<Wavelength, Spectrum> sample_wlgth(const Float& sample, Mask active) const override {
        if constexpr (is_spectral_v<Spectrum>) {
            static constexpr ScalarFloat
                    min_w = std::max(ScalarFloat(MI_CIE_MIN), WAVELENGTHS<ScalarFloat>[0]),
                    max_w = std::min(ScalarFloat(MI_CIE_MAX), WAVELENGTHS<ScalarFloat>[WAVELENGTH_COUNT - 1]);

            Wavelength wavelengths = math::sample_shifted<Wavelength>(sample);
            wavelengths = min_w + (max_w - min_w) * wavelengths;

            return { wavelengths, /* 1/pdf = */ (max_w - min_w) };
        } else {
            DRJIT_MARK_USED(sample);
            DRJIT_MARK_USED(active);

            NotImplementedError("sample_wavelengths")
        }
    }

    // ================================================================================================
    // ====================================== HELPER FUNCTIONS ========================================
    // ================================================================================================

    template<typename Dataset>
    Dataset bezier_interp(const TensorXf& dataset, const USpecUInt32& channel_idx, const Float& eta, const USpecMask& active) const {
        Dataset res = dr::zeros<Dataset>();

        Float x = dr::cbrt(2 * dr::InvPi<Float> * eta);
              x = dr::minimum(x, dr::OneMinusEpsilon<Float>);
        constexpr ScalarFloat coefs[SKY_CTRL_PTS] = {1, 5, 10, 10, 5, 1};

        Float x_pow = 1.f, x_pow_inv = dr::pow(1.f - x, SKY_CTRL_PTS - 1);
        Float x_pow_inv_scale = dr::rcp(1.f - x);
        for (uint32_t ctrl_pt = 0; ctrl_pt < SKY_CTRL_PTS; ++ctrl_pt) {
            TensorXf temp = dr::take(dataset, ctrl_pt);
            Dataset data_ctrl_pt = dr::gather<Dataset>(
                temp.array(), channel_idx, active
            );

            res += data_ctrl_pt * coefs[ctrl_pt] * x_pow * x_pow_inv;

            x_pow *= x;
            x_pow_inv *= x_pow_inv_scale;
        }

        return res;
    }

    /**
     * Updates the textures used for importance sampling the sun and sky as
     * well as the sun's irradiance dataset.
     */
    std::pair<ref<SamplingTexture>, ref<SunIrradTexture>> update_irradiance_data() const {
        using UInt32Storage = dr::uint32_array_t<FloatStorage>;
        using Color3fStorage = mitsuba::Color<FloatStorage, 3>;
        using FullSpectrumStorage = unpolarized_spectrum_t<
            mitsuba::Spectrum<FloatStorage, WAVELENGTH_COUNT>
        >;

        ref<SamplingTexture> sky_weight_tex;
        ref<SunIrradTexture> sun_irrad_tex;

        UInt32Storage elevation_idx = dr::arange<UInt32Storage>(ELEVATION_CTRL_PTS);
        FullSpectrumStorage wavelengths = {
            320, 360, 400, 440, 480, 520, 560, 600, 640, 680, 720
        };

        FloatStorage sky_irrad_data = dr::take_interp(m_sky_irrad_dataset, m_turbidity - 1.f).array(),
                     sun_irrad_data = dr::take_interp(m_sun_irrad_dataset, m_turbidity - 1.f).array();

        FullSpectrumStorage sky_irrad = dr::gather<FullSpectrumStorage>(sky_irrad_data, elevation_idx),
                            sun_irrad = dr::gather<FullSpectrumStorage>(sun_irrad_data, elevation_idx);

        // Sampling weights
        {
            FloatStorage sky_lum = m_sky_scale * luminance(sky_irrad, wavelengths),
                         sun_lum = m_sun_scale * luminance(sun_irrad, wavelengths),
                         sampling_weigths = sky_lum / (sky_lum + sun_lum);

            dr::masked(sampling_weigths, !dr::isfinite(sampling_weigths)) = 0.f;

            const size_t shape[2] = { ELEVATION_CTRL_PTS, 1 };
            TensorXf temp = TensorXf(sampling_weigths, 2, shape);

            sky_weight_tex = new SamplingTexture(temp, true, true, dr::FilterMode::Linear, dr::WrapMode::Clamp);
        }

        // Sun irradiance
        {
            // Convert to RGB if needed
            if constexpr (is_rgb_v<Spectrum>) {
                // Cancel out the CIE Y normalization factor since it will be multiplied later
                Color3fStorage rgb_sun_irrad = spectrum_to_srgb(sun_irrad, wavelengths) / ScalarFloat(MI_CIE_Y_NORMALIZATION);
                sun_irrad_data = dr::ravel(rgb_sun_irrad);
            }

            const size_t shape[2] = { ELEVATION_CTRL_PTS, CHANNEL_COUNT };
            TensorXf temp = TensorXf(sun_irrad_data, 2, shape);

            sun_irrad_tex = new SunIrradTexture(temp, true, true, dr::FilterMode::Linear, dr::WrapMode::Clamp);
        }

        return std::make_pair(sky_weight_tex, sun_irrad_tex);
    }

    // ================================================================================================
    // ========================================= ATTRIBUTES ===========================================
    // ================================================================================================

    ScalarFloat m_shutter_open, m_inv_shutter_open_time;

    Float m_window_start_time, m_window_end_time;
    DateTimeRecord<Float> m_start_date, m_end_date;
    LocationRecord<Float> m_location;

    Int32 m_nb_days;

    // ========= Radiance parameters =========
    TensorXf m_sky_rad;
    TensorXf m_sky_params;

    // ========= Sampling parameters =========
    /// Sampling weights (sun vs sky) for each elevation
    ref<SamplingTexture> m_sky_sampling_weight_tex;
    ref<SunIrradTexture> m_sun_irrad_tex;

    MI_TRAVERSE_CB(
        Base,
        Base::m_bsphere,
        Base::m_turbidity,
        Base::m_albedo_tex,
        Base::m_albedo,
        Base::m_sun_radiance,
        Base::m_sky_rad_dataset,
        Base::m_sky_params_dataset,
        Base::m_sun_ld,
        Base::m_sun_rad_dataset,
        Base::m_sky_irrad_dataset,
        Base::m_sun_irrad_dataset,
        Base::m_tgmm_tables,
        m_window_start_time,
        m_window_end_time,
        m_start_date,
        m_end_date,
        m_location,
        m_nb_days,
        m_sky_rad,
        m_sky_params,
        m_sky_sampling_weight_tex,
        m_sun_irrad_tex
    )
};

MI_EXPORT_PLUGIN(TimedSunskyEmitter)
NAMESPACE_END(mitsuba)
