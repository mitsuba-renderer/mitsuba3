/*
 * This file contains various helpers for the `sunsky` plugin.
 */

#pragma once

#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/spectrum.h>
#include <drjit/sphere.h>


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
static const constexpr size_t SKY_CTRL_PTS = 6;
/// Number of parameters for the skylight model equation
static const constexpr size_t SKY_PARAMS = 9;

/// Number of control points for interpolation in the sun model
static const constexpr size_t SUN_CTRL_PTS = 4;
/// Number of segments for the piecewise polynomial in the sun model
static const constexpr size_t SUN_SEGMENTS = 45;
/// Number of coefficients for the sun's limb darkening
static const constexpr size_t SUN_LD_PARAMS = 6;

/// Number of elevation control points for the tgmm sampling tables
static const constexpr size_t ELEVATION_CTRL_PTS = 30;
/// Number of gaussian components in the tgmm
static const constexpr size_t TGMM_COMPONENTS = 5;
/// Number of parameters for each gaussian component
static const constexpr size_t TGMM_GAUSSIAN_PARAMS = 5;

/// Sun half aperture angle in radians
#define SUN_HALF_APERTURE (dr::deg_to_rad(0.5358/2.0))
/// Mean radius of the Earth
static const constexpr double EARTH_MEAN_RADIUS = 6371.01;   // In km
/// Astronomical unit
static const constexpr size_t ASTRONOMICAL_UNIT = 149597890; // In km

/// Conversion constant to convert spectral solar luminosity to RGB
static const constexpr float SPEC_TO_RGB_SUN_CONV = 467.069280386;

#define DATABASE_PATH "data/sunsky/"

enum class Dataset {
    SkyParams, SkyRadiance,
    SunRadiance, SunLimbDarkening,
    TGMMTables
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

/**
 * \brief Extracts the relative path to the given dataset
 *
 * \tparam IsRGB
 *      Indicates if the dataset is RGB or spectral
 * \param dataset
 *      Dataset to retrieve the path for
 * \return
 *      The path to the dataset
 */
template<bool IsRGB>
std::string path_to_dataset(const Dataset dataset) {
    const std::string type_str = IsRGB ? "_rgb_" : "_spec_";
    switch (dataset) {
        case Dataset::SkyParams:
            return DATABASE_PATH "sky" + type_str + "params.bin";
        case Dataset::SkyRadiance:
            return DATABASE_PATH "sky" + type_str + "rad.bin";
        case Dataset::SunRadiance:
            return DATABASE_PATH "sun" + type_str + "rad.bin";
        case Dataset::SunLimbDarkening:
            return DATABASE_PATH "sun_spec_ld.bin";
        case Dataset::TGMMTables:
            return DATABASE_PATH "tgmm_tables.bin";
        default:
            return "Unknown dataset";
    }
}

// ================================================================================================
// ========================================== SKY MODEL ===========================================
// ================================================================================================

/**
 * \brief Interpolates the dataset along a quintic bezier curve
 *
 * \param dataset
 *      Dataset to interpolate using a degree 5 bezier curve
 * \param out_size
 *      Size of the data to interpolate
 * \param offset
 *      Offset to apply to the indices
 * \param x
 *      Interpolation factor
 * \param active
 *      Indicates if the offsets are valid
 * \return
 *      Interpolated data
 */
template <typename Float>
DynamicBuffer<Float> bezier_interpolate(
    const DynamicBuffer<Float>& dataset, const uint32_t out_size,
    const dr::uint32_array_t<Float>& offset,
    const Float& x, const dr::mask_t<Float>& active) {

    using ScalarFloat = dr::value_t<Float>;
    using FloatStorage = DynamicBuffer<Float>;
    using UInt32Storage = DynamicBuffer<dr::uint32_array_t<Float>>;

    UInt32Storage indices = offset + dr::arange<UInt32Storage>(out_size);
    constexpr ScalarFloat coefs[SKY_CTRL_PTS] = {1, 5, 10, 10, 5, 1};

    FloatStorage res = dr::zeros<FloatStorage>(out_size);
    for (uint8_t ctrl_pt = 0; ctrl_pt < SKY_CTRL_PTS; ++ctrl_pt) {
        UInt32Storage idx = indices + ctrl_pt * out_size;
        FloatStorage data = dr::gather<FloatStorage>(dataset, idx, active);
        res += coefs[ctrl_pt] * dr::pow(x, ctrl_pt) *
               dr::pow(1 - x, (SKY_CTRL_PTS - 1) - ctrl_pt) * data;
    }

    return res;
}

/**
 * \brief Pre-computes the sky dataset using turbidity, albedo and sun
 * elevation
 *
 * \tparam dataset_size
 *      Size of the dataset
 * \param dataset
 *      Dataset to interpolate
 * \param albedo
 *      Albedo values corresponding to each channel
 * \param turbidity
 *      Turbidity used for the skylight model
 * \param eta
 *      Sun elevation angle
 * \return
 *      The interpolated dataset
 */
template <uint32_t dataset_size, typename Float>
DynamicBuffer<Float> sky_radiance_params(
    const DynamicBuffer<Float>& dataset,
    const DynamicBuffer<Float>& albedo, Float turbidity, Float eta) {

    using UInt32 = dr::uint32_array_t<Float>;
    using UInt32Storage = DynamicBuffer<UInt32>;
    using FloatStorage = DynamicBuffer<Float>;


    Float x = dr::cbrt(2 * dr::InvPi<Float> * eta);

    UInt32 t_high = dr::floor2int<UInt32>(turbidity),
           t_low = t_high - 1;

    Float t_rem = turbidity - t_high;

    // Compute block sizes for each parameter to facilitate indexing
    const uint32_t t_block_size = dataset_size / TURBITDITY_LVLS,
                   a_block_size = t_block_size / ALBEDO_LVLS,
                   result_size  = a_block_size / SKY_CTRL_PTS,
                   nb_params    = result_size / albedo.size();
                                             // albedo.size() <==> NB_CHANNELS

    // Interpolate on elevation
    FloatStorage
        t_low_a_low   = bezier_interpolate(dataset, result_size, t_low  * t_block_size + 0 * a_block_size, x, t_low < TURBITDITY_LVLS),
        t_high_a_low  = bezier_interpolate(dataset, result_size, t_high * t_block_size + 0 * a_block_size, x, t_high < TURBITDITY_LVLS),
        t_low_a_high  = bezier_interpolate(dataset, result_size, t_low  * t_block_size + 1 * a_block_size, x, t_low < TURBITDITY_LVLS),
        t_high_a_high = bezier_interpolate(dataset, result_size, t_high * t_block_size + 1 * a_block_size, x, t_high < TURBITDITY_LVLS);

    // Interpolate on turbidity
    FloatStorage res_a_low  = dr::lerp(t_low_a_low, t_high_a_low, t_rem),
                 res_a_high = dr::lerp(t_low_a_high, t_high_a_high, t_rem);

    // Interpolate on albedo
    UInt32Storage idx = dr::arange<UInt32Storage>(nb_params * albedo.size());
                  idx /= nb_params;

    FloatStorage result = dr::lerp(res_a_low, res_a_high, dr::gather<FloatStorage>(albedo, idx));
    return result & (0.f <= eta && eta <= 0.5f * dr::Pi<Float>);
}


// ================================================================================================
// ========================================== SUN MODEL ===========================================
// ================================================================================================

template<typename Float>
struct DateTimeRecord {
    using Int32 = dr::int32_array_t<Float>;
    Int32 year;
    Int32 month;
    Int32 day;
    Float hour;
    Float minute;
    Float second;

    std::string to_string() const {
        std::ostringstream oss;
        oss << "DateTimeRecord[year = " << year
            << ", month= " << month
            << ", day = " << day
            << ", hour = " << hour
            << ", minute = " << minute
            << ", second = " << second << "]";
        return oss.str();
    }
};

template<typename Float>
struct LocationRecord {
    Float longitude;
    Float latitude;
    Float timezone;

    std::string to_string() const {
        std::ostringstream oss;
        oss << "LocationRecord[latitude = " << latitude
            << ", longitude = " << longitude
            << ", timezone = " << timezone << "]";
        return oss.str();
    }
};

/**
 * \brief Compute the elevation and azimuth of the sun as seen by an observer
 * at \c location at the date and time specified in \c dateTime.
 *
 * Based on "Computing the Solar Vector" by Manuel Blanco-Muriel,
 * Diego C. Alarcon-Padilla, Teodoro Lopez-Moratalla, and Martin Lara-Coira,
 * in "Solar energy", vol 27, number 5, 2001 by Pergamon Press.
 */
template <typename Float>
Vector<Float, 3> sun_coordinates(const DateTimeRecord<Float> &date_time,
                                 const LocationRecord<Float> &location) {
    using Int32 = dr::int32_array_t<Float>;

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

    return sph_to_dir(elevation, azimuth - dr::Pi<Float>);
}

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
 * \brief Collects and linearly interpolates the sun radiance dataset along
 * turbidity
 *
 * \tparam dataset_size
 *      Size of the dataset
 * \param sun_radiance_dataset
 *      Dataset to interpolate
 * \param turbidity
 *      Turbidity used for the skylight model
 * \return  
 *      The interpolated dataset
 */
template <uint32_t dataset_size, typename Float>
DynamicBuffer<Float> sun_params(const DynamicBuffer<Float>& sun_radiance_dataset, Float turbidity) {
    using UInt32 = dr::uint32_array_t<Float>;
    using UInt32Storage = DynamicBuffer<dr::uint32_array_t<Float>>;
    using FloatStorage = DynamicBuffer<Float>;

    UInt32 t_high = dr::floor2int<UInt32>(turbidity),
           t_low = t_high - 1;
    Float  t_rem = turbidity - t_high;

    uint32_t t_block_size = dataset_size / TURBITDITY_LVLS;

    UInt32Storage idx = dr::arange<UInt32Storage>(t_block_size);
    return dr::lerp(
        dr::gather<FloatStorage>(sun_radiance_dataset, t_low * t_block_size + idx, t_low < TURBITDITY_LVLS),
        dr::gather<FloatStorage>(sun_radiance_dataset, t_high * t_block_size + idx, t_high < TURBITDITY_LVLS),
        t_rem
    );
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

// ================================================================================================
// =========================================== FILE I/O ===========================================
// ================================================================================================

/**
 * \brief Extracts an array from a compatible file
 *
 * \tparam FileType
 *      Type of the data stored in the file
 * \tparam OutType
 *      The type of the data to return
 * \param path
 *      Path of the data file
 * \return
 *      The extracted array
 */
template <typename FileType, typename OutType>
DynamicBuffer<OutType> sunsky_array_from_file(const std::string &path) {
    auto fs = Thread::thread()->file_resolver();
    fs::path file_path = fs->resolve(path);
    if (!fs::exists(file_path))
        Log(Error, "\"%s\": file does not exist!", file_path);

    FileStream file(file_path, FileStream::EMode::ERead);

    using ScalarFileType = dr::value_t<FileType>;
    using FileStorage = DynamicBuffer<FileType>;
    using FloatStorage = DynamicBuffer<OutType>;

    // =============== Read headers ===============
    char text_buff[5] = "";
    file.read(text_buff, 3);
    if (strcmp(text_buff, "SKY") != 0 && strcmp(text_buff, "SUN") != 0)
        Throw("File \"%s\" does not contain the expected header!", path);

    // Read version
    uint32_t version;
    file.read(version);

    // =============== Read tensor dimensions ===============
    size_t nb_dims = 0;
    file.read(nb_dims);

    size_t nb_elements = 1;
    std::vector<size_t> shape(nb_dims);
    for (size_t dim = 0; dim < nb_dims; ++dim) {
        file.read(shape[dim]);

        if (!shape[dim])
            Throw("Got dimension with 0 elements");

        nb_elements *= shape[dim];
    }

    // ==================== Read data ====================
    std::vector<ScalarFileType> buffer(nb_elements);
    file.read_array(buffer.data(), nb_elements);
    file.close();

    FileStorage data_f = dr::load<FileStorage>(buffer.data(), nb_elements);

    return FloatStorage(data_f);
}

#undef DATABASE_PATH
#undef SUN_HALF_APERTURE

NAMESPACE_END(mitsuba)
