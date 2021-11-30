#include <mitsuba/render/fwd.h>
#include <mitsuba/mitsuba.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/spectrum.h>
#include <cmath>
#include <mitsuba/core/distr_1d.h>

#define EARTH_MEAN_RADIUS 6371.01   // In km
#define ASTRONOMICAL_UNIT 149597890 // In km


NAMESPACE_BEGIN(mitsuba)

struct DateTimeRecord {
    int year;
    int month;
    int day;
    double hour;
    double minute;
    double second;

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

struct LocationRecord {
    double longitude;
    double latitude;
    double timezone;

    std::string to_string() const {
        std::ostringstream oss;
        oss << "LocationRecord[latitude = " << latitude
            << ", longitude = " << longitude
            << ", timezone = " << timezone << "]";
        return oss.str();
    }
};

struct SphericalCoordinates {

    float elevation;
    float azimuth;

    inline SphericalCoordinates() { }

    inline SphericalCoordinates(float elevation, float azimuth)
        : elevation(elevation), azimuth(azimuth) { }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "SphericalCoordinates[elevation = " << ek::rad_to_deg(elevation)
            << ", azimuth = " << ek::rad_to_deg(azimuth) << "]";
        return oss.str();
    }
};


Vector<float, 3> to_sphere(const SphericalCoordinates coords) {
    auto [sinTheta, cosTheta] = ek::sincos(coords.elevation);
 	auto [sinPhi, cosPhi] = ek::sincos(coords.azimuth);

    return Vector<float, 3>(sinPhi*sinTheta, cosTheta, -cosPhi*sinTheta);
}

 
SphericalCoordinates from_sphere(const Vector<float, 3> &d) {
    float azimuth = std::atan2(d.x(), -d.z());
    float elevation = ek::safe_acos(d.y());
    if (azimuth < 0)
        azimuth += 2*M_PI;
    return SphericalCoordinates(elevation, azimuth);
}

/**
 * \brief Compute the elevation and azimuth of the sun as seen by an observer
 * at \c location at the date and time specified in \c dateTime.
 *
 * Based on "Computing the Solar Vector" by Manuel Blanco-Muriel,
 * Diego C. Alarcon-Padilla, Teodoro Lopez-Moratalla, and Martin Lara-Coira,
 * in "Solar energy", vol 27, number 5, 2001 by Pergamon Press.
 */
SphericalCoordinates compute_sun_coordinates(const DateTimeRecord &dateTime, const LocationRecord &location) {
    // Main variables
    double elapsedJulianDays, decHours;
    double eclipticLongitude, eclipticObliquity;
    double rightAscension, declination;
    double elevation, azimuth;

    // Auxiliary variables
    double dY;
    double dX;

    /* Calculate difference in days between the current Julian Day
       and JD 2451545.0, which is noon 1 January 2000 Universal Time */
    {
        // Calculate time of the day in UT decimal hours
        decHours = dateTime.hour - location.timezone +
            (dateTime.minute + dateTime.second / 60.0 ) / 60.0;

        // Calculate current Julian Day
        int liAux1 = (dateTime.month-14) / 12;
        int liAux2 = (1461*(dateTime.year + 4800 + liAux1)) / 4
            + (367 * (dateTime.month - 2 - 12 * liAux1)) / 12
            - (3 * ((dateTime.year + 4900 + liAux1) / 100)) / 4
            + dateTime.day - 32075;
        double dJulianDate = (double) liAux2 - 0.5 + decHours / 24.0;

        // Calculate difference between current Julian Day and JD 2451545.0
        elapsedJulianDays = dJulianDate - 2451545.0;
    }

    /* Calculate ecliptic coordinates (ecliptic longitude and obliquity of the
       ecliptic in radians but without limiting the angle to be less than 2*Pi
       (i.e., the result may be greater than 2*Pi) */
    {
        double omega = 2.1429 - 0.0010394594 * elapsedJulianDays;
        double meanLongitude = 4.8950630 + 0.017202791698 * elapsedJulianDays; // Radians
        double anomaly = 6.2400600 + 0.0172019699 * elapsedJulianDays;

        eclipticLongitude = meanLongitude + 0.03341607 * std::sin(anomaly)
            + 0.00034894 * std::sin(2*anomaly) - 0.0001134
            - 0.0000203 * std::sin(omega);

        eclipticObliquity = 0.4090928 - 6.2140e-9 * elapsedJulianDays
            + 0.0000396 * std::cos(omega);
    }

    /* Calculate celestial coordinates ( right ascension and declination ) in radians
       but without limiting the angle to be less than 2*Pi (i.e., the result may be
       greater than 2*Pi) */
    {
        double sinEclipticLongitude = std::sin(eclipticLongitude);
        dY = std::cos(eclipticObliquity) * sinEclipticLongitude;
        dX = std::cos(eclipticLongitude);
        rightAscension = std::atan2(dY, dX);
        if (rightAscension < 0.0)
            rightAscension += 2*M_PI;
        declination = std::asin(std::sin(eclipticObliquity) * sinEclipticLongitude);
    }

    // Calculate local coordinates (azimuth and zenith angle) in degrees
    {
        double greenwichMeanSiderealTime = 6.6974243242
            + 0.0657098283 * elapsedJulianDays + decHours;

        double localMeanSiderealTime = ek::deg_to_rad<double> (greenwichMeanSiderealTime * 15
            + location.longitude);

        double latitudeInRadians = ek::deg_to_rad<double>(location.latitude);
        double cosLatitude = std::cos(latitudeInRadians);
        double sinLatitude = std::sin(latitudeInRadians);

        double hourAngle = localMeanSiderealTime - rightAscension;
        double cosHourAngle = std::cos(hourAngle);

        elevation = std::acos(cosLatitude * cosHourAngle
            * std::cos(declination) + std::sin(declination) * sinLatitude);

        dY = -std::sin(hourAngle);
        dX = std::tan(declination) * cosLatitude - sinLatitude * cosHourAngle;

        azimuth = std::atan2(dY, dX);
        if (azimuth < 0.0)
            azimuth += 2*M_PI;

        // Parallax Correction
        elevation += (EARTH_MEAN_RADIUS / ASTRONOMICAL_UNIT) * std::sin(elevation);
    }

    return SphericalCoordinates((float) elevation, (float) azimuth);
}

SphericalCoordinates compute_sun_coordinates(const Vector<float, 3>& sunDir, const Transform<Point<float, 4>> &worldToLuminaire) {
    return from_sphere(normalize(worldToLuminaire * sunDir));
}

SphericalCoordinates compute_sun_coordinates(const Properties &props) {
    using Float = float;
    MTS_IMPORT_CORE_TYPES();
    /* configure position of sun */
    if (props.has_property("sun_direction")) {
        if (props.has_property("latitude") || props.has_property("longitude")
            || props.has_property("timezone") || props.has_property("day")
            || props.has_property("time"))
            Log(Error, "Both the 'sun_direction' parameter and time/location "
                    "information were provided -- only one of them can be specified at a time!");
            ref<AnimatedTransform> at = props.animated_transform("to_world", nullptr);
            return compute_sun_coordinates(
                props.get<Vector3f>("sun_direction"),
                at ? at->eval<float>(0).inverse() : Transform<Point<float, 4>>() );
        
        } else {
        LocationRecord location;
        DateTimeRecord dateTime;

        location.latitude  = props.get<double>("latitude", 35.6894);
        location.longitude = props.get<double>("longitude", 139.6917);
        location.timezone  = props.get<double>("timezone", 9);
        dateTime.year      = props.get<int>("year", 2010);
        dateTime.day       = props.get<int>("day", 10);
        dateTime.month     = props.get<int>("month", 7);
        dateTime.hour      = props.get<double>("hour", 15.0);
        dateTime.minute    = props.get<double>("minute", 0.0);
        dateTime.second    = props.get<double>("second", 0.0);

        SphericalCoordinates coords = compute_sun_coordinates(dateTime, location);

        Log(Debug, "Computed sun position for %s and %s: %s",
            location.to_string().c_str(), dateTime.to_string().c_str(),
            coords.to_string().c_str());

        return coords;
    }
}

/* All data lifted from MI. Units are either [] or cm^-1. refer when in doubt MI */

// k_o Spectrum table from pg 127, MI.

float k_o_wavelengths[64] = {
    300, 305, 310, 315, 320, 325, 330, 335, 340, 345,
    350, 355, 445, 450, 455, 460, 465, 470, 475, 480,
    485, 490, 495, 500, 505, 510, 515, 520, 525, 530,
    535, 540, 545, 550, 555, 560, 565, 570, 575, 580,
    585, 590, 595, 600, 605, 610, 620, 630, 640, 650,
    660, 670, 680, 690, 700, 710, 720, 730, 740, 750,
    760, 770, 780, 790
};


float k_o_amplitudes[65] = {
    10.0, 4.8, 2.7, 1.35, .8, .380, .160, .075, .04, .019, .007,
    .0, .003, .003, .004, .006, .008, .009, .012, .014, .017,
    .021, .025, .03, .035, .04, .045, .048, .057, .063, .07,
    .075, .08, .085, .095, .103, .110, .12, .122, .12, .118,
    .115, .12, .125, .130, .12, .105, .09, .079, .067, .057,
    .048, .036, .028, .023, .018, .014, .011, .010, .009,
    .007, .004, .0, .0
};

// k_g Spectrum table from pg 130, MI.
float k_g_wavelengths[4] = {
    759, 760, 770, 771
};

 
float k_g_amplitudes[4] = {
    0, 3.0, 0.210, 0
};

// k_wa Spectrum table from pg 130, MI.
float k_wa_wavelengths[13] = {
    689, 690, 700, 710, 720,
    730, 740, 750, 760, 770,
    780, 790, 800
};


float k_wa_amplitudes[13] = {
    0, 0.160e-1, 0.240e-1, 0.125e-1,
    0.100e+1, 0.870, 0.610e-1, 0.100e-2,
    0.100e-4, 0.100e-4, 0.600e-3,
    0.175e-1, 0.360e-1
};

/* Wavelengths corresponding to the table below */
float sol_wavelengths[38] = {
    380, 390, 400, 410, 420, 430, 440, 450,
    460, 470, 480, 490, 500, 510, 520, 530,
    540, 550, 560, 570, 580, 590, 600, 610,
    620, 630, 640, 650, 660, 670, 680, 690,
    700, 710, 720, 730, 740, 750
};

/* Solar amplitude in watts / (m^2 * nm * sr) */

float sol_amplitudes[38] = {
    16559.0, 16233.7, 21127.5, 25888.2, 25829.1,
    24232.3, 26760.5, 29658.3, 30545.4, 30057.5,
    30663.7, 28830.4, 28712.1, 27825.0, 27100.6,
    27233.6, 26361.3, 25503.8, 25060.2, 25311.6,
    25355.9, 25134.2, 24631.5, 24173.2, 23685.3,
    23212.1, 22827.7, 22339.8, 21970.2, 21526.7,
    21097.9, 20728.3, 20240.4, 19870.8, 19427.2,
    19072.4, 18628.9, 18259.2
};


NAMESPACE_END(mitsuba)
