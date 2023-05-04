#include <drjit/tensor.h>
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

/**!

.. _plugin-sensor-mdistant:

Multi distant radiance meter (:monosp:`mdistant`)
-------------------------------------------------

.. pluginparameters::

 * - directions
   - |string|
   - Comma-separated list of directions in which the sensors are pointing in
     world coordinates.
   - —

 * - target
   - |point| or nested :paramtype:`shape` plugin
   - *Optional.* Define the ray target sampling strategy.
     If this parameter is unset, ray target points are sampled uniformly on
     the cross section of the scene's bounding sphere.
     If a |point| is passed, rays will target it.
     If a shape plugin is passed, ray target points will be sampled from its
     surface.
   - —

 * - ray_offset
   - |float|
   - *Optional.* Define the ray origin offsetting policy.
     If this parameter is unset, ray origins are positioned at a far distance
     from the target. If a value is set, rays are offset by the corresponding
     distance.
   - —

This sensor plugin aggregates an arbitrary number of distant directional sensors
which records the spectral radiance leaving the scene in specified directions.
It is the aggregation of multiple :monosp:`distant` sensors.

By default, ray target points are sampled from the cross section of the scene's
bounding sphere. The ``target`` parameter can be set to restrict ray target
sampling to a specific subregion of the scene. The recorded radiance is averaged
over the targeted geometry.

Ray origins are positioned outside of the scene's geometry.

.. warning::

   If this sensor is used with a targeting strategy leading to rays not hitting
   the scene's geometry (*e.g.* default targeting strategy), it will pick up
   ambient emitter radiance samples (or zero values if no ambient emitter is
   defined). Therefore, it is almost always preferable to use a nondefault
   targeting strategy.

.. important::

   This sensor must be used with a film with size (`N`, 1), where `N` is the
   number of aggregated sensors, and is best used with a default :monosp:`box`
   reconstruction filter.
*/

template <typename Float, typename Spectrum>
class MultiDistantSensor final : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sensor, m_to_world, m_film, m_needs_sample_2,
                   m_needs_sample_3)
    MI_IMPORT_TYPES(Scene, Shape)

    using Matrix = dr::Matrix<Float, Transform4f::Size>;
    using Index = dr::int32_array_t<Float>;

    MultiDistantSensor(const Properties &props) : Base(props) {
        // Collect directions and set transforms accordingly
        if (props.has_property("to_world")) {
            Throw("This sensor is specified through a set of origin and "
                  "direction values and cannot use the to_world transform.");
        }

        std::vector<std::string> directions_str =
            string::tokenize(props.string("directions"), " ,");

        if (directions_str.size() % 3 != 0)
            Throw("Invalid specification! Number of parameters %s, is not a "
                  "multiple of three.",
                  directions_str.size());

        m_sensor_count = (size_t) directions_str.size() / 3;
        size_t width = m_sensor_count * 16;
        std::vector<ScalarFloat> buffer(width);

        // TODO: update transform storage loading with new TensorXf interface
        for (size_t i = 0; i < m_sensor_count; ++i) {
            size_t index = i * 3;

            ScalarVector3f direction =
                ScalarVector3f(std::stof(directions_str[index]),
                               std::stof(directions_str[index + 1]),
                               std::stof(directions_str[index + 2]));

            ScalarVector3f up;
            std::tie(up, std::ignore) = coordinate_system(direction);
            ScalarTransform4f transform =
                ScalarTransform4f::look_at(ScalarPoint3f{ 0.f, 0.f, 0.f },
                                           ScalarPoint3f(direction), up)
                    .matrix;
            memcpy(&buffer[i * 16], &transform, sizeof(ScalarFloat) * 16);
        }

        size_t shape[3] = { (size_t) m_sensor_count, 4, 4 };
        m_transforms = TensorXf(buffer.data(), 3, shape);

        // Collect ray offset value
        // Default is -1, overridden upon set_scene() call
        m_ray_offset = props.get<ScalarFloat>("ray_offset", -1);

        // Check film size
        ScalarPoint2i expected_size{ m_sensor_count, 1 };
        if (m_film->size() != expected_size)
            Throw("Film size must be [sensor_count, 1]. Expected %s, "
                  "got %s",
                  expected_size, m_film->size());

        // Check reconstruction filter radius
        if (m_film->rfilter()->radius() > 0.5f + math::RayEpsilon<Float>) {
            Log(Warn, "This sensor should be used with a reconstruction filter "
                      "with a radius of 0.5 or lower (e.g. default box)");
        }

        // Set ray target if relevant
        if (props.has_property("target")) {
            if (props.type("target") == Properties::Type::Array3f) {
                m_target_type = RayTargetType::Point;
                m_target_point = props.get<ScalarPoint3f>("target");
            } else if (props.type("target") == Properties::Type::Object) {
                // We assume it's a shape
                m_target_type = RayTargetType::Shape;
                auto obj = props.object("target");
                m_target_shape = dynamic_cast<Shape *>(obj.get());

                if (!m_target_shape)
                    Throw("Invalid parameter target, must be a Point3f or a "
                          "Shape.");
            } else {
                Throw("Unsupported 'target' parameter type");
            }
        } else {
            m_target_type = RayTargetType::None;
            Log(Debug, "No target specified.");
        }

        m_needs_sample_2 = true;
        m_needs_sample_3 = true;
    }

    void set_scene(const Scene *scene) override {
        m_bsphere = scene->bbox().bounding_sphere();
        m_bsphere.radius =
            dr::maximum(math::RayEpsilon<Float>,
                        m_bsphere.radius * (1.f + math::RayEpsilon<Float>) );

        if (m_ray_offset < 0.f)
            m_ray_offset = m_target_type == RayTargetType::None
                               ? m_bsphere.radius
                               : 2.f * m_bsphere.radius;
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &film_sample,
                                          const Point2f &aperture_sample,
                                          Mask active) const override {
        MI_MASK_ARGUMENT(active);

        Ray3f ray;
        ray.time = time;

        // Sample spectrum
        auto [wavelengths, wav_weight] =
            sample_wavelength<Float, Spectrum>(wavelength_sample);
        ray.wavelengths = wavelengths;

        // Select sub-sensor
        Int32 sensor_index(film_sample.x() * m_sensor_count);
        Index index(sensor_index);

        Matrix coefficients = dr::gather<Matrix>(m_transforms, index);
        Transform4f trafo(coefficients);

        // Set ray direction
        ray.d = trafo.transform_affine(Vector3f{ 0.f, 0.f, 1.f });

        // Sample target point and position ray origin
        Spectrum ray_weight = 0.f;

        if (m_target_type == RayTargetType::Point) {
            ray.o = m_target_point - ray.d * m_ray_offset;
            ray_weight = wav_weight;
        } else if (m_target_type == RayTargetType::Shape) {
            // Use area-based sampling of shape
            PositionSample3f ps =
                m_target_shape->sample_position(time, aperture_sample, active);
            ray.o = ps.p - ray.d * m_ray_offset;
            ray_weight = wav_weight / (ps.pdf * m_target_shape->surface_area());
        } else { // if (m_target_type == RayTargetType::None) {
            // Sample target uniformly on bounding sphere cross section
            Point2f offset =
                warp::square_to_uniform_disk_concentric(aperture_sample);
            Vector3f perp_offset =
                trafo.transform_affine(Vector3f{ offset.x(), offset.y(), 0.f });
            ray.o = m_bsphere.center + perp_offset * m_bsphere.radius -
                    ray.d * m_ray_offset;
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

        // Film pixels are actually independent, we don't have differentials
        ray.has_differentials = false;

        return { ray, ray_weight & active };
    }

    // This sensor does not occupy any particular region of space, return an
    // invalid bounding box
    ScalarBoundingBox3f bbox() const override { return ScalarBoundingBox3f(); }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "MultiDistantSensor[" << std::endl
            << "  transforms = " << string::indent(m_transforms.array()) << "," << std::endl
            << "  film = " << string::indent(m_film) << "," << std::endl;

        if (m_target_type == RayTargetType::Point)
            oss << "  target = " << m_target_point << "," << std::endl;
        else if (m_target_type == RayTargetType::Shape)
            oss << "  target = " << string::indent(m_target_shape) << "," << std::endl;
        else // if (m_target_type == RayTargetType::None)
            oss << "  target = none" << "," << std::endl;

        oss << "  ray_offset = " << m_ray_offset << std::endl << "]";

        return oss.str();
    }

    MI_DECLARE_CLASS()

protected:
    // Scene bounding sphere
    ScalarBoundingSphere3f m_bsphere;
    // Ray target type
    RayTargetType m_target_type;
    // Target shape if any
    ref<Shape> m_target_shape;
    // Target point if any
    Point3f m_target_point;
    // Transforms associated with each direction
    TensorXf m_transforms;
    // Number of directions
    size_t m_sensor_count;
    // Ray offset distance (-1 means "distant")
    ScalarFloat m_ray_offset;
};

MI_IMPLEMENT_CLASS_VARIANT(MultiDistantSensor, Sensor)
MI_EXPORT_PLUGIN(MultiDistantSensor, "MultiDistantSensor")
NAMESPACE_END(mitsuba)
