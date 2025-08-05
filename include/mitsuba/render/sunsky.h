/*
 * This file contains various helpers for the `sunsky` plugin.
 */

#pragma once

#include "drjit/util.h"
#include <drjit/sphere.h>
#include <drjit/tensor.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/tensor.h>

NAMESPACE_BEGIN(mitsuba)

/// Number of spectral channels in the skylight model
static const constexpr size_t WAVELENGTH_COUNT = 11;
/// Number of turbidity levels in the skylight model
static const constexpr size_t TURBITDITY_LVLS = 10;
/// Number of albedo levels in the skylight model
static const constexpr size_t ALBEDO_LVLS = 2;

/// Distance between wavelengths in the skylight model
static constexpr size_t WAVELENGTH_STEP = 40;
/// Wavelengths used in the skylight model
template <typename Float>
static constexpr Float WAVELENGTHS[WAVELENGTH_COUNT] = {
    320, 360, 400, 440, 480, 520, 560, 600, 640, 680, 720
};

/// Number of control points for interpolation in the skylight model
static const constexpr uint32_t SKY_CTRL_PTS = 6;
/// Number of parameters for the skylight model equation
static const constexpr uint32_t SKY_PARAMS = 9;

/// Number of control points for interpolation in the sun model
static const constexpr uint32_t SUN_CTRL_PTS = 4;
/// Number of segments for the piecewise polynomial in the sun model
static const constexpr uint32_t SUN_SEGMENTS = 45;
/// Number of coefficients for the sun's limb darkening
static const constexpr uint32_t SUN_LD_PARAMS = 6;

/// Number of elevation control points for the tgmm sampling tables
static const constexpr uint32_t ELEVATION_CTRL_PTS = 30;
/// Number of gaussian components in the tgmm
static const constexpr uint32_t TGMM_COMPONENTS = 5;
/// Number of parameters for each gaussian component
static const constexpr uint32_t TGMM_GAUSSIAN_PARAMS = 5;

/// Sun half aperture angle in radians
#define SUN_HALF_APERTURE (dr::deg_to_rad(0.5358/2.0))
/// Mean radius of the Earth
static const constexpr double EARTH_MEAN_RADIUS = 6371.01;   // In km
/// Astronomical unit
static const constexpr double ASTRONOMICAL_UNIT = 149597890; // In km

/// Conversion constant to convert spectral solar luminosity to RGB
static const constexpr float SPEC_TO_RGB_SUN_CONV = 467.069280386f;

static const std::string DATABASE_PATH = "data/sunsky/";

template<typename Float>
struct LocationRecord {
    using ScalarFloat = dr::scalar_t<Float>;

    Float longitude = 139.6917f;
    Float latitude = 35.6894f;
    Float timezone = 9.f;

    explicit LocationRecord(const Properties& props, const std::string& prefix = ""):
        longitude(props.get<ScalarFloat>(prefix + "longitude", 139.6917f)),
        latitude(props.get<ScalarFloat>(prefix + "latitude", 35.6894f)),
        timezone(props.get<ScalarFloat>(prefix + "timezone", 9)) {}

    template<typename OtherFloat>
    LocationRecord& operator=(const LocationRecord<OtherFloat>& other) {
        longitude = other.longitude;
        latitude = other.latitude;
        timezone = other.timezone;
        return *this;
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "LocationRecord[latitude = " << latitude
            << ", longitude = " << longitude
            << ", timezone = " << timezone << "]";
        return oss.str();
    }

    DRJIT_STRUCT(LocationRecord, longitude, latitude, timezone)
};


template<typename Float>
struct DateTimeRecord {
    using Int32       = dr::int32_array_t<Float>;
    using ScalarInt32 = dr::scalar_t<Int32>;
    using ScalarFloat = dr::scalar_t<Float>;
    Int32 year, month, day;
    Float hour = 0.f, minute = 0.f, second = 0.f;

    /** Calculate difference in days between the current Julian Day
     *  and JD 2451545.0, which is noon 1 January 2000 Universal Time
     *
     *  \param timezone (Float) Timezone to use for conversion
     *  \return The elapsed time in julian days since New Year of 2000.
     */
    Float to_elapsed_julian_date(const Float& timezone) const {
        // Calculate time of the day in UT decimal hours
        Float dec_hours = hour - timezone + (minute + second / 60.f) / 60.f;

        // Calculate current Julian Day
        Int32 li_aux_1 = (month - 14) / 12;
        Int32 li_aux_2 = (1461 * (year + 4800 + li_aux_1)) / 4 +
                         (367 * (month - 2 - 12 * li_aux_1)) / 12 -
                         (3 * ((year + 4900 + li_aux_1) / 100)) / 4 + day -
                         32075;
        Float d_julian_date = li_aux_2 - 0.5f + dec_hours / 24.f;

        // Calculate difference between current Julian Day and JD 2451545.0
        return d_julian_date - 2451545.f;
    }

    explicit DateTimeRecord(const Properties& props, const std::string& prefix = ""):
        year(props.get<ScalarInt32>(prefix + "year", 2010)), month(props.get<ScalarInt32>(prefix + "month", 7)),
        day(props.get<ScalarInt32>(prefix + "day", 10)), hour(props.get<ScalarFloat>(prefix + "hour", 15.f)),
        minute(props.get<ScalarFloat>(prefix + "minute", 0.f)), second(props.get<ScalarFloat>(prefix + "second", 0.f)) {}


    static Int32 get_days_between(const DateTimeRecord& start, const DateTimeRecord& end, const LocationRecord<Float>& location) {
        Float elapsed_jd_start = start.to_elapsed_julian_date(location.timezone),
              elapsed_jd_end = end.to_elapsed_julian_date(location.timezone);

        if (dr::any(elapsed_jd_start > elapsed_jd_end))
            Throw("Start date is after end date");

        // Add one to count the end
        return dr::floor2int<Int32>(elapsed_jd_end - elapsed_jd_start);
    }

    template<typename OtherFloat>
    DateTimeRecord& operator=(const DateTimeRecord<OtherFloat>& other) {
        year = other.year;
        month = other.month;
        day = other.day;
        hour = other.hour;
        minute = other.minute;
        second = other.second;
        return *this;
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "DateTimeRecord[\n"
            << "year = " << year
            << ",\n month= " << month
            << ",\n day = " << day
            << ",\n hour = " << hour
            << ",\n minute = " << minute
            << ",\n second = " << second << "]";
        return oss.str();
    }

    DRJIT_STRUCT(DateTimeRecord, year, month, day, hour, minute, second)
};

MI_VARIANT
class BaseSunskyEmitter: public Emitter<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Emitter, m_flags, m_to_world)
    MI_IMPORT_TYPES(Scene, Texture)

    using FloatStorage = DynamicBuffer<Float>;

    BaseSunskyEmitter(const Properties &props) : Base(props) {
        if constexpr (!(is_rgb_v<Spectrum> || is_spectral_v<Spectrum>))
            Throw("Unsupported spectrum type, can only render in Spectral or RGB modes!");

        m_sun_scale = props.get<ScalarFloat>("sun_scale", 1.f);
        if (m_sun_scale < 0.f)
            Log(Error, "Invalid sun scale: %f, must be positive!", m_sun_scale);

        m_sky_scale = props.get<ScalarFloat>("sky_scale", 1.f);
        if (m_sky_scale < 0.f)
            Log(Error, "Invalid sky scale: %f, must be positive!", m_sky_scale);

        ScalarFloat turb = props.get<ScalarFloat>("turbidity", 3.f);
        if (turb < 1.f || 10.f < turb)
            Log(Error, "Turbidity value %f is out of range [1, 10]", turb);
        m_turbidity = turb;
        dr::make_opaque(m_turbidity);

        m_sun_half_aperture = dr::deg_to_rad(.5f * props.get<ScalarFloat>("sun_aperture", 0.5358f));
        if (m_sun_half_aperture <= 0.f || 0.5f * dr::Pi<Float> <= m_sun_half_aperture)
            Log(Error, "Invalid sun aperture angle: %f, must be in ]0, 90[ degrees!", dr::rad_to_deg(2 * m_sun_half_aperture));

        m_albedo_tex = props.get_texture<Texture>("albedo", 0.3f);
        if (m_albedo_tex->is_spatially_varying())
            Log(Error, "Expected a non-spatially varying radiance spectra!");
        m_albedo = extract_albedo(m_albedo_tex);


        const std::string dataset_type = is_rgb_v<Spectrum> ? "_rgb" : "_spec";
        const TensorFile datasets {
            file_resolver()->resolve(DATABASE_PATH + "sunsky_datasets.bin")
        };

        m_sky_params_dataset = load_field<TensorXf64>(datasets, "sky_params" + dataset_type);
        m_sky_rad_dataset = load_field<TensorXf64>(datasets, "sky_rad" + dataset_type);
        m_sun_rad_dataset = load_field<TensorXf64>(datasets, "sun_rad" + dataset_type);

        // Only used in spectral mode since limb darkening is baked in the RGB dataset
        if constexpr (is_spectral_v<Spectrum>) {
            m_sun_ld = load_field<TensorXf64>(datasets, "sun_ld_spec");
        }

        /* Until `set_scene` is called, we have no information
        about the scene and default to the unit bounding sphere. */
        m_bsphere = BoundingSphere3f(ScalarPoint3f(0.f), 1.f);

        m_flags = +EmitterFlags::Infinite | +EmitterFlags::SpatiallyVarying;
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("turbidity", m_turbidity, ParamFlags::NonDifferentiable);
        cb->put("sky_scale", m_sky_scale, ParamFlags::NonDifferentiable);
        cb->put("sun_scale", m_sun_scale, ParamFlags::NonDifferentiable);
        cb->put("albedo",    m_albedo_tex,    ParamFlags::NonDifferentiable);
        cb->put("to_world", m_to_world, ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (m_sun_scale < 0.f)
            Log(Error, "Invalid sun scale: %f, must be positive!", m_sun_scale);
        if (m_sky_scale < 0.f)
            Log(Error, "Invalid sky scale: %f, must be positive!", m_sky_scale);
        if (dr::any(m_turbidity < 1.f || 10.f < m_turbidity))
            Log(Error, "Turbidity value %f is out of range [1, 10]", m_turbidity);
        if (m_sun_half_aperture <= 0.f || 0.5f * dr::Pi<Float> <= m_sun_half_aperture)
            Log(Error, "Invalid sun aperture angle: %f, must be in ]0, 90[ degrees!",
                dr::rad_to_deg(2 * m_sun_half_aperture));
        if (m_albedo_tex->is_spatially_varying())
            Log(Error, "Expected a non-spatially varying radiance spectra!");

        if (keys.empty() || string::contains(keys, "albedo"))
            m_albedo = extract_albedo(m_albedo_tex);

    }

    void set_scene(const Scene *scene) override {
        if (scene->bbox().valid()) {
            ScalarBoundingSphere3f scene_sphere =
                scene->bbox().bounding_sphere();
            m_bsphere = BoundingSphere3f(scene_sphere.center, scene_sphere.radius);
            m_bsphere.radius =
                dr::maximum(math::RayEpsilon<Float>,
                        m_bsphere.radius * (1.f + math::RayEpsilon<Float>));
        } else {
            m_bsphere.center = 0.f;
            m_bsphere.radius = math::RayEpsilon<Float>;
        }

        dr::make_opaque(m_bsphere.center, m_bsphere.radius);
    }

    Spectrum eval_direction(const Interaction3f &it,
                            const DirectionSample3f &ds,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.wavelengths = it.wavelengths;
        si.time = it.time;
        si.wi = -ds.d;

        return this->eval(si, active);
    }

    std::pair<PositionSample3f, Float>
    sample_position(Float /*time*/, const Point2f & /*sample*/,
                    Mask /*active*/) const override {
        if constexpr (dr::is_jit_v<Float>) {
            /* Do not throw an exception in JIT-compiled variants. This
               function might be invoked by DrJit's virtual function call
               recording mechanism despite not influencing any actual
               calculation. */
            return { dr::zeros<PositionSample3f>(), dr::NaN<Float> };
        } else {
            NotImplementedError("sample_position");
        }
    }

    /// This emitter does not occupy any particular region of space, return an invalid bounding box
    ScalarBoundingBox3f bbox() const override {
        return ScalarBoundingBox3f();
    }

protected:

    template <typename Spec, typename Dataset>
    Spec eval_sky(const dr::uint32_array_t<Spec> &channel_idx, const Float &cos_theta, const Float &gamma,
               const Dataset &sky_params, const Dataset &sky_radiance, const dr::mask_t<Spec> &active = true) const {
        using SpecSkyParams = dr::Array<Spec, SKY_PARAMS>;

        using SpecSkyParams = dr::Array<Spec, SKY_PARAMS>;
        SpecSkyParams coefs = dr::gather<SpecSkyParams>(sky_params, channel_idx, active);

        Float cos_gamma = dr::cos(gamma),
                cos_gamma_sqr = dr::square(cos_gamma);

        Spec c1 = 1 + coefs[0] * dr::exp(coefs[1] / (cos_theta + 0.01f));
        Spec chi = (1 + cos_gamma_sqr) /
                    dr::pow(1 + dr::square(coefs[8]) - 2 * coefs[8] * cos_gamma, 1.5f);
        Spec c2 = coefs[2] + coefs[3] * dr::exp(coefs[4] * gamma) +
                    coefs[5] * cos_gamma_sqr + coefs[6] * chi +
                    coefs[7] * dr::safe_sqrt(cos_theta);

        return c1 * c2 * dr::gather<Spec>(sky_radiance, channel_idx, active);
    }

    /*
    template <typename Spec>
    Spec eval_sun(const dr::uint32_array_t<Spec> &channel_idx,
                   const Float &cos_theta, const Float &gamma,
                   const FloatStorage &sun_radiance,
                   const dr::mask_t<Spec> &active = true) const {
        using SpecUInt32 = dr::uint32_array_t<Spec>;
        using UInt32 = dr::uint32_array_t<Float>;

        // Angles computation
        Float elevation = 0.5f * dr::Pi<Float> - dr::acos(cos_theta);

        // Find the segment of the piecewise function we are in
        UInt32 pos = dr::floor2int<UInt32>(
            dr::cbrt(2 * elevation * dr::InvPi<Float>) * SUN_SEGMENTS);
        pos = dr::minimum(pos, SUN_SEGMENTS - 1);

        Float break_x =
            0.5f * dr::Pi<Float> * dr::pow((Float) pos / SUN_SEGMENTS, 3.f);
        Float x = elevation - break_x;

        Spec solar_radiance = 0.f;
        if constexpr (is_rgb_v<Spectrum>) {
            DRJIT_MARK_USED(gamma);
            // Compute sun radiance
            SpecUInt32 global_idx = pos * WAVELENGTH_COUNT * SUN_CTRL_PTS +
                                    channel_idx * SUN_CTRL_PTS;
            for (uint8_t k = 0; k < SUN_CTRL_PTS; ++k)
                solar_radiance +=
                    dr::pow(x, k) *
                    dr::gather<Spec>(sun_radiance, global_idx + k, active);
        } else if constexpr (is_spectral_v<Spectrum>) {
            // Reproduces the spectral computation for RGB, however, in this case,
            // limb darkening is baked into the dataset, hence the two for-loops
            Float cos_psi = sun_cos_psi(gamma);
            SpecUInt32 global_idx = pos * (3 * SUN_CTRL_PTS * SUN_LD_PARAMS) +
                                    channel_idx * (SUN_CTRL_PTS * SUN_LD_PARAMS);

            for (uint8_t k = 0; k < SUN_CTRL_PTS; ++k) {
                for (uint8_t j = 0; j < SUN_LD_PARAMS; ++j) {
                    SpecUInt32 idx = global_idx + k * SUN_LD_PARAMS + j;
                    solar_radiance +=
                        dr::pow(x, k) *
                        dr::pow(cos_psi, j) *
                        dr::gather<Spec>(sun_radiance, idx, active);
                }
            }
        }

        return solar_radiance & active;
    }

    template <typename Spec>
    Spec compute_sun_ld(const dr::uint32_array_t<Spec> &channel_idx_low,
                         const dr::uint32_array_t<Spec> &channel_idx_high,
                         const wavelength_t<Spec> &lerp_f, const Float &gamma,
                         const dr::mask_t<Spec> &active) const {
        using SpecLdArray = dr::Array<Spec, SUN_LD_PARAMS>;

        SpecLdArray sun_ld_low  = dr::gather<SpecLdArray>(m_sun_ld, channel_idx_low, active),
                    sun_ld_high = dr::gather<SpecLdArray>(m_sun_ld, channel_idx_high, active),
                    sun_ld_coefs = dr::lerp(sun_ld_low, sun_ld_high, lerp_f);

        Float cos_psi = sun_cos_psi(gamma);

        Spec sun_ld = 0.f;
        for (uint8_t j = 0; j < SUN_LD_PARAMS; ++j)
            sun_ld += dr::pow(cos_psi, j) * sun_ld_coefs[j];

        return sun_ld & active;
    }

    MI_INLINE Float sun_cos_psi(const Float& gamma) const {
        const Float sol_rad_sin = dr::sin(m_sun_half_aperture),
                    ar2 = 1.f / (sol_rad_sin * sol_rad_sin),
                    sin_gamma = dr::sin(gamma),
                    sc2 = 1.f - ar2 * sin_gamma * sin_gamma;

        return dr::safe_sqrt(sc2);
    }*/

    TensorXf bilinear_interp(const TensorXf& dataset,
        const FloatStorage& albedo, const Float& turbidity) const {

        using UInt32 = dr::uint32_array_t<Float>;
        using UInt32Storage = DynamicBuffer<UInt32>;

        uint32_t bilinear_res_size = dataset.size() / (dataset.shape(0) * dataset.shape(1)),
                 nb_params = bilinear_res_size / (dataset.shape(2) * dataset.shape(3));

        // Interpolate on turbidity
        TensorXf res = dr::take_interp(dataset, turbidity - 1.f);

        // Interpolate on albedo
        UInt32Storage rep_albedo_idx = dr::arange<UInt32Storage>(bilinear_res_size);
        FloatStorage rep_albedo = dr::gather<FloatStorage>(
            albedo,
            (rep_albedo_idx / nb_params) % (uint32_t) albedo.size()
        );

        return dr::take_interp(res, rep_albedo);
    }

    template<typename Dataset>
    Dataset bezier_interp(const TensorXf& dataset, const Float& eta) const {
        Dataset res = dr::zeros<Dataset>(dataset.size() / dataset.shape(0));

        Float x = dr::cbrt(2 * dr::InvPi<Float> * eta);
        constexpr dr::scalar_t<Float> coefs[SKY_CTRL_PTS] = {1, 5, 10, 10, 5, 1};

        Float x_pow = 1.f, x_pow_inv = dr::pow(1.f - x, SKY_CTRL_PTS - 1);
        Float x_pow_inv_scale = dr::rcp(1.f - x);
        for (uint32_t ctrl_pt = 0; ctrl_pt < SKY_CTRL_PTS; ++ctrl_pt) {
            res += dr::take(dataset, ctrl_pt) * coefs[ctrl_pt] * x_pow * x_pow_inv;

            x_pow *= x;
            x_pow_inv *= x_pow_inv_scale;
        }

        return res;
    }

    /**
     * \brief Compute the elevation and azimuth of the sun as seen by an observer
     * at \c location at the date and time specified in \c dateTime.
     *
     * \returns The pair containing the polar angle and the azimuth
     *
     * Based on "Computing the Solar Vector" by Manuel Blanco-Muriel,
     * Diego C. Alarcon-Padilla, Teodoro Lopez-Moratalla, and Martin Lara-Coira,
     * in "Solar energy", vol 27, number 5, 2001 by Pergamon Press.
     */
    std::pair<Float, Float> sun_coordinates(const DateTimeRecord<Float> &date_time,
                                            const LocationRecord<Float> &location) const {

        // Main variables
        Float elapsed_julian_days, dec_hours;
        Float ecliptic_longitude, ecliptic_obliquity;
        Float right_ascension, declination;
        Float elevation, azimuth;

        // Auxiliary variables
        Float d_y;
        Float d_x;

        /* Calculate difference in days between the current Julian Day
           and JD 2451545.0, which is noon 1 January 2000 Universal Time */
        {
            // Calculate time of the day in UT decimal hours
            dec_hours = date_time.hour - location.timezone +
                (date_time.minute + date_time.second / 60.f ) / 60.f;

            // Calculate current Julian Day
            Int32 li_aux_1 = (date_time.month-14) / 12;
            Int32 li_aux_2 = (1461*(date_time.year + 4800 + li_aux_1)) / 4
                + (367 * (date_time.month - 2 - 12 * li_aux_1)) / 12
                - (3 * ((date_time.year + 4900 + li_aux_1) / 100)) / 4
                + date_time.day - 32075;
            Float d_julian_date = li_aux_2 - 0.5f + dec_hours / 24.f;

            // Calculate difference between current Julian Day and JD 2451545.0
            elapsed_julian_days = d_julian_date - 2451545.f;
        }

        /* Calculate ecliptic coordinates (ecliptic longitude and obliquity of the
           ecliptic in radians but without limiting the angle to be less than 2*Pi
           (i.e., the result may be greater than 2*Pi) */
        {
            Float omega = 2.1429f - 0.0010394594f * elapsed_julian_days;
            Float mean_longitude = 4.8950630f + 0.017202791698f * elapsed_julian_days; // Radians
            Float anomaly = 6.2400600f + 0.0172019699f * elapsed_julian_days;

            ecliptic_longitude = mean_longitude + 0.03341607f * dr::sin(anomaly)
                + 0.00034894f * dr::sin(2*anomaly) - 0.0001134f
                - 0.0000203f * dr::sin(omega);

            ecliptic_obliquity = 0.4090928f - 6.2140e-9f * elapsed_julian_days
                + 0.0000396f * dr::cos(omega);
        }

        /* Calculate celestial coordinates ( right ascension and declination ) in radians
           but without limiting the angle to be less than 2*Pi (i.e., the result may be
           greater than 2*Pi) */
        {
            Float sin_ecliptic_longitude = dr::sin(ecliptic_longitude);
            d_y = dr::cos(ecliptic_obliquity) * sin_ecliptic_longitude;
            d_x = dr::cos(ecliptic_longitude);
            right_ascension = dr::atan2(d_y, d_x);
            right_ascension += dr::select(right_ascension < 0.f, dr::TwoPi<Float>, 0.f);

            declination = dr::asin(dr::sin(ecliptic_obliquity) * sin_ecliptic_longitude);
        }

        // Calculate local coordinates (azimuth and zenith angle) in degrees
        {
            Float greenwich_mean_sidereal_time = 6.6974243242f
                + 0.0657098283f * elapsed_julian_days + dec_hours;

            Float local_mean_sidereal_time = dr::deg_to_rad(greenwich_mean_sidereal_time * 15
                + location.longitude);

            Float latitude_in_radians = dr::deg_to_rad(location.latitude);
            Float cos_latitude = dr::cos(latitude_in_radians);
            Float sin_latitude = dr::sin(latitude_in_radians);

            Float hour_angle = local_mean_sidereal_time - right_ascension;
            Float cos_hour_angle = dr::cos(hour_angle);

            elevation = dr::acos(cos_latitude * cos_hour_angle
                * dr::cos(declination) + dr::sin(declination) * sin_latitude);

            d_y = -dr::sin(hour_angle);
            d_x = dr::tan(declination) * cos_latitude - sin_latitude * cos_hour_angle;

            azimuth = dr::atan2(d_y, d_x);
            azimuth += dr::select(azimuth < 0.f, dr::TwoPi<Float>, 0.f);

            // Parallax Correction
            elevation += Float(EARTH_MEAN_RADIUS / ASTRONOMICAL_UNIT) * dr::sin(elevation);
        }

        return std::make_pair(elevation, azimuth - dr::Pi<Float>);
    }

    template <typename FileTensor>
    TensorXf load_field(const TensorFile& tensor_file, const std::string_view tensor_name) const {

        const TensorFile::Field& field = tensor_file.field(tensor_name);
        if (unlikely(field.dtype != struct_type_v<FileTensor>))
            Throw("Incoherent type requested from field");

        FileTensor ft = field.to<FileTensor>();
        return TensorXf(ft);
    }

    /**
     * \brief Extract the albedo values for the required wavelengths/channels
     *
     * \param albedo_tex
     *      Texture to extract the albedo from
     * \return
     *      The buffer with the extracted albedo values for the needed wavelengths/channels
     */
    FloatStorage extract_albedo(const ref<Texture>& albedo_tex) const {
        FloatStorage albedo = dr::zeros<FloatStorage>(CHANNEL_COUNT);
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

        if constexpr (is_rgb_v<Spectrum>) {
            albedo = dr::ravel(albedo_tex->eval(si));

        } else if constexpr (dr::is_array_v<Float> && is_spectral_v<Spectrum>) {
            si.wavelengths = dr::load<FloatStorage>(WAVELENGTHS<ScalarFloat>, CHANNEL_COUNT);
            albedo = albedo_tex->eval(si)[0];
        } else if (!dr::is_array_v<Float> && is_spectral_v<Spectrum>) {
            for (ScalarUInt32 i = 0; i < CHANNEL_COUNT; ++i) {
                if constexpr (!is_monochromatic_v<Spectrum>)
                    si.wavelengths = WAVELENGTHS<ScalarFloat>[i];
                dr::scatter(albedo, albedo_tex->eval(si)[0], (UInt32) i);
            }
        }

        if (dr::any(albedo < 0.f || albedo > 1.f))
            Log(Error, "Albedo values must be in [0, 1], got: %f", albedo);

        return albedo;
    }

protected:
    /// Number of channels used in the skylight model
    static constexpr uint32_t CHANNEL_COUNT =
        is_spectral_v<Spectrum> ? WAVELENGTH_COUNT : 3;

    BoundingSphere3f m_bsphere;

    // ========= Common parameters =========
    Float m_turbidity;
    ScalarFloat m_sky_scale;
    ScalarFloat m_sun_scale;

    ref<Texture> m_albedo_tex;
    FloatStorage m_albedo;

    ScalarFloat m_sun_half_aperture;

    LocationRecord<Float> m_location;

    TensorXf m_sky_rad_dataset;
    TensorXf m_sky_params_dataset;
    FloatStorage m_sun_ld; // Not initialized in RGB mode
    TensorXf m_sun_rad_dataset;
};


// ================================================================================================
// ====================================== HELPER FUNCTIONS ========================================
// ================================================================================================

/**
 * \brief Provides the ratio of the sun's original area to that
 * of a custom aperture angle
 *
 * \param custom_half_aperture
 *      Angle of the sun's half aperture
 * \return
 *      The ratio of the sun's area to the custom aperture's area
 */
template <typename Value>
MI_INLINE Value get_area_ratio(const Value &custom_half_aperture) {
    return (1 - Value(dr::cos(SUN_HALF_APERTURE))) /
           (1 - dr::cos(custom_half_aperture));
}

/**
 * \brief Computes the Gaussian CDF for the given mean and standard
 * deviation
 *
 * \tparam Value
 *      Type to compute on
 * \param mu
 *      Mean of the gaussian
 * \param sigma
 *      Standard deviation of the gaussian
 * \param x
 *      Point to evaluate
 * \return
 *      The Gaussian cumulative distribution function at x
 */
template <typename Value>
MI_INLINE Value gaussian_cdf(const Value &mu, const Value &sigma,
                             const Value &x) {
    return 0.5f * (1 + dr::erf(dr::InvSqrtTwo<Value> * (x - mu) / sigma));
}


// ================================================================================================
// ==================================== RADIANCE COMPUTATIONS =====================================
// ================================================================================================


/**
 * \brief Computes the cosine of the angle made between the sun's radius and the viewing direction
 *
 * \tparam Value
 *      Type to compute on
 * \param gamma
 *      Angle between the sun's center and the viewing direction
 * \param sun_half_aperture
 *      Half aperture angle of the sun
 * \return
 *      The cosine of the angle between the sun's radius and the viewing direction
 */
template <typename Value>
MI_INLINE Value sun_cos_psi(const Value& gamma, const Value& sun_half_aperture) {
    const Value sol_rad_sin = dr::sin(sun_half_aperture),
                ar2 = 1.f / (sol_rad_sin * sol_rad_sin),
                sin_gamma = dr::sin(gamma),
                sc2 = 1.f - ar2 * sin_gamma * sin_gamma;

    return dr::safe_sqrt(sc2);
}

/**
* \brief Evaluates the sun model for the given channel indices and angles
*
* The template parameter is used to render the full 11 wavelengths at once
* in pre-computations
*
* Based on the Hosek-Wilkie sun model
* https://cgg.mff.cuni.cz/publications/adding-a-solar-radiance-function-to-the-hosek-wilkie-skylight-model/
*
* \tparam Spec
*       Spectral type to render (adapts the number of channels)
* \param channel_idx
*       Indices of the channels to render
* \param cos_theta
*       Cosine of the angle between the z-axis (up) and the viewing direction
* \param gamma
*       Angle between the sun and the viewing direction
* \param sun_radiance
*       Sun radiance dataset
* \param sun_half_aperture
        Sun half apperture angle
* \param active
*       Mask for the active lanes and channel indices
* \return
*       Sun radiance
*/

template <typename Spec, bool isRGB = is_spectral_v<Spec>,
          typename Float = dr::value_t<Spec>, typename Dataset = DynamicBuffer<Float>>
Spec eval_sun(const dr::uint32_array_t<Spec> &channel_idx,
               const Float &cos_theta, const Float &gamma,
               const Dataset &sun_radiance, const dr::scalar_t<Float> &sun_half_aperture,
               const dr::mask_t<Spec> &active = true) {
    using SpecUInt32 = dr::uint32_array_t<Spec>;
    using UInt32 = dr::uint32_array_t<Float>;

    // Angles computation
    Float elevation = 0.5f * dr::Pi<Float> - dr::acos(cos_theta);

    // Find the segment of the piecewise function we are in
    UInt32 pos = dr::floor2int<UInt32>(
        dr::cbrt(2 * elevation * dr::InvPi<Float>) * SUN_SEGMENTS);
    pos = dr::minimum(pos, SUN_SEGMENTS - 1);

    Float break_x =
        0.5f * dr::Pi<Float> * dr::pow((Float) pos / SUN_SEGMENTS, 3.f);
    Float x = elevation - break_x;

    Spec solar_radiance = 0.f;
    if constexpr (isRGB) {
        DRJIT_MARK_USED(gamma);
        // Compute sun radiance
        SpecUInt32 global_idx = pos * WAVELENGTH_COUNT * SUN_CTRL_PTS +
                                channel_idx * SUN_CTRL_PTS;
        for (uint8_t k = 0; k < SUN_CTRL_PTS; ++k)
            solar_radiance +=
                dr::pow(x, k) *
                dr::gather<Spec>(sun_radiance, global_idx + k, active);
    } else {
        // Reproduces the spectral computation for RGB, however, in this case,
        // limb darkening is baked into the dataset, hence the two for-loops
        Float cos_psi = sun_cos_psi<Float>(gamma, sun_half_aperture);
        SpecUInt32 global_idx = pos * (3 * SUN_CTRL_PTS * SUN_LD_PARAMS) +
                                channel_idx * (SUN_CTRL_PTS * SUN_LD_PARAMS);

        for (uint8_t k = 0; k < SUN_CTRL_PTS; ++k) {
            for (uint8_t j = 0; j < SUN_LD_PARAMS; ++j) {
                SpecUInt32 idx = global_idx + k * SUN_LD_PARAMS + j;
                solar_radiance +=
                    dr::pow(x, k) *
                    dr::pow(cos_psi, j) *
                    dr::gather<Spec>(sun_radiance, idx, active);
            }
        }
    }

    return dr::select(active, solar_radiance, 0.f);
}


/**
 * \brief Computes the limb darkening of the sun for a given gamma.
 *
 * Only works for spectral mode since limb darkening is baked into the RGB
 * model
 *
 * \tparam Spec
 *      Spectral type to render (adapts the number of channels)
 * \param channel_idx_low
 *      Indices of the lower wavelengths
 * \param channel_idx_high
 *      Indices of the upper wavelengths
 * \param lerp_f
 *      Linear interpolation factor for wavelength
 * \param gamma
 *      Angle between the sun's center and the viewing ray
 * \param sun_ld_data
 *      The sun's limb darkening dataset
 * \param sun_half_apperture
 *      Sun half apperture angle
 * \param active
 *      Indicates if the channel indices are valid and that the sun was hit
 * \return
 *      The spectral values of limb darkening to apply to the sun's
 *      radiance by multiplication
 */
template <typename Spec, typename Float, typename Dataset = DynamicBuffer<Float>>
Spec compute_sun_ld(const dr::uint32_array_t<Spec> &channel_idx_low,
                     const dr::uint32_array_t<Spec> &channel_idx_high,
                     const wavelength_t<Spec> &lerp_f, const Float &gamma,
                     const Dataset &sun_ld_data, const Float &sun_half_apperture,
                     const dr::mask_t<Spec> &active) {
    using SpecLdArray = dr::Array<Spec, SUN_LD_PARAMS>;

    SpecLdArray sun_ld_low  = dr::gather<SpecLdArray>(sun_ld_data, channel_idx_low, active),
                sun_ld_high = dr::gather<SpecLdArray>(sun_ld_data, channel_idx_high, active),
                sun_ld_coefs = dr::lerp(sun_ld_low, sun_ld_high, lerp_f);

    Float cos_psi = sun_cos_psi<Float>(gamma, sun_half_apperture);

    Spec sun_ld = 0.f;
    for (uint8_t j = 0; j < SUN_LD_PARAMS; ++j)
        sun_ld += dr::pow(cos_psi, j) * sun_ld_coefs[j];

    return sun_ld & active;
}



// ================================================================================================
// ======================================== SAMPLING MODEL ========================================
// ================================================================================================

/**
 * \brief Extracts the Gaussian Mixture Model parameters from the TGMM
 * dataset The 4 * (5 gaussians) cannot be interpolated directly, so we need
 * to combine them and adjust the weights based on the elevation and
 * turbidity linear interpolation parameters.
 *
 * \tparam dataset_size
 *      Size of the TGMM dataset
 * \param tgmm_tables
 *      Dataset for the Gaussian Mixture Models
 * \param turbidity
 *      Turbidity used for the skylight model
 * \param eta
 *      Elevation of the sun
 * \return
 *      The new distribution parameters and the mixture weights
 */
template <uint32_t dataset_size, typename Float>
std::pair<DynamicBuffer<Float>, DynamicBuffer<Float>>
build_tgmm_distribution(const DynamicBuffer<Float> &tgmm_tables,
                          Float turbidity, Float eta) {
    using UInt32        = dr::uint32_array_t<Float>;
    using ScalarUInt32  = dr::value_t<UInt32>;
    using UInt32Storage = DynamicBuffer<UInt32>;
    using FloatStorage  = DynamicBuffer<Float>;

    // ============== EXTRACT INDEXES AND LERP WEIGHTS ==============

    eta = dr::rad_to_deg(eta);
    Float eta_idx_f = dr::clip((eta - 2) / 3, 0, ELEVATION_CTRL_PTS - 1),
          t_idx_f = dr::clip(turbidity - 2, 0, (TURBITDITY_LVLS - 1) - 1);

    UInt32 eta_idx_low = dr::floor2int<UInt32>(eta_idx_f),
           t_idx_low = dr::floor2int<UInt32>(t_idx_f);

    UInt32 eta_idx_high = dr::minimum(eta_idx_low + 1, ELEVATION_CTRL_PTS - 1),
           t_idx_high = dr::minimum(t_idx_low + 1, (TURBITDITY_LVLS - 1) - 1);

    Float eta_rem = eta_idx_f - eta_idx_low,
          t_rem = t_idx_f - t_idx_low;

    uint32_t t_block_size = dataset_size / (TURBITDITY_LVLS - 1),
             result_size  = t_block_size / ELEVATION_CTRL_PTS;

    UInt32 indices[4] = {
        t_idx_low * t_block_size + eta_idx_low * result_size,
        t_idx_low * t_block_size + eta_idx_high * result_size,
        t_idx_high * t_block_size + eta_idx_low * result_size,
        t_idx_high * t_block_size + eta_idx_high * result_size
    };

    Float lerp_factors[4] = {
        (1 - t_rem) * (1 - eta_rem),
        (1 - t_rem) * eta_rem,
        t_rem * (1 - eta_rem),
        t_rem * eta_rem
    };

    // ==================== EXTRACT PARAMETERS AND APPLY LERP WEIGHT ====================
    FloatStorage distrib_params = dr::zeros<FloatStorage>(4 * result_size);
    UInt32Storage param_indices = dr::arange<UInt32Storage>(result_size);
    for (ScalarUInt32 mixture_idx = 0; mixture_idx < 4; ++mixture_idx) {

        // Gather gaussian weights and parameters
        FloatStorage params = dr::gather<FloatStorage>(tgmm_tables, indices[mixture_idx] + param_indices);

        // Apply lerp factor to gaussian weights
        for (ScalarUInt32 weight_idx = 0; weight_idx < TGMM_COMPONENTS; ++weight_idx) {
            ScalarUInt32 gaussian_weight_idx = weight_idx * TGMM_GAUSSIAN_PARAMS + (TGMM_GAUSSIAN_PARAMS - 1);
            dr::scatter(params, params[gaussian_weight_idx] * lerp_factors[mixture_idx], (UInt32) gaussian_weight_idx);
        }

        // Scatter back the parameters in the final gaussian mixture buffer
        dr::scatter(distrib_params, params, mixture_idx * result_size + param_indices);
    }

    // Extract gaussian weights
    UInt32Storage mis_weight_idx =
        TGMM_GAUSSIAN_PARAMS * dr::arange<UInt32Storage>(4 * TGMM_COMPONENTS) +
        (TGMM_GAUSSIAN_PARAMS - 1);
    FloatStorage mis_weights =
        dr::gather<FloatStorage>(distrib_params, mis_weight_idx);

    return std::make_pair(distrib_params, mis_weights);
}

#undef SUN_HALF_APERTURE

NAMESPACE_END(mitsuba)
