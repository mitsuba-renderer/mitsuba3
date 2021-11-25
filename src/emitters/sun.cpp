
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

#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/qmc.h>

NAMESPACE_BEGIN(mitsuba)

/* Apparent radius of the sun as seen from the earth (in degrees).
   This is an approximation--the actual value is somewhere between
   0.526 and 0.545 depending on the time of year */
#define SUN_APP_RADIUS 0.5358f

template <typename Float, typename Spectrum>
class SunEmitter final : public Emitter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Emitter, m_flags, m_to_world)
    MTS_IMPORT_TYPES(Scene, Shape, Texture)

    using DiscreteSpectrum = mitsuba::Spectrum<ScalarFloat, 11>;
    using SunPixelFormat = std::conditional_t<is_spectral_v<Spectrum>, DiscreteSpectrum, ScalarColor3f>;

    SunEmitter(const Properties &props): Base(props) {
            
        m_scale = props.get<float>("scale", 1.0f);
        m_turbidity = props.get<float>("turbidity", 3.0f);
        m_stretch = props.get<float>("stretch", 1.0f);
        m_resolution = props.get<int>("resolution", 512);

        m_sun = compute_sun_coordinates(props);
        m_sun_radius_scale = props.get<float>("sun_radius_scale", 1.0f);

        m_flags = EmitterFlags::Infinite | EmitterFlags::SpatiallyVarying;

        SphericalCoordinates sun(m_sun);
        sun.elevation *= m_stretch;
        m_sun_dir = to_sphere(sun);

        m_theta = ek::deg_to_rad(SUN_APP_RADIUS * 0.5f);
        m_solid_angle = 2 * enoki::Pi<Float> * (1 - std::cos(m_theta));

        if (m_sun_radius_scale == 0)
            m_scale *= m_solid_angle;
        
        auto m_wavelength_data = compute_sun_radiance(m_sun.elevation, m_turbidity, m_scale);
        m_data = m_wavelength_data.first;
        m_wavelengths = m_wavelength_data.second;

    }

    /// This emitter does not occupy any particular region of space, return an invalid bounding box
    ScalarBoundingBox3f bbox() const override {
        return ScalarBoundingBox3f();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SunEmitter[" << std::endl
            << "  sunDir = " << m_sun_dir  << "," << std::endl
            << "  sunRadiusScale = " << m_sun_radius_scale << "," << std::endl
            << "  turbidity = " << m_turbidity << "," << std::endl
            << "  scale = " << m_scale << std::endl
            << "]";
        return oss.str();
    }

    std::vector<ref<Object>> expand() const override {

        using ScalarArray3f = ek::Array<ScalarFloat, 3>;

        if (m_sun_radius_scale == 0){
            Properties props("directional");

            Vector<float, 3> direc = -(m_to_world.scalar() * m_sun_dir);
            ScalarArray3f direction_array(direc.x(), direc.y(), direc.z());
            props.set_array3f("direction", direction_array);

            
            Properties props_radiance("regular");
            props_radiance.set_float("lambda_min", 320);
            props_radiance.set_float("lambda_max", 800);
            props_radiance.set_pointer("wavelengths", m_wavelengths.data());
            props_radiance.set_pointer("values", m_data.data());
            props_radiance.set_long("size", m_wavelengths.size());
            ref<Texture> m_radiance = PluginManager::instance()->create_object<Texture>(props_radiance);
            

            props.set_object("irradiance", m_radiance);
            ref<Object> emitter = PluginManager::instance()->create_object<Base>(props).get();

            return {emitter};
        }

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

        ref<Bitmap> bitmap;
        if constexpr (is_spectral_v<Spectrum>){
            const std::vector<std::string> channel_names = {"320", "360", 
            "400", "440", "480", "520", "560", "600", "640", "680", "720"};
            bitmap = new Bitmap(Bitmap::PixelFormat::MultiChannel, Struct::Type::Float32, 
            ScalarVector2i(m_resolution, m_resolution / 2), channel_names.size(), channel_names);
        }else{
            bitmap = new Bitmap(Bitmap::PixelFormat::RGBA, Struct::Type::Float32, 
            ScalarVector2i(m_resolution, m_resolution / 2));
        }
        bitmap->clear();
        ScalarFrame3f frame(m_sun_dir);

        ScalarPoint2f factor(bitmap->width() / (2 * enoki::Pi<Float>) , 
            bitmap->height() / enoki::Pi<Float>);

        SunPixelFormat value;

        if constexpr (is_spectral_v<Spectrum>){
            IrregularContinuousDistribution<double> m_radiance_dist(m_wavelengths.data(), m_data.data(), 97);
            for (size_t i = 0; i < SunPixelFormat::Size; i ++){
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
                ek::min(ek::max(0, (int) (sph_coords.azimuth * factor.x())), bitmap->width() - 1),
                ek::min(ek::max(0, (int) (sph_coords.elevation * factor.y())), bitmap->height() - 1)
            );

            SunPixelFormat val = value / ek::max(1e-3f, sin_theta);
            // std::cout << val << std::endl;

            if constexpr (is_spectral_v<Spectrum>){
                float *target = (float *)bitmap->data();
                float *ptr = target + SunPixelFormat::Size * (pos.x() + pos.y() * bitmap->width());
                for (size_t i = 0; i < SunPixelFormat::Size; ++i)
                        *ptr++ += float(val[i]);
                
            }else{
                SunPixelFormat *target = (SunPixelFormat *)bitmap->data();
                SunPixelFormat *ptr = target + (pos.x() + pos.y() * bitmap->width());
                *ptr++ += val;
            }
        }

        FileStream *fs = new FileStream("sun.exr", FileStream::ETruncReadWrite);
        bitmap->write(fs, Bitmap::FileFormat::OpenEXR);


        Properties prop("envmap");
        prop.set_pointer("bitmap", bitmap.get());
        ref<Object> emitter = PluginManager::instance()->create_object<Base>(prop).get();

        return {emitter};

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

        std::cout << data << std::endl;
        std::cout << wavelengths << std::endl;


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
    /// Environment map resolution
    int m_resolution;
    /// Constant scale factor applied to the model
    ScalarFloat m_scale;
    /// Scale factor that can be applied to the sun radius
    ScalarFloat m_sun_radius_scale;
    /// Angle cutoff for the sun disk (w/o scaling)
    ScalarFloat m_theta;
    /// Solid angle covered by the sun (w/o scaling)
    ScalarFloat m_solid_angle;
    /// Position of the sun in spherical coordinates
    SphericalCoordinates m_sun;
    /// Direction of the sun (untransformed)
    Vector<float, 3> m_sun_dir;
    /// Turbidity of the atmosphere
    ScalarFloat m_turbidity;
    // /// Radiance arriving from the sun disk
    // ref<Texture> m_radiance;
    /// Stretch factor to extend to the bottom hemisphere
    ScalarFloat m_stretch;

    std::vector<double> m_data;

    std::vector<double> m_wavelengths;


};

MTS_IMPLEMENT_CLASS_VARIANT(SunEmitter, Emitter)
MTS_EXPORT_PLUGIN(SunEmitter, "Sun Emitter")
NAMESPACE_END(mitsuba)
