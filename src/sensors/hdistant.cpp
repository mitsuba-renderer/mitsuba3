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

// Forward declaration of specialized HemisphericalDistantSensor
template <typename Float, typename Spectrum, RayTargetType TargetType>
class HemisphericalDistantSensorImpl;

/**!

.. _sensor-hdistant:

Hemispherical distant radiancemeter sensor (:monosp:`hdistant`)
---------------------------------------------------------------

.. pluginparameters::

 * - to_world
   - |transform|
   - Sensor-to-world transformation matrix.
 * - target
   - |point| or nested :paramtype:`shape` plugin
   - *Optional.* Define the ray target sampling strategy.
     If this parameter is unset, ray target points are sampled uniformly on
     the cross section of the scene's bounding sphere.
     If a |point| is passed, rays will target it.
     If a shape plugin is passed, ray target points will be sampled from its
     surface.

This sensor plugin implements a distant directional sensor which records
radiation leaving the scene. It records the spectral radiance leaving the scene
in directions covering a hemisphere defined by its ``to_world`` parameter and
mapped to film coordinates. To some extent, it can be seen as the adjoint to
the ``envmap`` emitter.

The ``to_world`` transform is best set using a
:py:meth:`~mitsuba.core.Transform4f.look_at`. The default orientation covers a
hemisphere defined by the [0, 0, 1] direction, and the ``up`` film direction is
set to [0, 1, 0].

The following XML snippet creates a scene with a ``roughconductor``
surface illuminated by three ``directional`` emitter, each emitting in
a single RGB channel. A ``hdistant`` plugin with default orientation is
defined.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/sensor/sensor_hdistant_illumination_optimized.svg
   :caption: Example scene illumination setup
.. subfigend::
   :label: fig-hdistant-illumination

.. code:: xml

    <scene version="2.1.0">
        <sensor type="hdistant" id="hdistant">
            <transform name="to_world">
                <lookat origin="0, 0, 0" target="0, 0, 1" up="0, 1, 0"/>
            </transform>
            <sampler type="independent">
                <integer name="sample_count" value="3200"/>
            </sampler>
            <film type="hdrfilm">
                <integer name="width" value="32"/>
                <integer name="height" value="32"/>
                <string name="pixel_format" value="rgb"/>
                <string name="component_format" value="float32"/>
                <rfilter type="box"/>
            </film>
        </sensor>
        <integrator type="path"/>

        <emitter type="directional">
            <vector name="direction" x="1" y="0" z="-1"/>
            <rgb name="irradiance" value="1, 0, 0"/>
        </emitter>
        <emitter type="directional">
            <vector name="direction" x="1" y="1" z="-1"/>
            <rgb name="irradiance" value="0, 1, 0"/>
        </emitter>
        <emitter type="directional">
            <vector name="direction" x="0" y="1" z="-1"/>
            <rgb name="irradiance" value="0, 0, 1   "/>
        </emitter>

        <shape type="rectangle">
            <bsdf type="roughconductor"/>
        </shape>
    </scene>

The following figures show the recorded exitant radiance with the default film
orientation (left, ``up = [0,1,0]``) and with a rotated film (right,
``up = [1,1,0]``). Colored dots on the plots materialize emitter directions.
The orange arrow represents the ``up`` direction on the film.
Note that on the plots, the origin of pixel coordinates is taken at the bottom
left.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/sensor/sensor_hdistant_film_default_optimized.svg
   :caption: Default film orientation
.. subfigure:: ../../resources/data/docs/images/sensor/sensor_hdistant_film_rotated_optimized.svg
   :caption: Rotated film
.. subfigure:: ../../resources/data/docs/images/sensor/sensor_hdistant_default.svg
   :caption: Exitant radiance
.. subfigure:: ../../resources/data/docs/images/sensor/sensor_hdistant_rotated.svg
   :caption: Exitant radiance
.. subfigend::
   :label: fig-hdistant-film

By default, ray target points are sampled from the cross section of the scene's
bounding sphere. The ``target`` parameter can be set to restrict ray target
sampling to a specific subregion of the scene. The recorded radiance is averaged
over the targeted geometry.

Ray origins are positioned outside of the scene's geometry, such that it is
as if the sensor would be located at an infinite distance from the scene.

By default, ray target points are sampled from the cross section of the scene's
bounding sphere. The ``target`` parameter should be set to restrict ray target
sampling to a specific subregion of the scene using a flat surface. The recorded
radiance is averaged over the targeted geometry.

.. warning::

   * While setting ``target`` using any shape plugin is possible, only specific
     configurations will produce meaningful results. This is due to ray sampling
     method: when ``target`` is a shape, a point is sampled at its  surface,
     then shifted along the ``-direction`` vector by the diameter of the scene's
     bounding sphere, effectively positioning the ray origin outside of the
     geometry. The ray's weight is set to :math:`\frac{1}{A \, p}`, where
     :math:`A` is the shape's surface area and :math:`p` is the shape's position
     sampling PDF value. This weight definition is irrelevant when the sampled
     origin may corresponds to multiple points on the shape, *i.e.* when the
     sampled ray can intersect the target shape multiple times. From this
     follows that only flat surfaces should be used to set the ``target``
     parameter. Typically, one will rather use a ``rectangle`` or ``disk``
     shape.
   * If this sensor is used with a targeting strategy leading to rays not
     hitting the scene's geometry (*e.g.* default targeting strategy), it will
     pick up ambient emitter radiance samples (or zero values if no ambient
     emitter is defined). Therefore, it is almost always preferrable to use a
     nondefault targeting strategy.
*/

template <typename Float, typename Spectrum>
class HemisphericalDistantSensor final : public Sensor<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Sensor, m_to_world, m_film)
    MTS_IMPORT_TYPES(Scene, Shape)

    HemisphericalDistantSensor(const Properties &props) : Base(props) {
        // Check reconstruction filter radius
        if (m_film->reconstruction_filter()->radius() >
            0.5f + math::RayEpsilon<Float>) {
            Log(Warn, "This sensor is best used with a reconstruction filter "
                      "with a radius of 0.5 or lower (e.g. default box)");
        }

        // Store film sample location spacing for performance
        m_d.x() = 1.0f / m_film->size().x();
        m_d.y() = 1.0f / m_film->size().y();

        // Set ray target if relevant
        // Get target
        if (props.has_property("target")) {
            if (props.type("target") == Properties::Type::Array3f) {
                m_target_type  = RayTargetType::Point;
                m_target_point = props.get<ScalarPoint3f>("target");
            } else if (props.type("target") == Properties::Type::Object) {
                // We assume it's a shape
                m_target_type  = RayTargetType::Shape;
                auto obj       = props.object("target");
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
    }

    void set_scene(const Scene *scene) override {
        m_bsphere = scene->bbox().bounding_sphere();
        m_bsphere.radius =
            ek::max(math::RayEpsilon<Float>,
                    m_bsphere.radius * (1.f + math::RayEpsilon<Float>) );
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &film_sample,
                                          const Point2f &aperture_sample,
                                          Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        Ray3f ray;
        ray.time = time;

        // Sample spectrum
        auto [wavelengths, wav_weight] =
            sample_wavelength<Float, Spectrum>(wavelength_sample);
        ray.wavelengths = wavelengths;

        // Sample ray origin
        Spectrum ray_weight = 0.f;

        // Sample ray direction
        ray.d = -m_to_world.value().transform_affine(
            warp::square_to_uniform_hemisphere(film_sample));

        // Sample target point and position ray origin
        if (m_target_type == RayTargetType::Point) {
            ray.o      = m_target_point - 2.f * ray.d * m_bsphere.radius;
            ray_weight = wav_weight;
        } else if (m_target_type == RayTargetType::Shape) {
            // Use area-based sampling of shape
            PositionSample3f ps =
                m_target_shape->sample_position(time, aperture_sample, active);
            ray.o      = ps.p - 2.f * ray.d * m_bsphere.radius;
            ray_weight = wav_weight / (ps.pdf * m_target_shape->surface_area());
        } else { // if (m_target_type == RayTargetType::None) {
            // Sample target uniformly on bounding sphere cross section
            Point2f offset =
                warp::square_to_uniform_disk_concentric(aperture_sample);
            Vector3f perp_offset = m_to_world.value().transform_affine(
                Vector3f(offset.x(), offset.y(), 0.f));
            ray.o = m_bsphere.center + perp_offset * m_bsphere.radius -
                    ray.d * m_bsphere.radius;
            ray_weight = wav_weight;
        }

        return { ray, ray_weight & active };
    }

    // Ray sampling with spectral sampling removed
    std::pair<Vector3f, Point3f>
    sample_ray_dir_origin(Float time, const Point2f &film_sample,
                          const Point2f &aperture_sample, Mask active) const {
        MTS_MASK_ARGUMENT(active);

        // Sample ray direction
        Vector3f direction = -m_to_world.value().transform_affine(
            warp::square_to_uniform_hemisphere(film_sample));

        // Sample target point and position ray origin
        Point3f origin;

        if (m_target_type == RayTargetType::Point) {
            origin = m_target_point - 2.f * direction * m_bsphere.radius;
        } else if (m_target_type == RayTargetType::Shape) {
            // Use area-based sampling of shape
            PositionSample3f ps =
                m_target_shape->sample_position(time, aperture_sample, active);
            origin = ps.p - 2.f * direction * m_bsphere.radius;
        } else { // if (m_target_type == RayTargetType::None) {
            // Sample target uniformly on bounding sphere cross section
            Point2f offset =
                warp::square_to_uniform_disk_concentric(aperture_sample);
            Vector3f perp_offset = m_to_world.value().transform_affine(
                Vector3f(offset.x(), offset.y(), 0.f));
            origin = m_bsphere.center + perp_offset * m_bsphere.radius -
                     direction * m_bsphere.radius;
        }

        return { direction, origin };
    }

    std::pair<RayDifferential3f, Spectrum> sample_ray_differential(
        Float time, Float wavelength_sample, const Point2f &film_sample,
        const Point2f &aperture_sample, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        RayDifferential3f ray;
        Spectrum ray_weight;

        std::tie(ray, ray_weight) = sample_ray(
            time, wavelength_sample, film_sample, aperture_sample, active);

        // Compute ray differentials
        ray.has_differentials = true;

        Point2f film_sample_x{ film_sample.x() + m_d.x(), film_sample.y() };
        std::tie(ray.d_x, ray.o_x) =
            sample_ray_dir_origin(time, film_sample_x, aperture_sample, active);

        Point2f film_sample_y{ film_sample.x(), film_sample.y() + m_d.y() };
        std::tie(ray.d_y, ray.o_y) =
            sample_ray_dir_origin(time, film_sample_y, aperture_sample, active);

        return { ray, ray_weight & active };
    }

    // This sensor does not occupy any particular region of space, return an
    // invalid bounding box
    ScalarBoundingBox3f bbox() const override { return ScalarBoundingBox3f(); }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "HemisphericalDistantSensor[" << std::endl
            << "  to_world = " << string::indent(m_to_world) << "," << std::endl
            << "  film = " << string::indent(m_film) << "," << std::endl;

        if (m_target_type == RayTargetType::Point)
            oss << "  target = " << m_target_point << std::endl;
        else if (m_target_type == RayTargetType::Shape)
            oss << "  target = " << string::indent(m_target_shape) << std::endl;
        else // if (m_target_type == RayTargetType::None)
            oss << "  target = None" << std::endl;

        oss << "]";

        return oss.str();
    }

    MTS_DECLARE_CLASS()

protected:
    // Scene bounding sphere
    ScalarBoundingSphere3f m_bsphere;
    // Ray target type
    RayTargetType m_target_type;
    // Target shape if any
    ref<Shape> m_target_shape;
    // Target point if any
    Point3f m_target_point;
    // Spacing between two pixels in film coordinates
    ScalarPoint2f m_d;
};

MTS_IMPLEMENT_CLASS_VARIANT(HemisphericalDistantSensor, Sensor)
MTS_EXPORT_PLUGIN(HemisphericalDistantSensor, "HemisphericalDistantSensor")
NAMESPACE_END(mitsuba)
