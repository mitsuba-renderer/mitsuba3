#include <mitsuba/core/bsphere.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/sunsky.h>


NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-sunsky:

Sun and sky emitter (:monosp:`sunsky`)
-------------------------------------------------

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
   - |exposed|

 * - to_world
   - |transform|
   - Specifies an optional emitter-to-world transformation.  (Default: none, i.e. emitter space = world space)
   - |exposed|

This plugin implements an environment emitter for the sun and sky dome.
It uses the Hosek-Wilkie sun :cite:`HosekSun2013` and sky model
:cite:`HosekSky2012` to generate strong approximations of the sky-dome without
the cost of path tracing the atmosphere.

Internally, this emitter does not compute a bitmap of the sky-dome like an
environment map, but evaluates the spectral radiance whenever it is needed.
Consequently, sampling is done through a Truncated Gaussian Mixture Model
pre-fitted to the given parameters :cite:`vitsas2021tgmm`.

Users should be aware that given certain parameters, the sun's radiance is
ill-represented by the linear sRGB color space. Whether Mitsuba is rendering in
spectral or RGB mode, if the final output is an sRGB image, it can happen that
it contains negative pixel values or be over-saturated. These results are left
un-clamped to let the user post-process the image to their liking, without
losing information.

Note that attaching a ``sunsky`` emitter to the scene introduces physical units
into the rendering process of Mitsuba 3, which is ordinarily a unitless system.
Specifically, the evaluated spectral radiance has units of power (:math:`W`) per
unit area (:math:`m^{-2}`) per steradian (:math:`sr^{-1}`) per unit wavelength
(:math:`nm^{-1}`). As a consequence, your scene should be modeled in meters for
this plugin to work properly.

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
class TimedSunskyEmitter final: public BaseSunskyEmitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BaseSunskyEmitter,
        m_turbidity, m_sky_scale, m_sun_scale, m_albedo, m_bsphere,
        m_sun_half_aperture, m_location, m_sky_rad_dataset,
        m_sky_params_dataset, m_sun_ld, m_sun_rad_dataset,
        CHANNEL_COUNT, m_to_world
    )

    MI_IMPORT_TYPES()

    using ArrayXf = dr::DynamicArray<Float>;
    using FloatStorage = DynamicBuffer<Float>;

    TimedSunskyEmitter(const Properties &props) : Base(props) {

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

        dr::make_opaque(m_window_start_time, m_window_end_time,
                        m_start_date, m_end_date, m_location);


        m_sky_params = Base::bilinear_interp(m_sky_params_dataset, m_albedo, m_turbidity);
        m_sky_radiance = Base::bilinear_interp(m_sky_rad_dataset, m_albedo, m_turbidity);
        m_sun_radiance = dr::take_interp(m_sun_rad_dataset, m_turbidity - 1.f);
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("latitude",  m_location.latitude,  ParamFlags::NonDifferentiable);
        cb->put("longitude", m_location.longitude, ParamFlags::NonDifferentiable);
        cb->put("timezone",  m_location.timezone,  ParamFlags::NonDifferentiable);
        NotImplementedError()
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        Base::parameters_changed(keys);
        NotImplementedError()
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        using Spec = unpolarized_spectrum_t<Spectrum>;
        using UInt32Spec = dr::uint32_array_t<Spec>;

        // These typedefs concatenate the discrete spectrae together
        // to make it easier to iterate over them for interpolation
        using ExtendedSpec = dr::Array<Float, (is_spectral_v<Spec> ? 2 : 1) * dr::size_v<Spec>>;
        using ExtendedSpecUInt32 = dr::uint32_array_t<ExtendedSpec>;
        using ExtendedSpecMask   = dr::mask_t<ExtendedSpec>;

        Datasets datasets = compute_dataset(si.time);
        ArrayXf sky_rad_data = datasets.sky_rad, sky_params_data = datasets.sky_params;

        // Compute angles
        Vector3f local_wo = m_to_world.value().inverse() * (-si.wi);
        Float cos_theta = Frame3f::cos_theta(local_wo),
              gamma = dr::unit_angle(datasets.sun_dir, local_wo);

        active &= cos_theta >= 0;
        Mask hit_sun = dr::dot(datasets.sun_dir, local_wo) >= dr::cos(m_sun_half_aperture);

        // Compute interpolation coefficients and indices for the wavelengths
        ExtendedSpec lerp_factor;
        ExtendedSpecUInt32 channel_idx;
        ExtendedSpecMask valid_idx = active;
        if constexpr (is_rgb_v<Spec>) {
            lerp_factor = 1.f;
            channel_idx = {0, 1, 2};
        } else if constexpr (is_spectral_v<Spec>) {
            Wavelength normalized_wavelengths =
                (si.wavelengths - WAVELENGTHS<ScalarFloat>[0]) / WAVELENGTH_STEP;

            UInt32Spec query_idx_low = dr::floor2int<UInt32Spec>(normalized_wavelengths);
            channel_idx = dr::concat(query_idx_low, query_idx_low + 1);

            Wavelength lerp_factor_wavelength = normalized_wavelengths - query_idx_low;
            lerp_factor = dr::concat(lerp_factor_wavelength, 1.f - lerp_factor_wavelength);

            valid_idx &= (0.f <= lerp_factor) & (lerp_factor <= 1.f);
        }

        // Evaluate the model channel by channel
        Spec res = 0.f;
        for (uint32_t idx = 0; idx < dr::size_v<ExtendedSpec>; ++idx) {
            Float sky_rad = m_sky_scale * Base::template eval_sky<Float>(channel_idx[idx], cos_theta, gamma, sky_params_data, sky_rad_data, valid_idx[idx]);

            Float sun_rad = m_sun_scale * get_area_ratio(m_sun_half_aperture) *
                eval_sun<Float, is_rgb_v<Spec>>(channel_idx[idx], cos_theta, gamma, m_sun_radiance, m_sun_half_aperture, hit_sun & valid_idx[idx]);

            if constexpr (is_rgb_v<Spec>) {
                sun_rad *= SPEC_TO_RGB_SUN_CONV;
            } else if constexpr (is_spectral_v<Spec>) {
                // TODO sort sun limb darkening
                sun_rad *= 1.f;
            }

            res[idx % dr::size_v<Spec>] += lerp_factor[idx] * sky_rad * sun_rad * (is_rgb_v<Spec> ? ScalarFloat(MI_CIE_Y_NORMALIZATION) : 1.f);
        }

        return depolarizer<Spectrum>(res) & active;
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                      const Point2f &sample2,
                                      const Point2f &sample3,
                                      Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);
        DRJIT_MARK_USED(time);
        DRJIT_MARK_USED(wavelength_sample);
        DRJIT_MARK_USED(sample2);
        DRJIT_MARK_USED(sample3);
        NotImplementedError()
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample,
                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);
        DRJIT_MARK_USED(it);
        DRJIT_MARK_USED(sample);
        NotImplementedError()
    }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &ds,
                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        DRJIT_MARK_USED(ds);
        NotImplementedError()
    }

    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f &si, Float sample,
                       Mask active) const override {
        DRJIT_MARK_USED(si);
        DRJIT_MARK_USED(sample);
        DRJIT_MARK_USED(active);
        NotImplementedError()
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SunskyEmitter[" << std::endl
            << "  bsphere = " << string::indent(m_bsphere) << std::endl
            << "  turbidity = " << string::indent(m_turbidity) << std::endl
            << "  sky_scale = " << string::indent(m_sky_scale) << std::endl
            << "  sun_scale = " << string::indent(m_sun_scale) << std::endl
            << "  albedo = " << string::indent(m_albedo) << std::endl
            << "  sun aperture (°) = " << string::indent(dr::rad_to_deg(2 * m_sun_half_aperture)) << std::endl;
        oss << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(TimedSunskyEmitter)

private:

    struct Datasets {
        Vector3f sun_dir;
        ArrayXf sky_rad;
        ArrayXf sky_params;

        std::string to_string() const {
            std::ostringstream oss;
            oss << "Datasets[" << std::endl
                << "  sun_dir = " << string::indent(sun_dir) << std::endl
                << "  sky_rad = " << string::indent(sky_rad) << std::endl
                << "  sky_params = " << string::indent(sky_params) << std::endl
                << "]" << std::endl;
            return oss.str();
        }

        DRJIT_STRUCT(Datasets, sun_dir, sky_rad, sky_params)
    };


    /**
     * Computes the sky datasets associated with a given time
     *
     * @param time Time value in [0, 1]
     * @return The ``Datasets`` instance containing the sky parameters, radiance
     * and associated direction
     */
    Datasets compute_dataset(const Float& time) const {
        DateTimeRecord<Float> date_time = dr::zeros<DateTimeRecord<Float>>();
        date_time.year = m_start_date.year;
        date_time.month = m_start_date.month;

        Float day = time * DateTimeRecord<Float>::get_days_between(m_start_date, m_end_date, m_location);

        date_time.day = dr::floor2int<Int32>(day);
        date_time.hour = m_window_start_time + (m_window_end_time - m_window_start_time) * dr::fmod(day, 1.f);

        const auto [sun_elevation, sun_azimuth] = Base::sun_coordinates(date_time, m_location);
        const Float sun_eta = 0.5f * dr::Pi<Float> - sun_elevation;

        return Datasets {
            sph_to_dir(sun_elevation, sun_azimuth),
            Base::template bezier_interp<ArrayXf>(m_sky_radiance, sun_eta),
            Base::template bezier_interp<ArrayXf>(m_sky_params, sun_eta)
        };
    }


    // ================================================================================================
    // ========================================= ATTRIBUTES ===========================================
    // ================================================================================================

    Float m_window_start_time, m_window_end_time;
    DateTimeRecord<Float> m_start_date, m_end_date;

    // ========= Radiance parameters =========
    TensorXf m_sky_params;
    TensorXf m_sky_radiance;
    FloatStorage m_sun_radiance;

};

MI_EXPORT_PLUGIN(TimedSunskyEmitter)
NAMESPACE_END(mitsuba)
