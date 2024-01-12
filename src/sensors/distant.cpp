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
class DistantSensorImpl;

/**!

.. _sensor-distant:

Distant radiancemeter sensor (:monosp:`distant`)
------------------------------------------------

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

 * - srf
   - |spectrum|
   - Sensor Response Function that defines the :ref:`spectral sensitivity <explanation_srf_sensor>`
     of the sensor (Default: :monosp:`none`)

This sensor plugin implements a distant directional sensor which records
radiation leaving the scene in a given direction. It records the spectral
radiance leaving the scene in the specified direction. It is the adjoint to the
``directional`` emitter.

By default, ray target points are sampled from the cross section of the scene's
bounding sphere. The ``target`` parameter can be set to restrict ray target
sampling to a specific subregion of the scene. The recorded radiance is averaged
over the targeted geometry.

Ray origins are positioned outside of the scene's geometry.

.. warning::

   If this sensor is used with a targeting strategy leading to rays not hitting
   the scene's geometry (*e.g.* default targeting strategy), it will pick up
   ambient emitter radiance samples (or zero values if no ambient emitter is
   defined). Therefore, it is almost always preferable to use a non-default
   targeting strategy.
*/

template <typename Float, typename Spectrum>
class DistantSensor final : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sensor, m_to_world, m_film)
    MI_IMPORT_TYPES(Scene, Shape)

    DistantSensor(const Properties &props) : Base(props), m_props(props) {

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
    }

    // This must be implemented. However, it won't be used in practice:
    // instead, DistantSensorImpl::bbox() is used when the plugin is
    // instantiated.
    ScalarBoundingBox3f bbox() const override { return ScalarBoundingBox3f(); }


    template <RayTargetType TargetType>
    using Impl = DistantSensorImpl<Float, Spectrum, TargetType>;

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
class DistantSensorImpl final : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sensor, m_to_world, m_film, sample_wavelengths)
    MI_IMPORT_TYPES(Scene, Shape)

    DistantSensorImpl(const Properties &props) : Base(props) {
        // Check film size
        if (dr::all(m_film->size() != ScalarPoint2i(1, 1)))
            Throw("This sensor only supports films of size 1x1 Pixels!");

        // Check reconstruction filter radius
        if (m_film->rfilter()->radius() >
            0.5f + math::RayEpsilon<Float>) {
            Log(Warn, "This sensor should be used with a reconstruction filter "
                      "with a radius of 0.5 or lower (e.g. default box)");
        }

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
    }

    void set_scene(const Scene *scene) override {
        m_bsphere = scene->bbox().bounding_sphere();
        m_bsphere.radius =
            dr::maximum(math::RayEpsilon<Float>,
                    m_bsphere.radius * (1.f + math::RayEpsilon<Float>) );
    }

    std::pair<Ray3f, Spectrum>
    sample_ray(Float time, Float wavelength_sample,
                    const Point2f & /*film_sample*/,
                    const Point2f &aperture_sample, Mask active) const override {
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
        if constexpr (TargetType == RayTargetType::Point) {
            ray.o = m_target_point - 2.f * ray.d * m_bsphere.radius;
            ray_weight = wav_weight;
        } else if constexpr (TargetType == RayTargetType::Shape) {
            // Use area-based sampling of shape
            PositionSample3f ps =
                m_target_shape->sample_position(time, aperture_sample, active);
            ray.o = ps.p - 2.f * ray.d * m_bsphere.radius;
            ray_weight = wav_weight / (ps.pdf * m_target_shape->surface_area());
        } else { // if constexpr (TargetType == RayTargetType::None) {
            // Sample target uniformly on bounding sphere cross section
            Point2f offset =
                warp::square_to_uniform_disk_concentric(aperture_sample);
            Vector3f perp_offset =
                m_to_world.value().transform_affine(Vector3f(offset.x(), offset.y(), 0.f));
            ray.o = m_bsphere.center + perp_offset * m_bsphere.radius - ray.d * m_bsphere.radius;
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

        // Since the film size is always 1x1, we don't have differentials
        ray.has_differentials = false;

        return { ray, ray_weight & active };
    }

    // This sensor does not occupy any particular region of space, return an
    // invalid bounding box
    ScalarBoundingBox3f bbox() const override { return ScalarBoundingBox3f(); }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "DistantSensor[" << std::endl
            << "  to_world = " << m_to_world << "," << std::endl
            << "  film = " << m_film << "," << std::endl;

        if constexpr (TargetType == RayTargetType::Point)
            oss << "  target = " << m_target_point << std::endl;
        else if constexpr (TargetType == RayTargetType::Shape)
            oss << "  target = " << m_target_shape << std::endl;
        else // if constexpr (TargetType == RayTargetType::None)
            oss << "  target = none" << std::endl;

        oss << "]";

        return oss.str();
    }

    MI_DECLARE_CLASS()

protected:
    ScalarBoundingSphere3f m_bsphere;
    ref<Shape> m_target_shape;
    Point3f m_target_point;
};

MI_IMPLEMENT_CLASS_VARIANT(DistantSensor, Sensor)
MI_EXPORT_PLUGIN(DistantSensor, "DistantSensor")

NAMESPACE_BEGIN(detail)
template <RayTargetType TargetType>
constexpr const char *distant_sensor_class_name() {
    if constexpr (TargetType == RayTargetType::Shape) {
        return "DistantSensor_Shape";
    } else if constexpr (TargetType == RayTargetType::Point) {
        return "DistantSensor_Point";
    } else if constexpr (TargetType == RayTargetType::None) {
        return "DistantSensor_NoTarget";
    }
}
NAMESPACE_END(detail)

template <typename Float, typename Spectrum, RayTargetType TargetType>
Class *DistantSensorImpl<Float, Spectrum, TargetType>::m_class = new Class(
    detail::distant_sensor_class_name<TargetType>(), "Sensor",
    ::mitsuba::detail::get_variant<Float, Spectrum>(), nullptr, nullptr);

template <typename Float, typename Spectrum, RayTargetType TargetType>
const Class *DistantSensorImpl<Float, Spectrum, TargetType>::class_() const {
    return m_class;
}

NAMESPACE_END(mitsuba)
