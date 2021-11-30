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

#include "sunsky/sunmodel.h"
#include "sunsky/skymodel.h"
#include <cmath>


NAMESPACE_BEGIN(mitsuba)

/**!

.. _emitter-sky:

Skylight emitter (:monosp:`sky`)
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

 This plugin provides the physically-based skylight model by
 Hosek and Wilkie. It can be used to create predictive daylight renderings 
 of scenes under clear skies,
 which is useful for architectural and computer vision applications.
 The implementation in Mitsuba is based on code that was
 generously provided by the authors.
 
 The model has two main parameters: the turbidity of the atmosphere
 and the position of the sun.
 The position of the sun in turn depends on a number of secondary
 parameters, including the latitude, longitude,
 and timezone at the location of the observer, as well as the
 current year, month, day, hour,
 minute, and second.
 Using all of these, the elevation and azimuth of the sun are computed
 using the PSA algorithm by Blanco et al. \cite{Blanco2001Computing},
 which is accurate to about 0.5 arcminutes (\nicefrac{1}{120} degrees).
 Note that this algorithm does not account for daylight
 savings time where it is used, hence a manual correction of the
 time may be necessary.
 For detailed coordinate and timezone information of various cities, see
 \url{http://www.esrl.noaa.gov/gmd/grad/solcalc}.

 If desired, the world-space solar vector may also be specified
 using the sun_direction} parameter, in which case all of the
 previously mentioned time and location parameters become irrelevant.


 Turbidity, the other important parameter, specifies the aerosol
 content of the atmosphere. Aerosol particles cause additional scattering that
 manifests in a halo around the sun, as well as color fringes near the
 horizon.
 Smaller turbidity values ($\sim 1-2$) produce an
 arctic-like clear blue sky, whereas larger values ($\sim 8-10$)
 create an atmosphere that is more typical of a warm, humid day.
 Note that this model does not aim to reproduce overcast, cloudy, or foggy
 atmospheres with high corresponding turbidity values. An photographic
 environment map may be more appropriate in such cases.

 The default coordinate system of the emitter associates the up
 direction with the $+Y$ axis. The east direction is associated with $+X$
 and the north direction is equal to $+Z$. To change this coordinate
 system, rotations can be applied using the toWorld parameter
 (see \lstref{sky-up} for an example).
 
 By default, the emitter will not emit any light below the
 horizon, which means that these regions are black when
 observed directly. By setting the stretch parameter to values
 between 1 and 2, the sky can be extended to cover these directions
 as well. This is of course a complete kludge and only meant as a quick
 workaround for scenes that are not properly set up.

 Instead of evaluating the full sky model every on every radiance query,
 the implementation precomputes a low resolution environment map
 (512$\times$ 256) of the entire sky that is then forwarded to the
 \pluginref{envmap} plugin---this dramatically improves rendering
 performance. This resolution is generally plenty since the sky radiance
 distribution is so smooth, but it can be adjusted manually if
 necessary using the resolution} parameter.
 
 Note that while the model encompasses sunrise and sunset configurations,
 it does not extend to the night sky, where illumination from stars, galaxies,
 and the moon dominate. When started with a sun configuration that lies
 below the horizon, the plugin will fail with an error message.
 
 Physical units and spectral rendering
 The sky model introduces physical units into the rendering process.
 The radiance values computed by this plugin have units of power ($W$) per
 unit area ($m^{-2}$) per steradian ($sr^{-1}$) per unit wavelength ($nm^{-1}$).
 If these units are inconsistent with your scene description, you may use the
 optional \texttt{scale} parameter to adjust them.
 
 When Mitsuba is compiled for spectral rendering, the plugin switches
 from RGB to a spectral variant of the skylight model, which relies on
 precomputed data between $320$ and $720 nm$ sampled at $40nm$-increments.
 
 Ground albedo
 The albedo of the ground (e.g. due to rock, snow, or vegetation) can have a
 noticeable and nonlinear effect on the appearance of the sky.
 \figref{sky_groundalbedo} shows an example of this effect. By default,
the ground albedo is set to a 15% gray.
     

 */

/// S-shaped smoothly varying interpolation between two values
template <typename Scalar> 
inline Scalar smooth_step(Scalar min, Scalar max, Scalar value) {
    Scalar v = ek::clamp((value - min) / (max - min), (Scalar) 0, (Scalar) 1);
    return v * v * (-2 * v  + 3);
}

template <typename Float, typename Spectrum>
class SkyEmitter final : public Emitter<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Emitter, m_flags, m_to_world)
    MTS_IMPORT_TYPES(Scene, Shape, Texture)

    using DiscreteSpectrum = mitsuba::Spectrum<ScalarFloat, 11>;

    using SkyPixelFormat = std::conditional_t<is_spectral_v<Spectrum>, DiscreteSpectrum, ScalarColor3f>;

    SkyEmitter(const Properties &props): Base(props) {

        m_scale = props.get<float>("scale", 1.0f);
        m_turbidity = props.get<float>("turbidity", 3.0f);
        m_stretch = props.get<float>("stretch", 1.0f);
        m_resolution = props.get<int>("resolution", 512);
        
        m_sun = compute_sun_coordinates(props); 
        m_extend = props.get<bool>("extend", false);
        m_albedo = props.get<ScalarColor3f>("albedo", ScalarColor3f(0.2f));
        m_flags = EmitterFlags::Infinite | EmitterFlags::SpatiallyVarying;

        if (m_turbidity < 1 || m_turbidity > 10)
             Log(Error, "The turbidity parameter must be in the range[1,10]!");
        if (m_stretch < 1 || m_stretch > 2)
             Log(Error, "The stretch parameter must be in the range [1,2]!");
        for (size_t i=0; i < m_albedo.Size; ++i) {
             if (m_albedo[i] < 0 || m_albedo[i] > 1)
                 Log(Error, "The albedo parameter must be in the range [0,1]!");
         }

        ScalarFloat sun_elevation = 0.5f * enoki::Pi<Float> - m_sun.elevation;

        if (sun_elevation < 0)
            Log(Error, "The sun is below the horizon -- this is not supported by the sky model.");

        if constexpr (is_spectral_v<Spectrum>){
            for (size_t i = 0; i < m_channels; ++i)
                m_state[i] = arhosekskymodelstate_alloc_init
                ((double)sun_elevation, (double)m_turbidity, (double)luminance<Float>(m_albedo));
            
        }
        else {
            for (size_t i = 0; i < m_channels; ++i)
                m_state[i] = arhosek_rgb_skymodelstate_alloc_init
                ((double)m_turbidity, (double)m_albedo[i], (double)sun_elevation);
            
        }
    }

    ~SkyEmitter(){
        for (size_t i = 0; i < m_channels; ++i)
            arhosekskymodelstate_free(m_state[i]);
    }


    std::vector<ref<Object>> expand() const override {
        //expends into a envmap plugin writing to a bitmap
        ref<Bitmap> bitmap;
        if constexpr (!is_spectral_v<Spectrum>)
            bitmap = new Bitmap(Bitmap::PixelFormat::RGBA, Struct::Type::Float32, 
            ScalarVector2i(m_resolution, m_resolution / 2));
        else{
            const std::vector<std::string> channel_names = {"320", "360", 
            "400", "440", "480", "520", "560", "600", "640", "680", "720"};
            bitmap = new Bitmap(Bitmap::PixelFormat::MultiChannel, Struct::Type::Float32, 
            ScalarVector2i(m_resolution, m_resolution / 2), channel_names.size(), channel_names);
        }
        
        ScalarPoint2f factor((2 * enoki::Pi<Float>) / bitmap->width(), 
            enoki::Pi<Float> / bitmap->height());

        if constexpr (!is_spectral_v<Spectrum>){
            for (size_t y = 0; y < bitmap->height(); ++y) {
                ScalarFloat theta = (y + .5f) * factor.y();
                SkyPixelFormat *target = (SkyPixelFormat *) bitmap->data() + y * bitmap->width();

                for (size_t x = 0; x < bitmap->width(); ++x) {
                    ScalarFloat phi = (x + .5f) * factor.x();
                    SkyPixelFormat sky_rad = get_sky_radiance(SphericalCoordinates(theta, phi));
                    *target++ = sky_rad;
                }
            }
        } else{
            for (size_t y = 0; y < bitmap->height(); ++y) {
                ScalarFloat theta = (y + .5f) * factor.y();
                float *target = (float *) bitmap->data() + (y * bitmap->width()) * SkyPixelFormat::Size;

                for (size_t x = 0; x < bitmap->width(); ++x) {
                    ScalarFloat phi = (x + .5f) * factor.x();
                    SkyPixelFormat sky_rad = get_sky_radiance(SphericalCoordinates(theta, phi)); 
                    for (size_t i = 0; i < SkyPixelFormat::Size; ++i)
                        *target++ = float(sky_rad[i]);
                }
            }
        }

        if constexpr (!is_spectral_v<Spectrum>)
            bitmap =  bitmap->convert(Bitmap::PixelFormat::RGB, struct_type_v<Float>, false);

        Properties props("envmap");

        props.set_pointer("bitmap", (uint8_t *) bitmap.get());
        ref<Object> emitter = PluginManager::instance()->create_object<Base>(props).get();

        return {emitter};
    }

    SkyPixelFormat get_sky_radiance(const SphericalCoordinates coords) const {
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

        SkyPixelFormat result;

        for (size_t i = 0; i < SkyPixelFormat::Size; i++) {
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
    
        return result * m_scale;
    }


    /// This emitter does not occupy any particular region of space, return an invalid bounding box
    ScalarBoundingBox3f bbox() const override {
        return ScalarBoundingBox3f();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SkyEmitter[" << std::endl
            << "  turbidity = " << m_turbidity << "," << std::endl
            << "  sun_pos = " << m_sun.to_string() << ","  << std::endl
            << "  resolution = " << m_resolution << ","  << std::endl
            << "  stretch = " << m_stretch << ","  << std::endl
            << "  scale = " << m_scale << std::endl
            << "]";
        return oss.str();
    }


     

    /// Declare RTTI data structures
    MTS_DECLARE_CLASS()
protected:
    /// Environment map resolution in pixels
    int m_resolution;
    /// Constant scale factor applied to the model
    ScalarFloat m_scale;
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

};

/// Implement RTTI data structures
MTS_IMPLEMENT_CLASS_VARIANT(SkyEmitter, Emitter)
MTS_EXPORT_PLUGIN(SkyEmitter, "Sky Emitter")
NAMESPACE_END(mitsuba)