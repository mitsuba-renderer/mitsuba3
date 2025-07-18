#include <mitsuba/core/fwd.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT class Microphone final : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sensor, m_film, m_to_world, m_needs_sample_2)
    MI_IMPORT_TYPES()

    Microphone(const Properties &props) : Base(props) {
        if (props.has_property("to_world")) {
            // if direction and origin are present but overridden by
            // to_world, they must still be marked as queried
            props.mark_queried("direction");
            props.mark_queried("origin");
        } else {
            if (props.has_property("direction") !=
                props.has_property("origin")) {
                Throw("If the sensor is specified through origin and direction "
                      "both values must be set!");
            }

            if (props.has_property("direction")) {
                ScalarPoint3f origin     = props.get<ScalarPoint3f>("origin");
                ScalarVector3f direction = props.get<ScalarVector3f>("direction");
                ScalarPoint3f target     = origin + direction;
                auto [up, unused]        = coordinate_system(dr::normalize(direction));

                m_to_world = ScalarTransform4f::look_at(origin, target, up);
                dr::make_opaque(m_to_world);
            }
        }

        m_kappa = props.get<ScalarFloat>("kappa", 0.f),

        m_needs_sample_2 = false;
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f & /*position_sample*/,
                                          const Point2f & sample3 /*aperture_sample*/,
                                          Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);
        Ray3f ray;
        ray.time = time;

        // 1. Sample spectrum
        //Spectrum wav_weight(1.f);

        ray.wavelengths = wavelength_sample;

        // 2. Set ray origin and direction
        ray.o = m_to_world.value().translation();
        auto sample_dir = warp::square_to_von_mises_fisher(sample3, m_kappa);
        ray.d = m_to_world.value() * sample_dir;
        Spectrum wav_weight(warp::square_to_von_mises_fisher_pdf(sample_dir, m_kappa));
        //Spectrum wav_weight(1.f);
        //ray.d = m_to_world.value() * warp::square_to_uniform_cone(sample3, m_kappa);
        ray.o += ray.d * math::RayEpsilon<Float>;

        return { ray, wav_weight };
    }

    std::pair<RayDifferential3f, Spectrum>
    sample_ray_differential(Float time,
                            Float wavelength_sample,
                            const Point2f & /*position_sample*/,
                            const Point2f & sample3 /*aperture_sample*/,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);
        RayDifferential3f ray;
        ray.time = time;

        // 1. Sample spectrum
        //Spectrum wav_weight(1.f);

        ray.wavelengths = wavelength_sample;

        // 2. Set ray origin and direction
        ray.o = m_to_world.value().translation();
        auto sample_dir = warp::square_to_von_mises_fisher(sample3, m_kappa);
        ray.d = m_to_world.value() * sample_dir;
        Spectrum wav_weight(warp::square_to_von_mises_fisher_pdf(sample_dir, m_kappa));
        // Spectrum wav_weight(1.f);
        //ray.d = m_to_world.value() * warp::square_to_uniform_cone(sample3, m_kappa);

        // 3. Set differentials; since the film size is always 1x1, we don't have differentials
        ray.o_x = ray.o_y = ray.o; // nevertheless leave nothing uninitialized
        ray.d_x = ray.d_y = ray.d;
        ray.has_differentials = false;

        return { ray, wav_weight };
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &, Mask /* active */) const override {        
        Transform4f trafo = m_to_world.value();
        Transform4f trafo_inv = trafo.inverse();

        DirectionSample3f ds = dr::zeros<PositionSample3f>();
        ds.p = trafo.translation(),
        ds.d = ds.p - it.p;

        Float dist_squared = dr::squared_norm(ds.d);
        ds.dist = dr::sqrt(dist_squared);
        ds.d /= ds.dist;

        ds.n = -ds.d;
        ds.delta = Mask(true);
        // dir in local space
        Vector3f d_local = trafo_inv * ds.n;
        // Set to sample point that would produce it. Is this rational? Is the microphone point parameterized?
        ds.uv = warp::von_mises_fisher_to_square(d_local, m_kappa);

        Spectrum wav_weight(warp::square_to_von_mises_fisher_pdf(d_local, m_kappa));

        return { ds, wav_weight };
    }

    ScalarBoundingBox3f bbox() const override {
        // Return an invalid bounding box
        return ScalarBoundingBox3f();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Microphone[" << std::endl
            << "  to_world = " << m_to_world << "," << std::endl
            << "  film = " << m_film << "," << std::endl
            << "]";
        return oss.str();
    }

    // TODO: traverse() needed?

    MI_DECLARE_CLASS()
protected:
    Float m_kappa;
};

MI_IMPLEMENT_CLASS_VARIANT(Microphone, Sensor)
MI_EXPORT_PLUGIN(Microphone, "Microphone");
NAMESPACE_END(mitsuba)
