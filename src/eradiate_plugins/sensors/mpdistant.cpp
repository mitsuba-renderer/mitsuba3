#include <mitsuba/core/bbox.h>
#include <mitsuba/core/bsphere.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

enum class RayTargetType { Shape, Point, None };

// Forward declaration of specialized DistantSensor
template <typename Float, typename Spectrum, RayTargetType TargetType>
class MultiPixelDistantSensorImpl;

/**!

.. _sensor-mpdistant:

Multi-pixel distant radiancemeter sensor (:monosp:`mpdistant`)
--------------------------------------------------------------

.. pluginparameters::

 * - to_world
   - |transform|
   - Sensor-to-world transformation matrix.

 * - direction
   - |vector|
   - Alternative (and exclusive) to ``to_world``. Direction orienting the
     sensor's reference hemisphere.

 * - target
   - |point| or nested :paramtype:`shape` plugin
   - *Optional.* Define the ray target sampling strategy.
     If this parameter is unset, ray target points are sampled uniformly on
     the cross section of the scene's bounding sphere.
     If a |point| is passed, rays will target it.
     If a shape plugin is passed, ray target points will be sampled from its
     surface.

 * - target_radius
   - |float|
   - *Optional.* If a point target is used, setting this parameter to a positive
     value will turn the sensor into a distant radiometer with a fixed field of
     view defined as the cross section of a sphere of radius ``target_radius``
     and centered at ``target``. Otherwise, the single point ``target`` is
     targeted.

 * - srf
   - |spectrum|
   - Sensor Response Function that defines the
     :ref:`spectral sensitivity <explanation_srf_sensor>` of the sensor
     (Default: :monosp:`none`)

This sensor plugin implements a distant directional sensor which records
radiation leaving the scene in a given direction. It records the spectral
radiance leaving the scene in the specified direction. In its default version,
it is the adjoint to the ``directional`` emitter.

By default, ray target points are sampled from the cross section of the scene's
bounding sphere. The ``target`` parameter can be set to restrict ray target
sampling to a specific subregion of the scene.

Ray origins are positioned outside of the scene's geometry.

If the film size is larger than 1×1, film coordinates are mapped to the (u,v)
coordinates of the target shape.

.. warning::

   If this sensor is used with a targeting strategy leading to rays not hitting
   the scene's geometry (*e.g.* default targeting strategy), it will pick up
   ambient emitter radiance samples (or zero values if no ambient emitter is
   defined). Therefore, it is almost always preferable to use a non-default
   targeting strategy.
*/

template <typename Float, typename Spectrum>
class MultiPixelDistantSensor final : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sensor, m_to_world, m_film)
    MI_IMPORT_TYPES(Scene, Shape)

    MultiPixelDistantSensor(const Properties &props) : Base(props), m_props(props) {

        // Get target
        if (props.has_property("target")) {
            if (props.type("target") == Properties::Type::Array3f) {
                props.get<ScalarPoint3f>("target");
                m_target_type = RayTargetType::Point;
            } else if (props.type("target") == Properties::Type::Object) {
                // We assume it's a shape
                m_target_type = RayTargetType::Shape;
            } else {
                Throw("Unsupported 'target' parameter type");
            }
        } else {
            m_target_type = RayTargetType::None;
        }

        props.mark_queried("direction");
        props.mark_queried("to_world");
        props.mark_queried("target");
        props.mark_queried("target_radius");
        props.mark_queried("ray_offset");
    }

    // This must be implemented. However, it won't be used in practice:
    // instead, MultiPixelDistantSensorImpl::bbox() is used when the plugin is
    // instantiated.
    ScalarBoundingBox3f bbox() const override { return ScalarBoundingBox3f(); }


    template <RayTargetType TargetType>
    using Impl = MultiPixelDistantSensorImpl<Float, Spectrum, TargetType>;

    // Recursively expand into an implementation specialized to the target
    // specification.
    std::vector<ref<Object>> expand() const override {
        ref<Object> result;
        switch (m_target_type) {
            case RayTargetType::Shape:
                result = (Object *) new Impl<RayTargetType::Shape>(m_props);
                break;
            case RayTargetType::Point:
                result = (Object *) new Impl<RayTargetType::Point>(m_props);
                break;
            case RayTargetType::None:
                result = (Object *) new Impl<RayTargetType::None>(m_props);
                break;
            default:
                Throw("Unsupported ray target type!");
        }
        return { result };
    }

    MI_DECLARE_CLASS()

protected:
    Properties m_props;
    RayTargetType m_target_type;
};

template <typename Float, typename Spectrum, RayTargetType TargetType>
class MultiPixelDistantSensorImpl final : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sensor, m_to_world, m_film, sample_wavelengths,
                   m_needs_sample_2)
    MI_IMPORT_TYPES(Scene, Shape)

    MultiPixelDistantSensorImpl(const Properties &props) : Base(props) {
        // Compute transform, possibly based on direction parameter
        if (props.has_property("direction")) {
            if (props.has_property("to_world")) {
                Throw("Only one of the parameters 'direction' and 'to_world'"
                      "can be specified at the same time!'");
            }

            ScalarVector3f direction(normalize(props.get<ScalarVector3f>("direction")));
            ScalarVector3f up;

            std::tie(std::ignore, up) = coordinate_system(direction);

            m_to_world = ScalarTransform4f::look_at(
                ScalarPoint3f(0.0f), ScalarPoint3f(direction), up);
        }

        // Collect ray offset value
        // Default is -1, overridden upon set_scene() call
        m_ray_offset = props.get<ScalarFloat>("ray_offset", -1);

        // Collect target cross section if relevant (i.e. if point target is defined)
        m_target_radius = props.get<ScalarFloat>("target_radius", -1);

        // Set ray target if relevant
        if constexpr (TargetType == RayTargetType::Point) {
            m_target_point = props.get<ScalarPoint3f>("target");
        } else if constexpr (TargetType == RayTargetType::Shape) {
            auto obj       = props.object("target");
            m_target_shape = dynamic_cast<Shape *>(obj.get());

            if (!m_target_shape)
                Throw(
                    "Invalid parameter target, must be a Point3f or a Shape.");
        } else {
            Log(Debug, "No target specified.");
        }

        m_needs_sample_2 = true;
    }

    void set_scene(const Scene *scene) override {
        m_bsphere = scene->bbox().bounding_sphere();
        m_bsphere.radius =
            dr::maximum(math::RayEpsilon<Float>,
                    m_bsphere.radius * (1.f + math::RayEpsilon<Float>));
        if (m_ray_offset == -1)
            m_ray_offset = 2.f * m_bsphere.radius;
    }

    std::pair<Ray3f, Spectrum>
    sample_ray(Float time, Float wavelength_sample,
                    const Point2f &film_sample,
                    const Point2f &/*aperture_sample*/, Mask active) const override {
        MI_MASK_ARGUMENT(active);

        Ray3f ray;
        ray.time = time;

        // Sample spectrum
        auto [wavelengths, wav_weight] =
            sample_wavelengths(dr::zeros<SurfaceInteraction3f>(),
                               wavelength_sample,
                               active);
        ray.wavelengths = wavelengths;

        // Sample ray origin
        Spectrum ray_weight = 0.f;

        // Set ray direction
        ray.d = m_to_world.value().transform_affine(Vector3f{ 0.f, 0.f, 1.f });

        // Sample target point and position ray origin
        if constexpr (TargetType == RayTargetType::Shape) {
            // Use area-based sampling of shape
            PositionSample3f ps =
                m_target_shape->sample_position(time, film_sample, active);
            ray.o = ps.p - ray.d * m_ray_offset;
            ray_weight = wav_weight / (ps.pdf * m_target_shape->surface_area());
        } else {
            if constexpr (TargetType == RayTargetType::Point) {
                if (m_target_radius < 0.f)
                    ray.o = m_target_point - ray.d * m_ray_offset;
                else {
                    Point2f offset =
                        warp::square_to_uniform_disk_concentric(film_sample);
                    Vector3f perp_offset =
                        m_to_world.value().transform_affine(Vector3f(offset.x(), offset.y(), 0.f));
                    ray.o = m_target_point + perp_offset * m_target_radius - ray.d * m_ray_offset;
                }
            } else { // if constexpr (TargetType == RayTargetType::None
                // Sample target uniformly on bounding sphere cross section
                Point2f offset =
                    warp::square_to_uniform_disk_concentric(film_sample);
                Vector3f perp_offset =
                    m_to_world.value().transform_affine(Vector3f(offset.x(), offset.y(), 0.f));
                ray.o = m_bsphere.center + perp_offset * m_bsphere.radius - ray.d * m_ray_offset;
            }
            ray_weight = wav_weight;
        }

        return { ray, ray_weight & active };
    }

    std::pair<RayDifferential3f, Spectrum> sample_ray_differential(
        Float time, Float wavelength_sample, const Point2f &film_sample,
        const Point2f &aperture_sample, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        RayDifferential3f ray;
        Spectrum ray_weight;

        std::tie(ray, ray_weight) = sample_ray(
            time, wavelength_sample, film_sample, aperture_sample, active);

        // No differentials [TODO: Should we add this?]
        ray.has_differentials = false;

        return { ray, ray_weight & active };
    }

    // This sensor does not occupy any particular region of space, return an
    // invalid bounding box
    ScalarBoundingBox3f bbox() const override { return ScalarBoundingBox3f(); }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "MultiPixelDistantSensor[" << std::endl
            << "  to_world = " << string::indent(m_to_world, 13) << "," << std::endl
            << "  film = " << string::indent(m_film) << "," << std::endl
            << "  ray_offset = " << m_ray_offset << "," << std::endl;

        if constexpr (TargetType == RayTargetType::Point)
            oss << "  target = " << m_target_point << "," << std::endl
                << "  target_radius = " << m_target_radius << std::endl;
        else if constexpr (TargetType == RayTargetType::Shape)
            oss << "  target = " << string::indent(m_target_shape) << std::endl;
        else // if constexpr (TargetType == RayTargetType::None)
            oss << "  target = none" << "," << std::endl
                << "  bsphere = " << string::indent(m_bsphere) << std::endl;

        oss << "]";

        return oss.str();
    }

    MI_DECLARE_CLASS()

protected:
    // Scene bounding sphere
    ScalarBoundingSphere3f m_bsphere;
    // Target shape if any
    ref<Shape> m_target_shape;
    // Target point if any
    Point3f m_target_point;
    // Target cross section radius when using a point target (-1 means "target a single point")
    ScalarFloat m_target_radius;
    // Ray offset distance (-1 means "distant")
    ScalarFloat m_ray_offset;
};

MI_IMPLEMENT_CLASS_VARIANT(MultiPixelDistantSensor, Sensor)
MI_EXPORT_PLUGIN(MultiPixelDistantSensor, "MultiPixelDistantSensor")

NAMESPACE_BEGIN(detail)
template <RayTargetType TargetType>
constexpr const char *distant_sensor_class_name() {
    if constexpr (TargetType == RayTargetType::Shape) {
        return "MultiPixelDistantSensor_Shape";
    } else if constexpr (TargetType == RayTargetType::Point) {
        return "MultiPixelDistantSensor_Point";
    } else if constexpr (TargetType == RayTargetType::None) {
        return "MultiPixelDistantSensor_NoTarget";
    }
}
NAMESPACE_END(detail)

template <typename Float, typename Spectrum, RayTargetType TargetType>
Class *MultiPixelDistantSensorImpl<Float, Spectrum, TargetType>::m_class = new Class(
    detail::distant_sensor_class_name<TargetType>(), "Sensor",
    ::mitsuba::detail::get_variant<Float, Spectrum>(), nullptr, nullptr);

template <typename Float, typename Spectrum, RayTargetType TargetType>
const Class *MultiPixelDistantSensorImpl<Float, Spectrum, TargetType>::class_() const {
    return m_class;
}

NAMESPACE_END(mitsuba)
