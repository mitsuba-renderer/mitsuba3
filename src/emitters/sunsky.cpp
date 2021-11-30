
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/srgb.h>

#include <mitsuba/core/fresolver.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/scene.h>

#include <mitsuba/core/fstream.h>
#include <mitsuba/render/fwd.h>
#include "sunsky/sunmodel.h"
#include "sunsky/skymodel.h"

#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/qmc.h>

NAMESPACE_BEGIN(mitsuba)

/* Apparent radius of the sun as seen from the earth (in degrees).
   This is an approximation--the actual value is somewhere between
   0.526 and 0.545 depending on the time of year */
#define SUN_APP_RADIUS 0.5358f

/**!

.. _emitter-sunsky:

Sunsky emitter (:monosp:`sunsky`)
--------------------------------------

.. pluginparameters::

 * - turbidity
   - |Float|
   - Determines the amount of aerosol present in the atmosphere.
     Valid range: 1-10. (Default: 3, corresponding to a clear sky in a temperate climate)

 * - albedo
   - |Vector3f|
   - Specifies the ground albedo. (Default: ScalarColor3f(0.2f))

 * - year, month, day
   - |Int|
   - Denote the date of the observation. (Default: 2010, 07, 10)

 * - latitude, longitude, timezone
   - |Float|
   - Specify the oberver's latitude and longitude in degrees, and 
     the local timezone offset in hours, which are required to 
     compute the sun's position. 
     (Default 35.6894, 139.6917, 9 --- Tokyo, Japan)

 * - sun_direction
   - |Vector3f|
   - Allows to manually override the sun direction in world space.
     When this value is provided, parameters pertaining to the 
     computation of the sun direction (year, hour, latitude,} etc.
     are unnecessary. (Default: none)

 * - stretch
   - |Float|
   - Stretch factor to extend emitter below the horizon, must be 
     in [1,2] (Default: 1, i.e. not used)

 * - resolution
   - |Int|
   - Specifies the horizontal resolution of the precomputed image 
     that is used to represent the environment map. (Default: 512)

 * - scale
   - |Float|
   - This parameter can be used to scale the amount of illumination 
     emitted by the sky emitter. Default: 1.

 * - sun_radius_scale
   - |Float|
   - Scale factor to adjust the radius of the sun, while preserving its power.
     Set to 0 to turn it into a directional light source.

 * - has_sky
   - |Boolean|
   - Display the sky.

 * - has_sun
   - |Boolean|
   - Display the sun.

 This convenience plugin has the sole purpose of instantiating
 \pluginref{sun} and \pluginref{sky} and merging them into a joint
 environment map. Please refer to these plugins individually for more
 details.
 
 It can also be a single sun plugin or a single sky plugin when setting 
 either of the plugin to not be displayed. 
 */


/// S-shaped smoothly varying interpolation between two values
template <typename Scalar> 
inline Scalar smooth_step(Scalar min, Scalar max, Scalar value) {
    Scalar v = ek::clamp((value - min) / (max - min), (Scalar) 0, (Scalar) 1);
    return v * v * (-2 * v  + 3);
}

template <typename Float, typename Spectrum>
class SunSkyEmitter final : public Emitter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Emitter, m_flags, m_to_world)
    MTS_IMPORT_TYPES(Scene, Shape, Texture)

    using ScalarArray3f = ek::Array<ScalarFloat, 3>;

    using DiscreteSpectrum = mitsuba::Spectrum<ScalarFloat, 11>;
    using SunSkyPixelFormat = std::conditional_t<is_spectral_v<Spectrum>, DiscreteSpectrum, ScalarColor3f>;

    SunSkyEmitter(const Properties &props): Base(props) {
        m_sky_scale = props.get<float>("sky_scale", 1.0f);
        m_sun_scale = props.get<float>("sun_scale", 1.0f);
        m_turbidity = props.get<float>("turbidity", 3.0f);
        m_stretch = props.get<float>("stretch", 1.0f);
        m_resolution = props.get<int>("resolution", 512);
        
        m_sun = compute_sun_coordinates(props); 
        m_sun_radius_scale = props.get<float>("sun_radius_scale", 1.0f);
        m_extend = props.get<bool>("extend", false);
        m_albedo = props.get<ScalarColor3f>("albedo", ScalarColor3f(0.2f));
        m_flags = EmitterFlags::Infinite | EmitterFlags::SpatiallyVarying;

        m_sky_display = props.get<bool>("has_sky", true);
        m_sun_display = props.get<bool>("has_sun", true);

        if (m_turbidity < 1 || m_turbidity > 10)
            Log(Error, "The turbidity parameter must be in the range[1,10]!");
        if (m_stretch < 1 || m_stretch > 2)
            Log(Error, "The stretch parameter must be in the range [1,2]!");
        for (size_t i=0; i < m_albedo.Size; ++i) {
            if (m_albedo[i] < 0 || m_albedo[i] > 1)
                Log(Error, "The albedo parameter must be in the range [0,1]!");
         }

        // imitate behavior of Mitsuba1 in this case
        if (m_sky_display && m_sun_radius_scale == 0){
            m_sun_display = false;
        }

        ScalarFloat sun_elevation = 0.5f * enoki::Pi<Float> - m_sun.elevation;

        if (sun_elevation < 0)
            Log(Error, "The sun is below the horizon -- this is not supported by the sky model.");

        if constexpr (is_spectral_v<Spectrum>){
            for (size_t i = 0; i < m_channels; ++i){
                m_state[i] = arhosekskymodelstate_alloc_init
                ((double)sun_elevation, (double)m_turbidity, (double)luminance<Float>(m_albedo));
            }
        }
        else {
            for (size_t i = 0; i < m_channels; ++i){
                m_state[i] = arhosek_rgb_skymodelstate_alloc_init
                ((double)m_turbidity, (double)m_albedo[i], (double)sun_elevation);
            }
        }

        SphericalCoordinates sun(m_sun);
        sun.elevation *= m_stretch;
        m_sun_dir = to_sphere(sun);

        m_theta = ek::deg_to_rad(SUN_APP_RADIUS * 0.5f);
        m_solid_angle = 2 * enoki::Pi<Float> * (1 - std::cos(m_theta));

        if (m_sun_radius_scale == 0)
            m_sun_scale *= m_solid_angle;
        
        auto m_wavelength_data = compute_sun_radiance(m_sun.elevation, m_turbidity, m_sun_scale);
        m_data = m_wavelength_data.first;
        m_wavelengths = m_wavelength_data.second;

    }

    std::vector<ref<Object>> expand() const override {


        ref<Bitmap> bitmap;
        if constexpr (!is_spectral_v<Spectrum>){
            bitmap = new Bitmap(Bitmap::PixelFormat::RGBA, Struct::Type::Float32, 
            ScalarVector2i(m_resolution, m_resolution / 2));
        }else{
            const std::vector<std::string> channel_names = {"320", "360", 
            "400", "440", "480", "520", "560", "600", "640", "680", "720"};
            bitmap = new Bitmap(Bitmap::PixelFormat::MultiChannel, Struct::Type::Float32, 
            ScalarVector2i(m_resolution, m_resolution / 2), channel_names.size(), channel_names);
        }

        if (m_sky_display){
            /* First, rasterize the sky */
            ScalarPoint2f factor((2 * enoki::Pi<Float>) / bitmap->width(), 
                enoki::Pi<Float> / bitmap->height());

            if constexpr (!is_spectral_v<Spectrum>){
                for (size_t y = 0; y < bitmap->height(); ++y) {
                    ScalarFloat theta = (y + .5f) * factor.y();
                    SunSkyPixelFormat *target = (SunSkyPixelFormat *) bitmap->data() + y * bitmap->width();

                    for (size_t x = 0; x < bitmap->width(); ++x) {
                        ScalarFloat phi = (x + .5f) * factor.x();
                        SunSkyPixelFormat sky_rad = get_sky_radiance(SphericalCoordinates(theta, phi));
                        *target++ = sky_rad;
                    }
                }
            } else{
                for (size_t y = 0; y < bitmap->height(); ++y) {
                    ScalarFloat theta = (y + .5f) * factor.y();
                    float *target = (float *) bitmap->data() + (y * bitmap->width()) * SunSkyPixelFormat::Size;

                    for (size_t x = 0; x < bitmap->width(); ++x) {
                        ScalarFloat phi = (x + .5f) * factor.x();
                        SunSkyPixelFormat sky_rad = get_sky_radiance(SphericalCoordinates(theta, phi)); 
                        for (size_t i = 0; i < SunSkyPixelFormat::Size; ++i)
                            *target++ = float(sky_rad[i]);
                    }
                }
            }
        }
        
        if (m_sun_display){

            //Then create the sun
            /* Step 1: compute a *very* rough estimate of how many
            pixel in the output environment map will be covered
            by the sun */

            size_t pixel_count = m_resolution * m_resolution / 2;
            ScalarFloat cos_theta = std::cos(m_theta * m_sun_radius_scale);

            /* Ratio of the sphere that is covered by the sun */
            ScalarFloat covered_portion = 0.5f * (1 - cos_theta);

            /* Approx. number of samples that need to be generated,
            be very conservative */
            size_t n_samples = (size_t) std::max(100.f, (pixel_count * covered_portion * 1000.f));

            ScalarFrame3f frame(m_sun_dir);

            ScalarPoint2f factor_sun(bitmap->width() / (2 * enoki::Pi<Float>) , 
                bitmap->height() / enoki::Pi<Float>);

            SunSkyPixelFormat value;

            if constexpr (is_spectral_v<Spectrum>){
                IrregularContinuousDistribution<double> m_radiance_dist(m_wavelengths.data(), m_data.data(), 97);
                for (size_t i = 0; i < SunSkyPixelFormat::Size; i ++){
                    double w_l = 320. + (double)i * 40.;
                    value[i] = m_radiance_dist.eval_pdf(w_l, true);
                }
                value *= 2 * ek::Pi<float> * (1 - std::cos(m_theta)) * (float) (bitmap->width() * bitmap->height()) /
                (2 * ek::Pi<float> * ek::Pi<float> * n_samples);

            }else{
                value = spectrum_list_to_srgb(m_wavelengths, m_data, false) * 2 * ek::Pi<float> 
                * (1 - std::cos(m_theta)) * (float) (bitmap->width() * bitmap->height()) /
                (2 * ek::Pi<float> * ek::Pi<float> * n_samples);
            }

            value *= MTS_CIE_Y_NORMALIZATION;
            
            for (size_t i = 0; i < n_samples; ++i) {
                ScalarVector3f dir = frame.to_world(warp::square_to_uniform_cone<ScalarFloat>(sample02(i), cos_theta));

                ScalarFloat sin_theta = ek::safe_sqrt(1 - dir.y() * dir.y());
                SphericalCoordinates sph_coords = from_sphere(dir);

                ScalarPoint2i pos(
                    ek::min(ek::max(0, (int) (sph_coords.azimuth * factor_sun.x())), bitmap->width() - 1),
                    ek::min(ek::max(0, (int) (sph_coords.elevation * factor_sun.y())), bitmap->height() - 1)
                );

                SunSkyPixelFormat val = value / ek::max(1e-3f, sin_theta);

                if constexpr (is_spectral_v<Spectrum>){
                    float *target = (float *)bitmap->data();
                    float *ptr = target + SunSkyPixelFormat::Size * (pos.x() + pos.y() * bitmap->width());
                    for (size_t i = 0; i < SunSkyPixelFormat::Size; ++i)
                        *ptr++ += float(val[i]);
                    
                }else{
                    SunSkyPixelFormat *target = (SunSkyPixelFormat *)bitmap->data();
                    SunSkyPixelFormat *ptr = target + (pos.x() + pos.y() * bitmap->width());
                    *ptr++ += val;
                }
            }

        }

        
        if constexpr (!is_spectral_v<Spectrum>){
            bitmap =  bitmap->convert(Bitmap::PixelFormat::RGB, struct_type_v<Float>, false);
        }
        /* Instantiate a nested envmap plugin */
        Properties props("envmap");

        props.set_pointer("bitmap", (uint8_t *) bitmap.get());
        ref<Object> emitter = PluginManager::instance()->create_object<Base>(props).get();

        return {emitter};

    }

    

    /// This emitter does not occupy any particular region of space, return an invalid bounding box
    ScalarBoundingBox3f bbox() const override {
        return ScalarBoundingBox3f();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SunSkyEmitter[" << std::endl
            << "  sun_dir = " << m_sun_dir  << "," << std::endl
            << "  sun_radius_scale = " << m_sun_radius_scale << "," << std::endl
            << "  turbidity = " << m_turbidity << "," << std::endl
            << "  sun_scale = " << m_sun_scale << std::endl
            << "  sky_scale = " << m_sky_scale << std::endl
            << "  sun_pos = " << m_sun.to_string() << ","  << std::endl
            << "  resolution = " << m_resolution << ","  << std::endl
            << "  stretch = " << m_stretch << ","  << std::endl
            << "]";
        return oss.str();
    }

    SunSkyPixelFormat get_sky_radiance(const SphericalCoordinates coords) const {
        ScalarFloat theta = coords.elevation / m_stretch;

        if (cos(theta) <= 0) {
            if (!m_extend)
                return 0;
            else
                theta = 0.5f * ek::Pi<Float> - 1e-4f;
        }

        ScalarFloat cos_gamma = cos(theta) * cos(m_sun.elevation)
            + sin(theta) * sin(m_sun.elevation)
            * cos(coords.azimuth - m_sun.azimuth);

        // Angle between the sun and the spherical coordinates in radians
        ScalarFloat gamma = ek::safe_acos(cos_gamma);

        SunSkyPixelFormat result;

        for (size_t i = 0; i < SunSkyPixelFormat::Size; i++) {
            if constexpr (!is_spectral_v<Spectrum>)
                result[i] = (ScalarFloat) (arhosek_tristim_skymodel_radiance(
                m_state[i], (double)theta, (double)gamma, i) * MTS_CIE_Y_NORMALIZATION); // (sum of Spectrum::CIE_Y)
            else{
                int wave_l = 320 + i * 40;
                result[i] = (ScalarFloat) (arhosekskymodel_radiance(
                m_state[i], (double)theta, (double)gamma, (double)wave_l) * MTS_CIE_Y_NORMALIZATION ); 
            }
        }

        result = max(result, 0.f);

        if (m_extend)
            result *= smooth_step<ScalarFloat>(0.f, 1.f, 2.f - 2.f * coords.elevation * ek::InvPi<Float>);
    
        return result * m_sky_scale;
    }

    /* The following is from the implementation of "A Practical Analytic Model for
    Daylight" by A.J. Preetham, Peter Shirley, and Brian Smits */

    std::pair<std::vector<double>, std::vector<double>> compute_sun_radiance(float theta, float turbidity, float scale) {

        IrregularContinuousDistribution<float> k_o_curve(k_o_wavelengths, k_o_amplitudes, 64);
        IrregularContinuousDistribution<float> k_g_curve(k_g_wavelengths, k_g_amplitudes, 4);
        IrregularContinuousDistribution<float> k_wa_curve(k_wa_wavelengths, k_wa_amplitudes, 13);
        IrregularContinuousDistribution<float> sol_curve(sol_wavelengths, sol_amplitudes, 38);

        std::vector<double> data(97), wavelengths(97);  // (800 - 320) / 5  + 1

        float beta = 0.04608365822050f * turbidity - 0.04586025928522f;

        // Relative Optical Mass
        float m = 1.0f / ((float) cos(theta) + 0.15f *
            pow(93.885f - theta * ek::InvPi<float> * 180.0f, (float) -1.253f));

        float lambda;
        int i = 0;
        for (i = 0, lambda = 320; i < 97; i++, lambda += 5) {

            // Rayleigh Scattering
            // Results agree with the graph (pg 115, MI) */
            float tau_r = ek::exp(-m * 0.008735f * ek::pow(lambda / 1000.0f, (float) -4.08));

            // Aerosol (water + dust) attenuation
            // beta - amount of aerosols present
            // alpha - ratio of small to large particle sizes. (0:4,usually 1.3)
            // Results agree with the graph (pg 121, MI)
            const float alpha = 1.3f;
            float tau_a = ek::exp(-m * beta * ek::pow(lambda / 1000.0f, -alpha));  // lambda should be in um

            // Attenuation due to ozone absorption
            // lOzone - amount of ozone in cm(NTP)
            // Results agree with the graph (pg 128, MI)
            const float l_ozone = .35f;
            float tau_o = ek::exp(-m * k_o_curve.eval_pdf(lambda) * l_ozone);

            // Attenuation due to mixed gases absorption
            // Results agree with the graph (pg 131, MI)
            float tau_g = ek::exp(-1.41f * k_g_curve.eval_pdf(lambda) * m / pow(1 + 118.93f
                * k_g_curve.eval_pdf(lambda) * m, (float) 0.45f));

            // Attenuation due to water vapor absorbtion
            // w - precipitable water vapor in centimeters (standard = 2)
            // Results agree with the graph (pg 132, MI)
            const float w = 2.0f;
            float tau_wa = ek::exp(-0.2385f * k_wa_curve.eval_pdf(lambda) * w * m /
                    pow(1 + 20.07f * k_wa_curve.eval_pdf(lambda) * w * m, (float) 0.45f));

            data[i] = (double) (sol_curve.eval_pdf(lambda) * tau_r * tau_a * tau_o * tau_g * tau_wa * scale);
            wavelengths[i] = (double)lambda;
        }
        return std::make_pair(data, wavelengths);
        
    }

    /// Van der Corput radical inverse in base 2 with double precision
    inline double radicalInverse2Double(uint64_t n, uint64_t scramble = 0ULL) const {
        /* Efficiently reverse the bits in 'n' using binary operations */
        #if (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2))) || defined(__clang__)
            n = __builtin_bswap64(n);
        #else
            printf("!!!!!!!!!!!!!!!!!!!!! radical else\n");
            n = (n << 32) | (n >> 32);
            n = ((n & 0x0000ffff0000ffffULL) << 16) | ((n & 0xffff0000ffff0000ULL) >> 16);
            n = ((n & 0x00ff00ff00ff00ffULL) << 8)  | ((n & 0xff00ff00ff00ff00ULL) >> 8);
        #endif
            n = ((n & 0x0f0f0f0f0f0f0f0fULL) << 4)  | ((n & 0xf0f0f0f0f0f0f0f0ULL) >> 4);
            n = ((n & 0x3333333333333333ULL) << 2)  | ((n & 0xccccccccccccccccULL) >> 2);
            n = ((n & 0x5555555555555555ULL) << 1)  | ((n & 0xaaaaaaaaaaaaaaaaULL) >> 1);

        // Account for the available precision and scramble
        n = (n >> (64 - 53)) ^ (scramble & ~-(1LL << 53));

        return (double) n / (double) (1ULL << 53);
    }

    /// Sobol' radical inverse in base 2 with double precision.
    inline double sobol2Double(uint64_t n, uint64_t scramble = 0ULL) const {
        scramble &= ~-(1LL << 53);
        for (uint64_t v = 1ULL << 52; n != 0; n >>= 1, v ^= v >> 1)
            if (n & 1)
                scramble ^= v;
        return (double) scramble / (double) (1ULL << 53);
    }

    /// Generate an element from a (0, 2) sequence (without scrambling)
    inline ScalarPoint2f sample02(size_t n) const {
        return ScalarPoint2f(
            radicalInverse2Double((uint64_t) n),
            sobol2Double((uint64_t) n)
        );
    }



    MTS_DECLARE_CLASS()
protected:
    /// Environment map resolution in pixels
    int m_resolution;
    /// Constant scale factor applied to the model
    ScalarFloat m_sky_scale;
    ScalarFloat m_sun_scale;
    /// Sky turbidity
    ScalarFloat m_turbidity;
    /// Position of the sun in spherical coordinates
    SphericalCoordinates m_sun;
    /// Stretch factor to extend to the bottom hemisphere
    ScalarFloat m_stretch;
    /// Extend to the bottom hemisphere (super-unrealistic mode)
    bool m_extend;
    /// temp variable for sample count
    static constexpr size_t m_channels = is_spectral_v<Spectrum> ? 11 : 3;
    /// Ground albedo
    ScalarColor3f m_albedo;
    /// State vector for the sky model
    ArHosekSkyModelState *m_state[m_channels];
    /// Scale factor that can be applied to the sun radius
    ScalarFloat m_sun_radius_scale;
    /// Angle cutoff for the sun disk (w/o scaling)
    ScalarFloat m_theta;
    /// Solid angle covered by the sun (w/o scaling)
    ScalarFloat m_solid_angle;
    /// Direction of the sun (untransformed)
    Vector<float, 3> m_sun_dir;
    /// Coefficients of the wavelength from the spectrum of the sun
    std::vector<double> m_data;
    /// Wavelength from the spectrum of the sun
    std::vector<double> m_wavelengths;
    /// Current render displays the sky
    bool m_sky_display;
    /// Current render displays the sun
    bool m_sun_display;
};

MTS_IMPLEMENT_CLASS_VARIANT(SunSkyEmitter, Emitter)
MTS_EXPORT_PLUGIN(SunSkyEmitter, "SunSky Emitter")
NAMESPACE_END(mitsuba)