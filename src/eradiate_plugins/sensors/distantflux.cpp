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

.. _plugin-sensor-distantflux:

Distant fluxmeter sensor (:monosp:`distantflux`)
------------------------------------------------

.. pluginparameters::

 * - to_world
   - |transform|
   - Sensor-to-world transformation matrix.
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

This sensor plugin implements a distant sensor which records the radiative flux
density leaving the scene (in W/m², scaled by scene unit length). It covers a
hemisphere defined by its ``to_world`` parameter and mapped to film coordinates.

The ``to_world`` transform is best set using a
:py:meth:`~mitsuba.core.Transform4f.look_at`. The default orientation covers a
hemisphere defined by the [0, 0, 1] direction, and the ``up`` film direction is
set to [0, 1, 0].

Using a 1x1 film with a stratified sampler is recommended.
A different film size can also be used. In that case, the exitant flux is given
by the sum of all pixel values.

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
*/

template <typename Float, typename Spectrum>
class DistantFluxSensor final : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sensor, m_to_world, m_film)
    MI_IMPORT_TYPES(Scene, Shape)

    DistantFluxSensor(const Properties &props) : Base(props) {
        // Check reconstruction filter radius
        if (m_film->rfilter()->radius() > 0.5f + math::RayEpsilon<Float>) {
            Log(Warn, "This sensor is best used with a reconstruction filter "
                      "with a radius of 0.5 or lower (e.g. default box)");
        }
        m_npixels = m_film->size().x() * m_film->size().y();

        // Collect ray offset value
        // Default is -1, overridden upon set_scene() call
        m_ray_offset = props.get<ScalarFloat>("ray_offset", -1);

        // Set ray target if relevant
        // Get target
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

        // Set reference surface normal (in world coords)
        ScalarTransform4f trafo =
            props.get<ScalarTransform4f>("to_world", ScalarTransform4f());
        m_reference_normal =
            trafo.transform_affine(ScalarVector3f{ 0.f, 0.f, 1.f });
    }

    void set_scene(const Scene *scene) override {
        m_bsphere = scene->bbox().bounding_sphere();
        m_bsphere.radius =
            dr::maximum(math::RayEpsilon<Float>,
                        m_bsphere.radius * (1.f + math::RayEpsilon<Float>));

        if (m_ray_offset < 0.f)
            m_ray_offset = m_target_type == RayTargetType::None
                               ? m_bsphere.radius
                               : 2.f * m_bsphere.radius;
    }

    std::pair<Ray3f, Spectrum> sample_ray_impl(Float time,
                                               Float wavelength_sample,
                                               const Point2f &film_sample,
                                               const Point2f &aperture_sample,
                                               Mask active) const {
        MI_MASK_ARGUMENT(active);

        Ray3f ray;
        ray.time = time;

        // Sample spectrum
        auto [wavelengths, wav_weight] =
            sample_wavelength<Float, Spectrum>(wavelength_sample);
        ray.wavelengths = wavelengths;

        // Sample ray direction
        ray.d = -m_to_world.value().transform_affine(
            warp::square_to_uniform_hemisphere(film_sample));

        // Sample target point and position ray origin
        Spectrum ray_weight =
            dot(-ray.d, m_reference_normal) /
            (warp::square_to_uniform_hemisphere_pdf(ray.d) * m_npixels);

        if (m_target_type == RayTargetType::Point) {
            ray.o = m_target_point - ray.d * m_ray_offset;
            ray_weight *= wav_weight;
        } else if (m_target_type == RayTargetType::Shape) {
            // Use area-based sampling of shape
            PositionSample3f ps =
                m_target_shape->sample_position(time, aperture_sample, active);
            ray.o = ps.p - ray.d * m_ray_offset;
            ray_weight *=
                wav_weight / (ps.pdf * m_target_shape->surface_area());
        } else { // if (m_target_type == RayTargetType::None) {
            // Sample target uniformly on bounding sphere cross section defined
            // by reference surface normal
            Point2f offset =
                warp::square_to_uniform_disk_concentric(aperture_sample);
            Vector3f perp_offset = m_to_world.value().transform_affine(
                Vector3f{ offset.x(), offset.y(), 0.f });
            ray.o = m_bsphere.center + perp_offset * m_bsphere.radius -
                    ray.d * m_ray_offset;
            ray_weight *= wav_weight;
        }

        return { ray, ray_weight & active };
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &film_sample,
                                          const Point2f &aperture_sample,
                                          Mask active) const override {
        MI_MASK_ARGUMENT(active);

        auto [ray, ray_weight] = sample_ray_impl(
            time, wavelength_sample, film_sample, aperture_sample, active);

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
        oss << "DistantFluxSensor[" << std::endl
            << "  reference_normal = " << m_reference_normal << "," << std::endl
            << "  to_world = " << string::indent(m_to_world) << "," << std::endl
            << "  film = " << string::indent(m_film) << "," << std::endl;

        if (m_target_type == RayTargetType::Point)
            oss << "  target = " << m_target_point << "," << std::endl;
        else if (m_target_type == RayTargetType::Shape)
            oss << "  target = " << string::indent(m_target_shape) << "," << std::endl;
        else // if (m_target_type == RayTargetType::None)
            oss << "  target = None" << "," << std::endl;

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
    // Normal to reference surface
    Vector3f m_reference_normal;
    // Total number of film pixels
    size_t m_npixels;
    // Ray offset distance (-1 means "distant")
    ScalarFloat m_ray_offset;
};

MI_IMPLEMENT_CLASS_VARIANT(DistantFluxSensor, Sensor)
MI_EXPORT_PLUGIN(DistantFluxSensor, "DistantFluxSensor")
NAMESPACE_END(mitsuba)
