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

#include <mitsuba/core/timer.h>
#include <mitsuba/core/qmc.h>
#include <mitsuba/core/vector.h>

NAMESPACE_BEGIN(mitsuba)

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

    SkyEmitter(const Properties &props): Base(props) {

        m_scale = props.get<float>("scale", 1.0f);
        m_turbidity = props.get<float>("turbidity", 3.0f);
        m_stretch = props.get<float>("stretch", 1.0f);
        m_resolution = props.get<int>("resolution", 512);
        
        m_sun = compute_sun_coordinates(props); 
        m_extend = props.get<bool>("extend", false);
        m_albedo = props.get<ScalarColor3f>("albedo", ScalarColor3f(0.2f));
        m_flags = EmitterFlags::Infinite | EmitterFlags::SpatiallyVarying;

        m_spectral = props.get<bool>("is_spectral", false);

        //TODO: temp setting count for bucket.
        if (m_spectral){
            m_channels = 11;
        }else{
            m_channels = 3;
        }
        
        if (m_turbidity < 1 || m_turbidity > 10)
             Log(Error, "The turbidity parameter must be in the range[1,10]!");
        if (m_stretch < 1 || m_stretch > 2)
             Log(Error, "The stretch parameter must be in the range [1,2]!");
        for (size_t i=0; i < m_albedo.Size; ++i) {
             if (m_albedo[i] < 0 || m_albedo[i] > 1)
                 Log(Error, "The albedo parameter must be in the range [0,1]!");
         }

        ScalarFloat sun_elevation = 0.5f * enoki::Pi<Float> - m_sun.elevation;

        if (sun_elevation < 0){
            Log(Error, "The sun is below the horizon -- this is not supported by the sky model.");
        }

        if (!m_spectral){
            for (size_t i = 0; i < m_channels; ++i){
                // m_state[i] = arhosek_rgb_skymodelstate_alloc_init
                // ((double)m_turbidity, (double)m_albedo[i], (double)sun_elevation);
                m_state[i] = arhosek_rgb_skymodelstate_alloc_init
                ((double)sun_elevation, (double)m_turbidity, (double)m_albedo[i]);
            }
        }
        else {
            for (size_t i = 0; i < m_channels; ++i){
                m_state[i] = arhosekskymodelstate_alloc_init
                ((double)m_turbidity, (double)m_albedo[i], (double)sun_elevation);
            }
            
        }
        std::cout << m_spectral << std::endl;
    }

    ~SkyEmitter(){
        for (size_t i = 0; i < m_channels; ++i){
            arhosekskymodelstate_free(m_state[i]);
        }
    }


    std::vector<ref<Object>> expand() const override {
        ref<Bitmap> bitmap = new Bitmap(Bitmap::PixelFormat::RGBA, Struct::Type::Float32, 
            ScalarVector2i(m_resolution, m_resolution / 2));

        ScalarPoint2f factor((2 * enoki::Pi<Float>) / bitmap->width(), 
            enoki::Pi<Float> / bitmap->height());

        for (size_t y = 0; y < bitmap->height(); ++y) {
            ScalarFloat theta = (y + .5f) * factor.y();
            Spectrum *target = (Spectrum *) bitmap->data() + y * bitmap->width();

            for (size_t x = 0; x < bitmap->width(); ++x) {
                ScalarFloat phi = (x + .5f) * factor.x();

                *target++ = get_sky_radiance(SphericalCoordinates(theta, phi));

            }
        }

        bitmap =  bitmap->convert(Bitmap::PixelFormat::RGB, struct_type_v<Float>, false);
        Properties props("envmap");

        props.set_pointer("bitmap", (uint8_t *) bitmap.get());
        ref<Object> texture = PluginManager::instance()->create_object<Base>(props).get();

        FileStream *fs = new FileStream("sky.exr", FileStream::ETruncReadWrite);
        bitmap->write(fs, Bitmap::FileFormat::OpenEXR);

        return {texture};
    }

    Spectrum get_sky_radiance(const SphericalCoordinates coords) const {

        ScalarFloat theta = coords.elevation / m_stretch;

        if (cos(theta) <= 0) {
            if (!m_extend)
                return Spectrum(0.0f);
            else
                theta = 0.5f * ek::Pi<Float> - 1e-4f;
        }

        ScalarFloat cos_gamma = cos(theta) * cos(m_sun.elevation)
            + sin(theta) * sin(m_sun.elevation)
            * cos(coords.azimuth - m_sun.azimuth);

        // Angle between the sun and the spherical coordinates in radians
        ScalarFloat gamma = ek::safe_acos(cos_gamma);

        Spectrum result;
        if (!m_spectral){
            for (size_t i = 0; i < m_channels; i++) {
                result[i] = (Float) (arhosek_tristim_skymodel_radiance(
                    m_state[i], (double)theta, (double)gamma, i) / 106.856980); // (sum of Spectrum::CIE_Y)
            }
        }else{ 
            for (size_t i = 0; i < m_channels; i++) {
                int j = (int) i;
                int wave_l = 320 + j * 40;
                result[i] = (Float) (arhosekskymodel_radiance(
                    m_state[i], (double)theta, (double)gamma, wave_l + 20.) / 106.856980); // (sum of Spectrum::CIE_Y)
            }

        }

        result = max(result, 0.f);

        if (m_extend){
            result *= smooth_step<Float>(0.f, 1.f, 2.f - 2.f * coords.elevation * ek::InvPi<Float>);
        }
        

        return result * m_scale;
    }

    // from constant.cpp
    Float pdf_direction(const Interaction3f &, const DirectionSample3f &ds,
                         Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointEvaluate, active);

        return warp::square_to_uniform_sphere_pdf(ds.d);
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
    /// Ground albedo
    ScalarColor3f m_albedo;
    /// temp variable for sample count
    unsigned int m_channels;
    /// State vector for the sky model
    ArHosekSkyModelState *m_state[11];
    /// Flag indicating whether a spectral sky is rendered
    bool m_spectral;
    


};

/// Implement RTTI data structures
MTS_IMPLEMENT_CLASS_VARIANT(SkyEmitter, Emitter)
MTS_EXPORT_PLUGIN(SkyEmitter, "Sky Emitter")
NAMESPACE_END(mitsuba)