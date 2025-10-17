/*
 * This file contains various helpers for the `sunsky` plugin.
 */

#pragma once

#include <drjit/sphere.h>
#include <drjit/tensor.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/tensor.h>

NAMESPACE_BEGIN(mitsuba)

/// Number of spectral channels in the skylight model
static const constexpr uint32_t WAVELENGTH_COUNT = 11;
/// Number of turbidity levels in the skylight model
static const constexpr uint32_t TURBITDITY_LVLS = 10;
/// Number of albedo levels in the skylight model
static const constexpr uint32_t ALBEDO_LVLS = 2;

/// Distance between wavelengths in the skylight model
static constexpr uint32_t WAVELENGTH_STEP = 40;
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
static const constexpr double SUN_HALF_APERTURE = dr::Pi<double> * (0.5 * 0.5358) / 180;
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
        oss << "LocationRecord["
            << "\n\t\tLatitude = " << latitude
            << "\n\t\tLongitude = " << longitude
            << "\n\t\tTimezone = " << timezone
        << "\n\t]";
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
        oss << "DateTimeRecord["
            << "\n\t\tYear = " << year
            << "\n\t\tMonth= " << month
            << "\n\t\tDay = " << day
            << "\n\t\tHour = " << hour
            << "\n\t\tMinute = " << minute
            << "\n\t\tSecond = " << second
        << "\n\t]";
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
    using Gaussian     = dr::Array<Float, TGMM_GAUSSIAN_PARAMS>;

    using USpec       = unpolarized_spectrum_t<Spectrum>;
    using USpecUInt32 = dr::uint32_array_t<USpec>;
    using USpecMask   = dr::mask_t<USpec>;

    using SkyRadData    = USpec;
    using SkyParamsData = dr::Array<SkyRadData, SKY_PARAMS>;
    using FullSpectrum  = unpolarized_spectrum_t<mitsuba::Spectrum<Float, WAVELENGTH_COUNT>>;

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

        m_complex_sun = props.get<bool>("complex_sun", false);

        m_albedo_tex = props.get_texture<Texture>("albedo", 0.3f);
        if (m_albedo_tex->is_spatially_varying())
            Log(Error, "Expected a non-spatially varying radiance spectra!");
        m_albedo = extract_albedo(m_albedo_tex);
        dr::make_opaque(m_albedo);


        const std::string dataset_type = is_rgb_v<Spectrum> ? "_rgb" : "_spec";
        const TensorFile datasets {
            file_resolver()->resolve(DATABASE_PATH + "sunsky_datasets.bin")
        };
        const TensorFile tgmm_dataset(
            file_resolver()->resolve(DATABASE_PATH + "tgmm_tables.bin")
        );
        const TensorFile irrad_dataset {
            file_resolver()->resolve(DATABASE_PATH + "sampling_data.bin")
        };

        m_sky_params_dataset = load_field<TensorXf64>(datasets, "sky_params" + dataset_type);
        m_sky_rad_dataset = load_field<TensorXf64>(datasets, "sky_rad" + dataset_type);
        m_sun_rad_dataset = load_field<TensorXf64>(datasets, "sun_rad" + dataset_type);

        m_tgmm_tables = load_field<TensorXf32>(tgmm_dataset, "tgmm_tables");

        m_sky_irrad_dataset = load_field<TensorXf32>(irrad_dataset, "sky_irradiance");
        m_sun_irrad_dataset = load_field<TensorXf32>(irrad_dataset, "sun_irradiance");

        // Only used in spectral mode since limb darkening is baked in the RGB dataset
        if constexpr (!is_rgb_v<Spectrum>) {
            m_sun_ld = load_field<TensorXf64>(datasets, "sun_ld_spec");
        }

        // Precompute sun dataset
        m_sun_radiance = dr::take_interp(m_sun_rad_dataset, m_turbidity - 1.f);

        /* Until `set_scene` is called, we have no information
        about the scene and default to the unit bounding sphere. */
        m_bsphere = BoundingSphere3f(ScalarPoint3f(0.f), 1.f);

        dr::eval(m_albedo_tex, m_albedo, m_sun_radiance, m_sky_rad_dataset,
                 m_sky_params_dataset, m_sun_ld, m_sun_rad_dataset,
                 m_sky_irrad_dataset, m_sun_irrad_dataset, m_tgmm_tables);

        m_flags = +EmitterFlags::Infinite | +EmitterFlags::SpatiallyVarying;
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("to_world", m_to_world, ParamFlags::NonDifferentiable);
        cb->put("turbidity", m_turbidity, ParamFlags::NonDifferentiable);
        cb->put("sky_scale", m_sky_scale, ParamFlags::NonDifferentiable);
        cb->put("sun_scale", m_sun_scale, ParamFlags::NonDifferentiable);
        cb->put("albedo",    m_albedo_tex,    ParamFlags::NonDifferentiable);
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

        dr::make_opaque(m_turbidity, m_albedo);

        // Update sun
        if (keys.empty() || string::contains(keys, "turbidity"))
            m_sun_radiance = dr::take_interp(m_sun_rad_dataset, m_turbidity - 1.f);

    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "\n\tTurbidity = " << m_turbidity
            << "\n\tAlbedo = " << m_albedo
            << "\n\tSun scale = " << m_sun_scale
            << "\n\tSky scale = " << m_sky_scale
            << "\n\tSun aperture = " << dr::rad_to_deg(2 * m_sun_half_aperture)
            << "\n\tComplex sun model = " << (m_complex_sun ? "true" : "false");
        return oss.str();
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

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        const Point2f sun_angles = get_sun_angles(si.time);
        Vector3f local_wo = dr::normalize(m_to_world.value().inverse() * -si.wi);

        return eval(local_wo, si.wavelengths, sun_angles, hit_sun(sun_angles, local_wo), active);
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                      const Point2f &sample2,
                                      const Point2f &sample3,
                                      Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);
        const Point2f sun_angles = get_sun_angles(time);
        active &= sun_angles.y() <= 0.5f * dr::Pi<Float>;

        const Float sky_sampling_w = get_sky_sampling_weight(sun_angles.y(), active);

        // 1. Sample spatial component
        Point2f offset = warp::square_to_uniform_disk_concentric(sample2);

        // 2. Sample directional component
        Mask pick_sky = sample3.x() < sky_sampling_w;
        Vector3f d = dr::select(
                pick_sky,
                sample_sky({sample3.x() / sky_sampling_w, sample3.y()}, sun_angles, active & pick_sky),
                sample_sun({(sample3.x() - sky_sampling_w) / (1 - sky_sampling_w), sample3.y()}, sun_angles)
        );
        active &= Frame3f::cos_theta(d) >= 0.f;
        // Unlike \ref sample_direction, ray goes from the envmap toward the scene
        Vector3f d_world = m_to_world.value() * (-d);

        Mask sun_hit = !pick_sky || hit_sun(sun_angles, d);

        // 3. PDF
        Float sky_pdf, sun_pdf;
        std::tie(sky_pdf, sun_pdf) = compute_pdfs(d, sun_angles, sun_hit, active);
        Float pdf = dr::lerp(sun_pdf, sky_pdf, sky_sampling_w);
        // Apply pdf of the origin offset
        pdf *= dr::InvPi<Float> * dr::rcp(dr::square(m_bsphere.radius));
        active &= pdf > 0.f;

        // 4. Sample spectrum
        Spectrum weight = 1.f;
        Wavelength wavelengths = Wavelength(0.f);
        if constexpr (is_spectral_v<Spectrum>)
            std::tie(wavelengths, weight) =
                this->sample_wlgth(wavelength_sample, active);

        weight *= eval(d, wavelengths, sun_angles, sun_hit, active);

        // 5. Compute ray origin
        Vector3f perpendicular_offset =
            Frame3f(d_world).to_world(Vector3f(offset.x(), offset.y(), 0));
        Point3f origin = m_bsphere.center +
                         (perpendicular_offset - d_world) * m_bsphere.radius;

        weight /= pdf;
        weight &= dr::isfinite(weight);
        return { Ray3f(origin, d_world, time, wavelengths), weight };
    }


    std::pair<Wavelength, Spectrum>
    sample_wavelengths(const SurfaceInteraction3f& si, Float wavelength_sample, Mask active) const override {
        const Point2f sun_angles = get_sun_angles(si.time);
        Vector3f local_wo = dr::normalize(m_to_world.value().inverse() * -si.wi);

        Spectrum weights = 1.f;
        Wavelength wavelengths = Wavelength(0.f);
        if constexpr (is_spectral_v<Spectrum>)
            std::tie(wavelengths, weights) =
                this->sample_wlgth(wavelength_sample, active);

        weights *= eval(local_wo, wavelengths, sun_angles, hit_sun(sun_angles, local_wo), active);

        return { wavelengths, weights };
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample,
                     Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleDirection, active);

        const Point2f sun_angles = get_sun_angles(it.time);
        active &= sun_angles.y() <= 0.5f * dr::Pi<Float>;

        const Float sky_sampling_w = get_sky_sampling_weight(sun_angles.y(), active);

        Mask pick_sky = sample.x() < sky_sampling_w;

        // Sample the sun or the sky
        Vector3f sample_dir = dr::select(
                pick_sky,
                sample_sky({sample.x() / sky_sampling_w, sample.y()}, sun_angles, active & pick_sky),
                sample_sun({(sample.x() - sky_sampling_w) / (1 - sky_sampling_w), sample.y()}, sun_angles)
        );
        active &= Frame3f::cos_theta(sample_dir) >= 0.f;

        // Automatically enlarge the bounding sphere when it does not contain the reference point
        Float radius = dr::maximum(m_bsphere.radius, dr::norm(it.p - m_bsphere.center)),
              dist   = 2.f * radius;

        Vector3f d = m_to_world.value() * sample_dir;
        DirectionSample3f ds = dr::zeros<DirectionSample3f>();
        ds.p = dr::fmadd(d, dist, it.p);
        ds.n = -d;
        ds.uv = sample;
        ds.time = it.time;
        ds.delta = false;
        ds.emitter = this;
        ds.d = d;
        ds.dist = dist;

        Float sun_hit = !pick_sky || hit_sun(sun_angles, sample_dir);

        Float sky_pdf, sun_pdf;
        std::tie(sky_pdf, sun_pdf) = compute_pdfs(sample_dir, sun_angles, sun_hit, active);
        ds.pdf = dr::lerp(sun_pdf, sky_pdf, sky_sampling_w);

        Spectrum res = eval(sample_dir, it.wavelengths, sun_angles, sun_hit, active) / ds.pdf;
                 res &= dr::isfinite(res);
        return { ds, res };
    }

    Float pdf_direction(const Interaction3f &, const DirectionSample3f &ds,
                    Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);
        const Point2f sun_angles = get_sun_angles(ds.time);
        const Float sky_sampling_w = get_sky_sampling_weight(sun_angles.y(), active);

        Vector3f local_dir = dr::normalize(m_to_world.value().inverse() * ds.d);
        Float sky_pdf, sun_pdf;
        std::tie(sky_pdf, sun_pdf) = compute_pdfs(local_dir, sun_angles, hit_sun(sun_angles, local_dir), active);

        Float combined_pdf = dr::lerp(sun_pdf, sky_pdf, sky_sampling_w);
        return dr::select(active, combined_pdf, 0.f);
    }

    Spectrum eval_direction(const Interaction3f &it,
                            const DirectionSample3f &ds,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.wavelengths = it.wavelengths;
        si.time = it.time;
        si.wi = -ds.d;

        return eval(si, active);
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

    /**
     * Getter for the sun angles at the given time
     * @param time Time value in [0, 1]
     * @return The sun azimuth and elevation angle
     */
    virtual Point2f get_sun_angles(const Float& time) const = 0;

    /**
     * Getter sky radiance datasets for the given wavelengths and sun angles
     * @param sun_theta The angle between the sun's direction and the z axis
     * @param channel_idx Indices of the queried channels
     * @param active Indicates which channels are valid indices
     * @return The sky mean radiance dataset and the sky parameters
     *         for the Wilkie-Hosek 2012 sky model
     */
    virtual std::pair<SkyRadData, SkyParamsData>
    get_sky_datasets(const Float& sun_theta, const USpecUInt32& channel_idx, const USpecMask& active) const = 0;

    /**
     * Getter fot the probability of sampling the sky for a given sun position
     * @param sun_theta The angle between the sun's direction and the z axis
     * @param active Indicates active lanes
     * @return The probability of sampling the sky over the sun
     */
    virtual Float get_sky_sampling_weight(const Float& sun_theta, const Mask& active) const = 0;

    /**
     * Getter for the sun's irradiance at a given sun elevation and wavelengths
     * @param sun_theta The angle between the sun's direction and the z axis
     * @param channel_idx The indices of the queried channels
     * @param active Indicates active lanes
     * @return The sun irradiance for the given conditions
     */
    virtual USpec get_sun_irradiance(const Float& sun_theta, const USpecUInt32& channel_idx, const USpecMask& active) const = 0;

    /**
     * Samples a gaussian from the Truncated Gaussian mixture
     * @param sample Sample in [0, 1]
     * @param sun_theta The angle between the sun's direction and the z axis
     * @param active Indicates active lanes
     * @return The index of the chosen gaussian and the sample to be reused
     */
    virtual std::pair<UInt32, Float> sample_reuse_tgmm(const Float& sample, const Float& sun_theta, const Mask &active) const = 0;

    /**
     * Samples wavelengths and evaluates the associated weights
     * @param sample Floating point value in [0, 1] to sample the wavelengths
     * @param active Indicates active lanes
     * @return The chosen wavelengths and their inverse pdf
     */
    virtual std::pair<Wavelength, Spectrum>
    sample_wlgth(const Float& sample, Mask active) const = 0;

    /**
     * This method should be used when the decision of hitting the sun was already made.
     * This avoids transforming coordinates to world space and making that decision a second
     * time since numerical precision can cause discrepencies between the two decisions.
     * @param local_wo Shading direction in local space
     * @param wavelengths Wavelengths to evaluate
     * @param sun_angles Sun azimuth and elevation angles
     * @param hit_sun Indicates if the sun was hit
     * @param active Indicates active lanes
     * @return The radiance for the given conditions
     */
    Spectrum eval(const Vector3f& local_wo, const Wavelength& wavelengths, const Point2f& sun_angles, Mask hit_sun, Mask active) const {
        const Vector3f sun_dir = sph_to_dir(sun_angles.y(), sun_angles.x());

        Float cos_theta = Frame3f::cos_theta(local_wo),
              gamma = dr::unit_angle(sun_dir, local_wo);

        active &= (cos_theta >= 0.f) &&
                    (Frame3f::cos_theta(sun_dir) >= 0.f);

        hit_sun &= active;

        USpec res = 0.f;
        if constexpr (!is_spectral_v<Spectrum>) {
            DRJIT_MARK_USED(wavelengths);
            USpecUInt32 idx = dr::zeros<USpecUInt32>();
            if constexpr (is_rgb_v<Spectrum>)
                idx = {0, 1, 2};
            else
                idx = USpecUInt32(0);

            if (m_sky_scale > 0.f) {
                const auto [ sky_rad, sky_params ] = get_sky_datasets(sun_angles.y(), idx, active);
                res = m_sky_scale * eval_sky(cos_theta, gamma, sky_params, sky_rad);
            }

            if (m_sun_scale > 0.f) {
                res += m_sun_scale * eval_sun(idx, sun_angles.y(), cos_theta, gamma, hit_sun) *
                       get_area_ratio(m_sun_half_aperture) * SPEC_TO_RGB_SUN_CONV;
            }

            res *= ScalarFloat(MI_CIE_Y_NORMALIZATION);

        } else {
            Wavelength normalized_wavelengths =
                (wavelengths - WAVELENGTHS<ScalarFloat>[0]) / WAVELENGTH_STEP;
            USpecMask valid_idx = (0.f <= normalized_wavelengths) &
                                 (normalized_wavelengths <= CHANNEL_COUNT - 1);

            USpecUInt32 query_idx_low = dr::floor2int<USpecUInt32>(normalized_wavelengths);
            USpecUInt32 query_idx_high = query_idx_low + 1;

            Wavelength lerp_factor = normalized_wavelengths - query_idx_low;

            if (m_sky_scale > 0.f) {
                const auto [ sky_rad_low, sky_params_low ] = get_sky_datasets(sun_angles.y(), query_idx_low, active & valid_idx);
                const auto [ sky_rad_high, sky_params_high ] = get_sky_datasets(sun_angles.y(), query_idx_high, active & valid_idx);

                // Linearly interpolate the sky's irradiance across the spectrum
                res = m_sky_scale * dr::lerp(
                    eval_sky(cos_theta, gamma, sky_params_low, sky_rad_low),
                    eval_sky(cos_theta, gamma, sky_params_high, sky_rad_high),
                    lerp_factor);
            }

            if (m_sun_scale > 0.f) {
                // Linearly interpolate the sun's irradiance across the spectrum
                USpec sun_rad_low = eval_sun(query_idx_low, sun_angles.y(), cos_theta, gamma, hit_sun & valid_idx);
                USpec sun_rad_high = eval_sun(query_idx_high, sun_angles.y(), cos_theta, gamma, hit_sun & valid_idx);
                USpec sun_rad = dr::lerp(sun_rad_low, sun_rad_high, lerp_factor);

                USpec sun_ld = 1.f;
                if (m_complex_sun)
                    sun_ld = compute_sun_ld(query_idx_low, query_idx_high, lerp_factor,
                                gamma, hit_sun & valid_idx);

                res += m_sun_scale * sun_rad * sun_ld * get_area_ratio(m_sun_half_aperture);
            }
        }

        return depolarizer<Spectrum>(res) & active;
    }

    /**
     * \brief Evaluate the sky model for the given channel indices and angles
     *
     * Based on the Hosek-Wilkie skylight model
     * https://cgg.mff.cuni.cz/projects/SkylightModelling/HosekWilkie_SkylightModel_SIGGRAPH2012_Preprint_lowres.pdf
     *
     * @param cos_theta
     *      Cosine of the angle between the z-axis (up) and the viewing direction
     * @param gamma
     *      Angle between the sun and the viewing direction
     * @param sky_params
     *      Sky parameters for the model
     * @param sky_radiance
     *      Sky radiance for the model
     * @return Indirect sun illumination
     */
    USpec eval_sky(const Float &cos_theta, const Float &gamma,
                  const SkyParamsData &sky_params, const SkyRadData &sky_radiance) const {

        Float cos_gamma = dr::cos(gamma),
              cos_gamma_sqr = dr::square(cos_gamma);

        USpec c1 = 1 + sky_params[0] * dr::exp(sky_params[1] / (cos_theta + 0.01f));
        USpec chi = (1 + cos_gamma_sqr) /
                    dr::pow(1 + dr::square(sky_params[8]) - 2 * sky_params[8] * cos_gamma, 1.5f);
        USpec c2 = sky_params[2] + sky_params[3] * dr::exp(sky_params[4] * gamma) +
                    sky_params[5] * cos_gamma_sqr + sky_params[6] * chi +
                    sky_params[7] * dr::safe_sqrt(cos_theta);

        return c1 * c2 * sky_radiance;
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
     * \param channel_idx
     *       Indices of the channels to render
     * \param sun_theta
     *       Elevation angle of the sun
     * \param cos_theta
     *       Cosine of the angle between the z-axis (up) and the viewing direction
     * \param gamma
     *       Angle between the sun and the viewing direction
     * \param active
     *       Mask for the active lanes and channel indices
     * \return Direct sun illumination
     */
    USpec eval_sun(const USpecUInt32 &channel_idx,
                   const Float &sun_theta,
                   const Float &cos_theta, const Float &gamma,
                   const USpecMask &active = true) const {


        if (!m_complex_sun)
            // Get mean radiance by dividing irradiance by the solid angle of the sun
            return get_sun_irradiance(sun_theta, channel_idx, active)
                        / (dr::TwoPi<Float> * (1.f - dr::cos(ScalarFloat(SUN_HALF_APERTURE))));

        // Angles computation
        Float elevation = 0.5f * dr::Pi<Float> - dr::acos(cos_theta);

        // Find the segment of the piecewise function we are in
        UInt32 pos = dr::floor2int<UInt32>(
            dr::cbrt(2 * elevation * dr::InvPi<Float>) * SUN_SEGMENTS);
        pos = dr::minimum(pos, SUN_SEGMENTS - 1);

        Float break_x =
            0.5f * dr::Pi<Float> * dr::pow((Float) pos / SUN_SEGMENTS, 3.f);
        Float x = elevation - break_x;

        USpec solar_radiance = 0.f;
        if constexpr (!is_rgb_v<Spectrum>) {
            DRJIT_MARK_USED(gamma);
            // Compute sun radiance
            USpecUInt32 global_idx = pos * WAVELENGTH_COUNT * SUN_CTRL_PTS +
                                    channel_idx * SUN_CTRL_PTS;
            for (uint8_t k = 0; k < SUN_CTRL_PTS; ++k)
                solar_radiance +=
                    dr::pow(x, k) *
                    dr::gather<USpec>(m_sun_radiance, global_idx + k, active);
        } else {
            // Reproduces the spectral computation for RGB, however, in this case,
            // limb darkening is baked into the dataset, hence the extra work

            Float cos_psi = sun_cos_psi(gamma);

            static constexpr uint8_t VEC_SIZE = 8;
            UInt32 global_idx = pos * 3 * SUN_CTRL_PTS * SUN_LD_PARAMS / VEC_SIZE;
            for (uint32_t packet_idx = 0; packet_idx < 9; ++packet_idx) {

                const auto data = dr::gather<dr::Array<Float, VEC_SIZE>>(
                    m_sun_radiance, global_idx + packet_idx, dr::any(active)
                );

                for (uint32_t data_idx = 0; data_idx < VEC_SIZE; ++data_idx) {
                    const uint32_t idx = packet_idx * VEC_SIZE + data_idx;
                    uint32_t j_idx = idx % SUN_LD_PARAMS,
                             k_idx = ((idx - j_idx) / SUN_LD_PARAMS) % SUN_CTRL_PTS,
                             c_idx = (idx - j_idx - SUN_LD_PARAMS * k_idx) / (SUN_LD_PARAMS * SUN_CTRL_PTS);

                    solar_radiance[c_idx] +=
                        dr::pow(x, k_idx) *
                        dr::pow(cos_psi, j_idx) *
                        data[data_idx];
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
     * \param active
     *      Indicates if the channel indices are valid and that the sun was hit
     * \return
     *      The spectral values of limb darkening to apply to the sun's
     *      radiance by multiplication
     */
    USpec compute_sun_ld(const USpecUInt32 &channel_idx_low,
                        const USpecUInt32 &channel_idx_high,
                        const wavelength_t<USpec> &lerp_f, const Float &gamma,
                        const USpecMask &active) const {
        using SpecLdArray = dr::Array<USpec, SUN_LD_PARAMS>;

        SpecLdArray sun_ld_low  = dr::gather<SpecLdArray>(m_sun_ld.array(), channel_idx_low, active),
                    sun_ld_high = dr::gather<SpecLdArray>(m_sun_ld.array(), channel_idx_high, active),
                    sun_ld_coefs = dr::lerp(sun_ld_low, sun_ld_high, lerp_f);

        Float cos_psi = sun_cos_psi(gamma);

        USpec sun_ld = 0.f;
        for (uint8_t j = 0; j < SUN_LD_PARAMS; ++j)
            sun_ld += dr::pow(cos_psi, j) * sun_ld_coefs[j];

        return sun_ld & active;
    }

    // ================================================================================================
    // ===================================== SAMPLING FUNCTIONS =======================================
    // ================================================================================================

    /**
     * \brief Getter for the 4 interpolation points on turbidity and elevation
     * of the Truncated Gaussian Mixtures
     * @param sun_theta The angle between the sun's direction and the z axis
     * @return The 4 interpolation weights and
     *          the 4 indices corresponding to the start of the TGMMs in memory
     */
    std::pair<Vector4f, Vector4u> get_tgmm_data(const Float& sun_theta) const {
        Float sun_eta = 0.5f * dr::Pi<Float> - sun_theta;
              sun_eta = dr::rad_to_deg(sun_eta);

        Float eta_idx_f = dr::clip((sun_eta - 2) / 3, 0, ELEVATION_CTRL_PTS - 1),
              t_idx_f   = dr::clip(m_turbidity - 2, 0, (TURBITDITY_LVLS - 1) - 1);

        UInt32 eta_idx_low = dr::floor2int<UInt32>(eta_idx_f),
               t_idx_low   = dr::floor2int<UInt32>(t_idx_f);

        UInt32 eta_idx_high = dr::minimum(eta_idx_low + 1, ELEVATION_CTRL_PTS - 1),
               t_idx_high   = dr::minimum(t_idx_low + 1, (TURBITDITY_LVLS - 1) - 1);

        Float eta_rem = eta_idx_f - eta_idx_low,
              t_rem = t_idx_f - t_idx_low;

        constexpr uint32_t t_block_size = GAUSSIAN_NB / (TURBITDITY_LVLS - 1),
                 result_size  = t_block_size / ELEVATION_CTRL_PTS;

        Vector4u indices = {
            t_idx_low * t_block_size + eta_idx_low * result_size,
            t_idx_low * t_block_size + eta_idx_high * result_size,
            t_idx_high * t_block_size + eta_idx_low * result_size,
            t_idx_high * t_block_size + eta_idx_high * result_size
        };

        Vector4f lerp_factors = {
            (1 - t_rem) * (1 - eta_rem),
            (1 - t_rem) * eta_rem,
            t_rem * (1 - eta_rem),
            t_rem * eta_rem
        };

        return std::make_pair(lerp_factors, indices);
    }

    /**
     * \brief Samples the sky from the truncated gaussian mixture with the given sample.
     *
     * Based on the Truncated Gaussian Mixture Model (TGMM) for sky dome by N. Vitsas and K. Vardis
     * https://diglib.eg.org/items/b3f1efca-1d13-44d0-ad60-741c4abe3d21
     *
     * \param sample Sample uniformly distributed in [0, 1]^2
     * \param sun_angles Azimuth and elevation angles of the sun
     * \param active Mask for the active lanes
     * \return The sampled direction in the sky
     */
    Vector3f sample_sky(Point2f sample, const Point2f& sun_angles, const Mask& active) const {
        // Sample a gaussian from the mixture
        const auto [ gaussian_idx, temp_sample ] = sample_reuse_tgmm(sample.x(), sun_angles.y(), active);

        // {mu_phi, mu_theta, sigma_phi, sigma_theta, weight}
        Gaussian gaussian = dr::gather<Gaussian>(m_tgmm_tables, gaussian_idx, active);

        ScalarPoint2f a = { 0.f,                    0.f },
                      b = { dr::TwoPi<ScalarFloat>, 0.5f * dr::Pi<ScalarFloat> };

        Point2f mu    = { gaussian[0], gaussian[1] },
                sigma = { gaussian[2], gaussian[3] };

        Point2f cdf_a = gaussian_cdf(mu, sigma, a),
                cdf_b = gaussian_cdf(mu, sigma, b);

        sample = dr::lerp(cdf_a, cdf_b, Point2f{temp_sample, sample.y()});
        // Clamp to erfinv's domain of definition
        sample = dr::clip(sample, dr::Epsilon<Float>, dr::OneMinusEpsilon<Float>);

        Point2f angles = dr::SqrtTwo<Float> * dr::erfinv(2 * sample - 1) * sigma + mu;

        // From fixed reference frame where sun_phi = pi/2 to local frame
        angles.x() += sun_angles.x() - 0.5f * dr::Pi<Float>;
        // Clamp theta to avoid negative z-axis values (FP errors)
        angles.y() = dr::minimum(angles.y(), 0.5f * dr::Pi<Float> - dr::Epsilon<Float>);

        return dr::sphdir(angles.y(), angles.x());
    }

    /**
     * \brief Computes the PDF from the TGMM for the given viewing angles
     *
     * \param angles Angles of the view direction in local coordinates (phi, theta)
     * \param sun_angles Azimuth and elevation angles of the sun
     * \param active Mask for the active lanes and valid rays
     * \return The PDF of the sky for the given angles with no sin(theta) factor
     */
    Float tgmm_pdf(Point2f angles, const Point2f& sun_angles, Mask active) const {
        const auto [ lerp_w, tgmm_idx ] = get_tgmm_data(sun_angles.y());

        // From local frame to reference frame where sun_phi = pi/2 and phi is in [0, 2pi]
        angles.x() -= sun_angles.x() - 0.5f * dr::Pi<Float>;
        angles.x() = dr::select(angles.x() < 0, angles.x() + dr::TwoPi<Float>, angles.x());
        angles.x() = dr::select(angles.x() > dr::TwoPi<Float>, angles.x() - dr::TwoPi<Float>, angles.x());

        // Bounds check for theta
        active &= (angles.y() >= 0.f) && (angles.y() <= 0.5f * dr::Pi<Float>);

        // Bounding points of the truncated gaussian mixture
        ScalarPoint2f a = { 0.f,                    0.f },
                      b = { dr::TwoPi<ScalarFloat>, 0.5f * dr::Pi<ScalarFloat> };

        // Evaluate gaussian mixture
        Float pdf = 0.f;
        for (uint32_t mixture_idx = 0; mixture_idx < 4; ++mixture_idx) {
            for (uint32_t gaussian_idx = 0; gaussian_idx < TGMM_COMPONENTS; ++gaussian_idx) {

                Gaussian gaussian = dr::gather<Gaussian>(
                    m_tgmm_tables,
                    gaussian_idx + tgmm_idx[mixture_idx],
                    active
                );

                // {mu_phi, mu_theta}, {sigma_phi, sigma_theta}
                Point2f mu    = { gaussian[0], gaussian[1] },
                        sigma = { gaussian[2], gaussian[3] };

                Point2f cdf_a = gaussian_cdf(mu, sigma, a),
                        cdf_b = gaussian_cdf(mu, sigma, b);

                Float volume = (cdf_b.x() - cdf_a.x()) *
                               (cdf_b.y() - cdf_a.y()) *
                               (sigma.x() * sigma.y());

                Point2f sample = (angles - mu) / sigma;
                Float gaussian_pdf = warp::square_to_std_normal_pdf(sample);

                pdf += lerp_w[mixture_idx] * gaussian[4] * gaussian_pdf / volume;

            }
        }

        return dr::select(active, pdf, 0.f);
    }


    /**
     * \brief Samples the sun from a uniform cone with the given sample
     *
     * \param sample Sample to generate the sun ray
     * \param sun_angles Azimuth and elevation angles of the sun
     * \return The sampled sun direction
     */
    Vector3f sample_sun(const Point2f& sample, const Point2f& sun_angles) const {
        return Frame3f(sph_to_dir(sun_angles.y(), sun_angles.x())).to_world(
            warp::square_to_uniform_cone<Float>(sample, dr::cos(m_sun_half_aperture))
        );
    }

    /**
     * Computes the PDFs of the sky and sun for the given local direction
     *
     * \param local_dir Local direction in the sky
     * \param sun_angles Azimuth and elevation angles of the sun
     * \param hit_sun Indicates if the sun's intersection should be tested
     * \param active Mask for the active lanes
     * \return The sky and sun PDFs
     */
    std::pair<Float, Float> compute_pdfs(const Vector3f &local_dir,
                                         const Point2f &sun_angles,
                                         const Mask &hit_sun,
                                         Mask active) const {
        // =========== SKY PDF ===========
        Float sin_theta = Frame3f::sin_theta(local_dir);
        active &= (Frame3f::cos_theta(local_dir) >= 0.f) && (sin_theta != 0.f);

        sin_theta = dr::maximum(sin_theta, dr::Epsilon<Float>);
        Point2f angles = dir_to_sph(local_dir);
        angles = { angles.y(), angles.x() }; // flip convention
        Float sky_pdf = tgmm_pdf(angles, sun_angles, active) / sin_theta;

        // =========== SUN PDF ===========
        Frame3f local_sun_frame =
            Frame3f(sph_to_dir(sun_angles.y(), sun_angles.x()));

        Float cosine_cutoff = dr::cos(m_sun_half_aperture);
        Float sun_pdf = warp::square_to_uniform_cone_pdf(
            local_sun_frame.to_local(local_dir), cosine_cutoff);
        sun_pdf = dr::select(hit_sun & active, sun_pdf, 0.f);

        return {sky_pdf, sun_pdf};
    }

    // ================================================================================================
    // ====================================== HELPER FUNCTIONS ========================================
    // ================================================================================================

    MI_INLINE Mask hit_sun(const Point2f& sun_angles, const Vector3f& local_wo) const {
        return hit_sun(sph_to_dir(sun_angles.y(), sun_angles.x()), local_wo);
    }

    MI_INLINE Mask hit_sun(const Vector3f& sun_dir, const Vector3f& local_wo) const {
        return dr::dot(sun_dir, local_wo) >= dr::cos(m_sun_half_aperture);
    }

    /**
     * Scaling factor to keep sun irradiance constant across aperture angles
     * @param custom_half_aperture Half aperture angle used
     * @return The appropriate scaling factor
     */
    MI_INLINE Float get_area_ratio(const Float &custom_half_aperture) const {
        return (1.f - dr::cos(ScalarFloat(SUN_HALF_APERTURE))) /
               (1.f - dr::cos(custom_half_aperture));
    }

    /**
     * Computes the gaussian cdf for the given parameters
     * @param mu Gaussian average
     * @param sigma Gaussian standard deviation
     * @param x Point to evaluate the CDF at
     * @return The cdf value of the given point
     */
    MI_INLINE Point2f gaussian_cdf(const Point2f &mu, const Point2f &sigma, const ScalarPoint2f &x) const {
        return 0.5f * (1 + dr::erf(dr::InvSqrtTwo<Float> * (x - mu) / sigma));
    }

    /**
     * \brief Interpolates the given tensor on turbidity then albedo
     * @param dataset The original dataset from file
     * @param albedo The albedo values for each channels
     * @param turbidity The turbidity value
     * @return The collapsed dataset interpolated linearly
     */
    TensorXf bilinear_interp(const TensorXf& dataset,
        const FloatStorage& albedo, const Float& turbidity) const {
        using UInt32Storage = DynamicBuffer<UInt32>;

        uint32_t bilinear_res_size = (uint32_t) (dataset.size() / (dataset.shape(0) * dataset.shape(1))),
                 nb_params = (uint32_t) (bilinear_res_size / (dataset.shape(2) * dataset.shape(3)));

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


   /**
    * \brief Computes the cosine of the angle made between the sun's radius and the viewing direction
    *
    * \param gamma
    *      Angle between the sun's center and the viewing direction
    * \return
    *      The cosine of the angle between the sun's radius and the viewing direction
    */
    MI_INLINE Float sun_cos_psi(const Float& gamma) const {
        const Float sol_rad_sin = dr::sin(m_sun_half_aperture),
                    ar2 = 1.f / (sol_rad_sin * sol_rad_sin),
                    sin_gamma = dr::sin(gamma),
                    sc2 = 1.f - ar2 * sin_gamma * sin_gamma;

        return dr::safe_sqrt(sc2);
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

    /**
     * Loads a tensor from the given tensor file
     * @tparam FileTensor Tensor and type stored in the file
     * @param tensor_file File wrapper
     * @param tensor_name Name of the tensor to extract
     * @return The queried tensor
     */
    template <typename FileTensor>
    TensorXf load_field(const TensorFile& tensor_file, const std::string_view tensor_name) const {
        FileTensor ft = tensor_file.field(tensor_name).to<FileTensor>();
        return TensorXf(std::move(ft.array()), std::move(ft.shape()));
    }

    /**
     * \brief Extract the albedo values for the required wavelengths/channels
     *
     * \param albedo_tex Texture to extract the albedo from
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

    // ================================================================================================
    // ========================================= ATTRIBUTES ===========================================
    // ================================================================================================

    /// Number of channels used in the skylight model
    static constexpr uint32_t CHANNEL_COUNT =
        is_spectral_v<Spectrum> ? WAVELENGTH_COUNT : 3;
    static constexpr uint32_t TGMM_DATA_SIZE =
        (TURBITDITY_LVLS - 1) * ELEVATION_CTRL_PTS * TGMM_COMPONENTS *
        TGMM_GAUSSIAN_PARAMS;
    static constexpr uint32_t GAUSSIAN_NB =
        (TURBITDITY_LVLS - 1) * ELEVATION_CTRL_PTS * TGMM_COMPONENTS;

    ScalarBool m_complex_sun;
    BoundingSphere3f m_bsphere;

    // ========= Common parameters =========
    Float m_turbidity;
    ScalarFloat m_sky_scale;
    ScalarFloat m_sun_scale;

    ref<Texture> m_albedo_tex;
    FloatStorage m_albedo;

    ScalarFloat m_sun_half_aperture;

    // Precomputed dataset
    FloatStorage m_sun_radiance;

    // Datasets from file
    TensorXf m_sky_rad_dataset;
    TensorXf m_sky_params_dataset;
    TensorXf m_sun_ld; // Not initialized in RGB mode
    TensorXf m_sun_rad_dataset;

    // Contains irradiance values for the 10 turbidites,
    // 30 elevations and 11 wavelengths
    TensorXf m_sky_irrad_dataset;
    TensorXf m_sun_irrad_dataset;

    FloatStorage m_tgmm_tables;
};

#undef SUN_HALF_APERTURE

NAMESPACE_END(mitsuba)
