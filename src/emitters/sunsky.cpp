#include <mitsuba/core/quad.h>
#include <mitsuba/render/srgb.h>
#include <mitsuba/render/sunsky.h>


NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-sunsky:

Sun and sky emitter (:monosp:`sunsky`)
--------------------------------------

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

 * - year
   - |int|
   - Year (Default: 2010).
   - |exposed|

 * - month
   - |int|
   - Month (Default: 7).
   - |exposed|

 * - day
   - |int|
   - Day (Default: 10).
   - |exposed|

 * - hour
   - |float|
   - Hour (Default: 15).
   - |exposed|

 * - minute
   - |float|
   - Minute (Default: 0).
   - |exposed|

 * - second
   - |float|
   - Second (Default: 0).
   - |exposed|

 * - sun_direction
   - |vector|
   - Direction of the sun in the sky (No defaults),
     cannot be specified along with one of the location/time parameters.
   - |exposed|, |differentiable|

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

 * - complex_sun
   - |bool|
   - Use a more complex sun model that adds gradients to the sun disk (Default: false).
     This is more expensive to evaluate, but produces a more realistic sun appearance.
     Both implementations integrate to the same total power.

 * - to_world
   - |transform|
   - Specifies an optional emitter-to-world transformation.  (Default: none, i.e. emitter space = world space)
   - |exposed|

This plugin implements an environment emitter for the sun and sky dome.
It uses the Hosek-Wilkie sun :cite:`HosekSun2013` and sky model
:cite:`HosekSky2012` to generate strong approximations of the sky-dome without
the cost of path tracing the atmosphere.

The local reference frame of this emitter is Z-up and X being towards the north direction.
This behaviour can be changed with the ``to_world`` parameter.

Internally, this emitter does not compute a bitmap of the sky-dome like an
environment map, but evaluates the spectral radiance whenever it is needed.
Consequently, sampling is done through a Truncated Gaussian Mixture Model
pre-fitted to the given parameters :cite:`vitsas2021tgmm`.

Parameter influence
********************

**Albedo (sky only)**

.. subfigstart::
.. subfigure:: https://d38rqfq1h7iukm.cloudfront.net/media/uploads/wjakob/2025/09/15/sunsky/sunsky_03_0_10.png
   :caption: :monosp:`albedo=0`
.. subfigure:: https://d38rqfq1h7iukm.cloudfront.net/media/uploads/wjakob/2025/09/15/sunsky/sunsky_03_1_10.png
   :caption: :monosp:`albedo=20% green`
.. subfigure:: https://d38rqfq1h7iukm.cloudfront.net/media/uploads/wjakob/2025/09/15/sunsky/sunsky_03_10_10.png
   :caption: :monosp:`albedo=1`
.. subfigend::
   :label: fig-sunsky-alb

**Time and Location (sky only)**

.. image:: https://d38rqfq1h7iukm.cloudfront.net/media/uploads/wjakob/2025/09/15/sunsky/emitter_sunsky_time_docs.svg
   :width: 200%

**Turbidity (sky only)**

.. image:: https://d38rqfq1h7iukm.cloudfront.net/media/uploads/wjakob/2025/09/15/sunsky/emitter_sunsky_turb_docs.svg
   :width: 200%

**Sun and sky scale**

.. subfigstart::
.. subfigure:: https://d38rqfq1h7iukm.cloudfront.net/media/uploads/wjakob/2025/09/15/sunsky/emitter_sky.jpg
   :caption: Sky only :monosp:`sun_scale=0`
.. subfigure:: https://d38rqfq1h7iukm.cloudfront.net/media/uploads/wjakob/2025/09/15/sunsky/emitter_sun.jpg
   :caption: Sun only :monosp:`sky_scale=0`
.. subfigure:: https://d38rqfq1h7iukm.cloudfront.net/media/uploads/wjakob/2025/09/15/sunsky/emitter_sunsky.jpg
   :caption: Sun and sky combined (default parameters)
.. subfigend::
   :label: fig-sunsky

.. warning::

    - Note that attaching a ``sunsky`` emitter to the scene introduces physical units
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

.. tabs::
    .. code-tab:: xml
        :name: sunsky-light

        <emitter type="sunsky">
            <float name="hour" value="20.0"/>
        </emitter>

    .. code-tab:: python

        'type': 'sunsky',
        'hour': 20.0

*/

template <typename Float, typename Spectrum>
class SunskyEmitter final: public BaseSunskyEmitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BaseSunskyEmitter,
        m_turbidity, m_sky_scale, m_sun_scale, m_albedo, m_bsphere,
        m_sun_half_aperture, m_sky_rad_dataset, m_tgmm_tables,
        m_sky_params_dataset, CHANNEL_COUNT, m_to_world,
        m_sky_irrad_dataset, m_sun_irrad_dataset
    )
    using typename Base::FloatStorage;

    using typename Base::SkyRadData;
    using typename Base::SkyParamsData;

    using typename Base::USpec;
    using typename Base::USpecUInt32;
    using typename Base::USpecMask;

    using typename Base::FullSpectrum;

    MI_IMPORT_TYPES(Scene, Texture)

    SunskyEmitter(const Properties &props) : Base(props) {
        if (props.has_property("sun_direction")) {
            if (props.has_property("latitude") || props.has_property("longitude")
                || props.has_property("timezone") || props.has_property("year")
                || props.has_property("month") || props.has_property("day")
                || props.has_property("hour") || props.has_property("minute")
                || props.has_property("second")) {
                Log(Error, "Both the 'sun_direction' and parameters for time/location "
                           "were provided, both information cannot be given at the same time!");
                }

            m_active_record = false;
            m_sun_dir = dr::normalize(props.get<ScalarVector3f>("sun_direction"));
            dr::make_opaque(m_sun_dir);

        } else {
            m_location.latitude  = props.get<ScalarFloat>("latitude", 35.6894f);
            m_location.longitude = props.get<ScalarFloat>("longitude", 139.6917f);
            m_location.timezone  = props.get<ScalarFloat>("timezone", 9);
            m_time.year          = props.get<ScalarInt32>("year", 2010);
            m_time.month         = props.get<ScalarUInt32>("month", 7);
            m_time.day           = props.get<ScalarUInt32>("day", 10);
            m_time.hour          = props.get<ScalarFloat>("hour", 15.0f);
            m_time.minute        = props.get<ScalarFloat>("minute", 0.0f);
            m_time.second        = props.get<ScalarFloat>("second", 0.0f);

            m_active_record = true;
            dr::make_opaque(m_location, m_time);

            const auto [theta, phi] = Base::sun_coordinates(m_time, m_location);
            m_sun_dir = m_to_world.value() * sph_to_dir(theta, phi);
        }

        // ================= UPDATE ANGLES =================
        Vector3f local_sun_dir = m_to_world.value().inverse() * m_sun_dir;

        if (dr::any(local_sun_dir.z() < 0.f))
            Log(Warn, "The sun is below the horizon at the specified time and location!");

        m_sun_angles = dir_to_sph(local_sun_dir);
        m_sun_angles = { m_sun_angles.y(), m_sun_angles.x() }; // flip convention

        Float sun_eta = 0.5f * dr::Pi<Float> - m_sun_angles.y();

        // ================= Compute datasets =================
        TensorXf temp_sky_params = Base::bilinear_interp(m_sky_params_dataset, m_albedo, m_turbidity);
        m_sky_params = bezier_interp(temp_sky_params, sun_eta);

        TensorXf temp_sky_radiance = Base::bilinear_interp(m_sky_rad_dataset, m_albedo, m_turbidity);
        m_sky_radiance = bezier_interp(temp_sky_radiance, sun_eta);

        m_gaussian_distr = build_tgmm_distribution(m_tgmm_tables);

        // =============== Get irradiance data ================
        std::tie(m_sky_sampling_w, m_spectral_distr, m_sun_irrad) = update_irradiance_data();

        dr::eval(m_sky_params, m_sky_radiance, m_sky_sampling_w,
                 m_gaussian_distr, m_spectral_distr, m_sun_irrad);
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        if (m_active_record) {
            cb->put("latitude",  m_location.latitude,  ParamFlags::NonDifferentiable);
            cb->put("longitude", m_location.longitude, ParamFlags::NonDifferentiable);
            cb->put("timezone",  m_location.timezone,  ParamFlags::NonDifferentiable);
            cb->put("year",             m_time.year,              ParamFlags::NonDifferentiable);
            cb->put("day",              m_time.day,               ParamFlags::NonDifferentiable);
            cb->put("month",            m_time.month,             ParamFlags::NonDifferentiable);
            cb->put("hour",      m_time.hour,          ParamFlags::NonDifferentiable);
            cb->put("minute",    m_time.minute,        ParamFlags::NonDifferentiable);
            cb->put("second",    m_time.second,        ParamFlags::NonDifferentiable);
        }

        cb->put("sun_direction", m_sun_dir, m_active_record ? ParamFlags::ReadOnly : ParamFlags::Differentiable);

    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        Base::parameters_changed(keys);

        dr::make_opaque(m_sun_dir, m_time, m_location);

        #define CHANGED(word) string::contains(keys, word)

        bool changed_atmosphere = keys.empty() || CHANGED("albedo") || CHANGED("turbidity");
        bool changed_time_record = keys.empty() || (m_active_record && (
                CHANGED("timezone") || CHANGED("year") ||
                CHANGED("day") || CHANGED("month") || CHANGED("hour") ||
                CHANGED("minute") || CHANGED("second") || CHANGED("latitude") ||
                CHANGED("longitude")
            ));
        bool changed_sun_dir = (!m_active_record && CHANGED("sun_direction")) || changed_time_record;


        // Update sun angles
        if (changed_time_record) {
            const auto [theta, phi] = Base::sun_coordinates(m_time, m_location);
            m_sun_dir = m_to_world.value() * sph_to_dir(theta, phi);
            m_sun_angles = { phi, theta }; // flip convention
        } else if (changed_sun_dir) {
            m_sun_angles = dir_to_sph(m_to_world.value().inverse() * m_sun_dir);
            m_sun_angles = { m_sun_angles.y(), m_sun_angles.x() }; // flip convention
        }

        Float eta = 0.5f * dr::Pi<Float> - m_sun_angles.y();

        // Update sky
        if (changed_sun_dir || changed_atmosphere) {
            TensorXf temp_sky_params = Base::bilinear_interp(m_sky_params_dataset, m_albedo, m_turbidity);
            m_sky_params = bezier_interp(temp_sky_params, eta);

            TensorXf temp_sky_radiance = Base::bilinear_interp(m_sky_rad_dataset, m_albedo, m_turbidity);
            m_sky_radiance = bezier_interp(temp_sky_radiance, eta);
        }

        // Update TGMM (no dependence on albedo)
        if (changed_sun_dir || CHANGED("turbidity"))
            m_gaussian_distr = build_tgmm_distribution(m_tgmm_tables);

        // Update sky-sun ratio and radiance distribution
        std::tie(m_sky_sampling_w, m_spectral_distr, m_sun_irrad) = update_irradiance_data();

        dr::eval(m_sky_params, m_sky_radiance, m_sky_sampling_w,
                 m_gaussian_distr, m_spectral_distr, m_sun_irrad);
        #undef CHANGED
    }

    std::string to_string() const override {
        std::string base_str = Base::to_string();
        std::ostringstream oss;
        oss << "SunskyEmitter[";
        if (m_active_record) {
            oss << "\n\tLocation = " << m_location.to_string()
                << "\n\tDate and time = " << m_time.to_string();
        } else {
            oss << "\n\tSun dir = " << m_sun_dir;
        }
        oss << base_str << "\n]";
        return oss.str();
    }

    MI_DECLARE_CLASS(SunskyEmitter)

private:

    Point2f get_sun_angles(const Float&) const override {
        return m_sun_angles;
    }

    std::pair<SkyRadData, SkyParamsData>
    get_sky_datasets(const Float& /* sun_theta */, const USpecUInt32& channel_idx, const USpecMask& active) const override {
        SkyRadData mean_rad = dr::gather<SkyRadData>(m_sky_radiance, channel_idx, active);
        SkyParamsData coefs = dr::gather<SkyParamsData>(m_sky_params, channel_idx, active);

        return std::make_pair(mean_rad, coefs);
    }

    Float get_sky_sampling_weight(const Float& /* sun_theta */, const Mask& /* active */) const override {
        return m_sky_sampling_w;
    }

    USpec get_sun_irradiance(const Float& /* sun_theta */, const USpecUInt32& channel_idx, const USpecMask& active) const override {
        return dr::gather<USpec>(m_sun_irrad, channel_idx, active);
    }


    std::pair<UInt32, Float> sample_reuse_tgmm(const Float& sample, const Float& /* sun_theta */, const Mask& active) const override {
        const auto [ idx, temp_sample ] = m_gaussian_distr.sample_reuse(sample, active);
        const auto [ idx_div, idx_mod ] = dr::idivmod(idx, TGMM_COMPONENTS);

        Float sun_eta = 0.5f * dr::Pi<Float> - m_sun_angles.y();
        sun_eta = dr::rad_to_deg(sun_eta);

        UInt32 eta_idx_low = dr::floor2int<UInt32>(dr::clip((sun_eta - 2) / 3, 0, ELEVATION_CTRL_PTS - 1)),
               t_idx_low   = dr::floor2int<UInt32>(dr::clip(m_turbidity - 2, 0, (TURBITDITY_LVLS - 1) - 1));

        constexpr uint32_t t_block_size = Base::GAUSSIAN_NB / (TURBITDITY_LVLS - 1),
                 result_size  = t_block_size / ELEVATION_CTRL_PTS;

        UInt32 gaussian_idx = (t_idx_low + ((idx_div >> 1) & 1)) * t_block_size +
                              (eta_idx_low + (idx_div & 1)) * result_size +
                              idx_mod;

        return std::make_pair(gaussian_idx, temp_sample);
    }

    std::pair<Wavelength, Spectrum> sample_wlgth(const Float& sample, Mask active) const override {
        if constexpr (is_spectral_v<Spectrum>) {
            Wavelength w_sample = math::sample_shifted<Wavelength>(sample);

            Spectrum pdf;
            Wavelength wavelengths;
            std::tie(wavelengths, pdf) =
                m_spectral_distr.sample_pdf(w_sample, active);

            return { wavelengths, dr::rcp(pdf) };
        } else {
            DRJIT_MARK_USED(sample);
            DRJIT_MARK_USED(active);

            NotImplementedError("sample_wavelengths")
        }
    }

    // ================================================================================================
    // ===================================== SAMPLING FUNCTIONS =======================================
    // ================================================================================================

    /**
     * \brief Extracts the Gaussian Mixture Model parameters from the TGMM
     * dataset The 4 * (5 gaussians) cannot be interpolated directly, so we need
     * to combine them and adjust the weights based on the elevation and
     * turbidity linear interpolation parameters.
     *
     * \param tgmm_tables
     *      Dataset for the Gaussian Mixture Models
     * \return
     *      The new distribution parameters and the mixture weights
     */
    DiscreteDistribution<Float> build_tgmm_distribution(const FloatStorage &tgmm_tables) const {
        using UInt32Storage = DynamicBuffer<UInt32>;

        // ============== EXTRACT INDEXES AND LERP WEIGHTS ==============
        const auto [ lerp_w, tgmm_idx ] = Base::get_tgmm_data(m_sun_angles.y());

        // ==================== EXTRACT PARAMETERS AND APPLY LERP WEIGHT ====================
        FloatStorage gaussian_weights = dr::zeros<FloatStorage>(4 * TGMM_COMPONENTS);
        UInt32Storage write_weight_idx = dr::arange<UInt32Storage>(TGMM_COMPONENTS);
        UInt32Storage read_weight_idx = TGMM_GAUSSIAN_PARAMS * write_weight_idx + (TGMM_GAUSSIAN_PARAMS - 1);

        for (ScalarUInt32 mixture_idx = 0; mixture_idx < 4; ++mixture_idx) {

            // Extract gaussian weights for the given mixture
            FloatStorage tgmm_weights = dr::gather<FloatStorage>(tgmm_tables, TGMM_GAUSSIAN_PARAMS * tgmm_idx[mixture_idx] + read_weight_idx);

            // Scatter back the weights for the current mixture
            dr::scatter(gaussian_weights, lerp_w[mixture_idx] * tgmm_weights, mixture_idx * TGMM_COMPONENTS + write_weight_idx);
        }

        return DiscreteDistribution<Float>(gaussian_weights);
    }

    // ================================================================================================
    // ====================================== HELPER FUNCTIONS ========================================
    // ================================================================================================

    FloatStorage bezier_interp(const TensorXf& dataset, const Float& eta) const {
        FloatStorage res = dr::zeros<FloatStorage>(dataset.size() / dataset.shape(0));

        Float x = dr::cbrt(2 * dr::InvPi<Float> * eta);
              x = dr::minimum(x, dr::OneMinusEpsilon<Float>);
        constexpr ScalarFloat coefs[SKY_CTRL_PTS] = {1, 5, 10, 10, 5, 1};

        Float x_pow = 1.f, x_pow_inv = dr::pow(1.f - x, SKY_CTRL_PTS - 1);
        Float x_pow_inv_scale = dr::rcp(1.f - x);
        for (uint32_t ctrl_pt = 0; ctrl_pt < SKY_CTRL_PTS; ++ctrl_pt) {
            FloatStorage data = dr::take(dataset, ctrl_pt).array();
            res += coefs[ctrl_pt] * x_pow * x_pow_inv * data;

            x_pow *= x;
            x_pow_inv *= x_pow_inv_scale;
        }

        return res;
    }

    /**
     * \brief Updates the sky sampling data and sun irradiance values
     * based on the current parameters.
     */
    std::tuple<Float, ContinuousDistribution<Wavelength>, FloatStorage> update_irradiance_data() const {
        Float angle = 0.5f * dr::Pi<Float> - m_sun_angles.y();
              angle = dr::clip((dr::rad_to_deg(angle) - 2.f) / 3.f,
                            0.f, ELEVATION_CTRL_PTS - dr::OneMinusEpsilon<ScalarFloat>);

        // Interpolate on turbidity and sun angle
        constexpr auto bilinear_interp = [](const TensorXf& dataset, const Float& turb, const Float& angle) {
            TensorXf tmp = dr::take_interp(dataset, turb - 1.f);
            return dr::take_interp(tmp, angle).array();
        };

        FloatStorage sky_irrad = bilinear_interp(m_sky_irrad_dataset, m_turbidity, angle),
                     sun_irrad = bilinear_interp(m_sun_irrad_dataset, m_turbidity, angle);

        // Extract luminance and downsample sun irradiance to RGB
        FloatStorage spec_sun_irrad = sun_irrad;
        Float sky_lum = m_sky_scale, sun_lum = m_sun_scale;
        {

            FullSpectrum sky_irrad_spec = dr::gather<FullSpectrum>(sky_irrad, UInt32(0)),
                         sun_irrad_spec = dr::gather<FullSpectrum>(sun_irrad, UInt32(0));

            FullSpectrum wavelengths = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
                         wavelengths = WAVELENGTH_STEP * wavelengths + WAVELENGTHS<ScalarFloat>[0];

            sky_lum *= luminance(sky_irrad_spec, wavelengths);
            sun_lum *= luminance(sun_irrad_spec, wavelengths) *
                            Base::get_area_ratio(m_sun_half_aperture);

            if constexpr (is_rgb_v<Spectrum>)
                // Cancel out the CIE Y normalization factor since it will be multiplied later
                spec_sun_irrad = dr::ravel(spectrum_to_srgb(sun_irrad_spec, wavelengths) / ScalarFloat(MI_CIE_Y_NORMALIZATION));

        }

        // Sky sampling weight
        Float sky_weight = sky_lum / (sky_lum + sun_lum);
              sky_weight = dr::select(dr::isnan(sky_weight), 0.f, sky_weight);

        // Spectral sampling
        ScalarVector2f range = {
            WAVELENGTHS<ScalarFloat>[0],
            WAVELENGTHS<ScalarFloat>[WAVELENGTH_COUNT - 1]
        };
        ContinuousDistribution<Wavelength> wav_dist;
        if constexpr (is_spectral_v<Spectrum>) {
            FloatStorage avg_spec = sun_irrad + sky_irrad;
            if (dr::all(avg_spec == 0.f))
                avg_spec += 1.f; // Prevent error in the distribution

            wav_dist = { range, avg_spec };
        } else {
            ScalarFloat pdf[2] = { 1.f, 1.f };
            wav_dist = { range, pdf, 2 };
        }

        return std::make_tuple(sky_weight, wav_dist, spec_sun_irrad);
    }

    // ================================================================================================
    // ========================================= ATTRIBUTES ===========================================
    // ================================================================================================

    /// Sun direction in world coordinates
    Vector3f m_sun_dir;
    /// Sun angles in local coordinates, (phi, theta)
    Point2f m_sun_angles;

    /// Indicates if the plugin was initialized with a location/time record
    bool m_active_record;
    DateTimeRecord<Float> m_time;
    LocationRecord<Float> m_location;

    // ========= Radiance parameters =========
    FloatStorage m_sky_params;
    FloatStorage m_sky_radiance;

    // Irradiance data
    Float m_sky_sampling_w;
    FloatStorage m_sun_irrad;

    // ========= Sampling parameters =========
    DiscreteDistribution<Float> m_gaussian_distr;
    ContinuousDistribution<Wavelength> m_spectral_distr;

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
        Base::m_tgmm_tables,
        Base::m_sky_irrad_dataset,
        Base::m_sun_irrad_dataset,
        m_sun_dir,
        m_sun_angles,
        m_time,
        m_location,
        m_sky_params,
        m_sky_radiance,
        m_sky_sampling_w,
        m_sun_irrad,
        m_gaussian_distr,
        m_spectral_distr
    )
};

MI_EXPORT_PLUGIN(SunskyEmitter)
NAMESPACE_END(mitsuba)
