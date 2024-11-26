#include <mitsuba/core/fwd.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _sensor-radiancemeter:

Radiance meter (:monosp:`radiancemeter`)
----------------------------------------

.. pluginparameters::

 * - to_world
   - |transform|
   - Specifies an optional camera-to-world transformation.
     (Default: none (i.e. camera space = world space))

 * - origin
   - |point|
   - Location from which the sensor will be recording in world coordinates.
     Must be used with `direction`.

 * - direction
   - |vector|
   - Alternative (and exclusive) to `to_world`. Direction in which the
     sensor is pointing in world coordinates. Must be used with `origin`.

 * - srf
   - |spectrum|
   - Sensor Response Function that defines the :ref:`spectral sensitivity <explanation_srf_sensor>`
     of the sensor (Default: :monosp:`none`)

This sensor plugin implements a simple radiance meter, which measures
the incident power per unit area per unit solid angle along a
certain ray. It can be thought of as the limit of a standard
perspective camera as its field of view tends to zero.
This sensor is used with films of 1 by 1 pixels.

Such a sensor is useful for conducting virtual experiments and
testing the renderer for correctness.

By default, the sensor is located at the origin and performs
a measurement in the positive Z direction :monosp:`(0,0,1)`. This can
be changed by providing a custom :monosp:`to_world` transformation, or a pair
of :monosp:`origin` and :monosp:`direction` values. If both types of
transformation are specified, the :monosp:`to_world` transformation has higher
priority.

*/

MI_VARIANT class RadianceMeter final : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sensor, m_film, m_to_world, m_needs_sample_2,
                    m_needs_sample_3, sample_wavelengths)
    MI_IMPORT_TYPES()

    RadianceMeter(const Properties &props) : Base(props) {
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

        if (dr::all(m_film->size() != ScalarPoint2i(1, 1)))
            Throw("This sensor only supports films of size 1x1 Pixels!");

        if (m_film->rfilter()->radius() >
            0.5f + math::RayEpsilon<Float>)
            Log(Warn, "This sensor should be used with a reconstruction filter "
                      "with a radius of 0.5 or lower (e.g. default box)");

        m_needs_sample_2 = false;
        m_needs_sample_3 = false;
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f & /*position_sample*/,
                                          const Point2f & /*aperture_sample*/,
                                          Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);
        Ray3f ray;
        ray.time = time;

        // 1. Sample spectrum
        auto [wavelengths, wav_weight] =
            sample_wavelengths(dr::zeros<SurfaceInteraction3f>(),
                               wavelength_sample,
                               active);
        ray.wavelengths = wavelengths;

        // 2. Set ray origin and direction
        ray.o = m_to_world.value().transform_affine(Point3f(0.f, 0.f, 0.f));
        ray.d = m_to_world.value().transform_affine(Vector3f(0.f, 0.f, 1.f));
        ray.o += ray.d * math::RayEpsilon<Float>;

        return { ray, wav_weight };
    }

    std::pair<RayDifferential3f, Spectrum>
    sample_ray_differential(Float time, Float wavelength_sample,
                            const Point2f & /*position_sample*/,
                            const Point2f & /*aperture_sample*/,
                            Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);
        RayDifferential3f ray = dr::zeros<RayDifferential3f>();
        ray.time = time;

        // 1. Sample spectrum
        auto [wavelengths, wav_weight] =
            sample_wavelengths(dr::zeros<SurfaceInteraction3f>(),
                               wavelength_sample,
                               active);
        ray.wavelengths = wavelengths;

        // 2. Set ray origin and direction
        ray.o = m_to_world.value().transform_affine(Point3f(0.f, 0.f, 0.f));
        ray.d = m_to_world.value().transform_affine(Vector3f(0.f, 0.f, 1.f));
        ray.o += ray.d * math::RayEpsilon<Float>;

        // 3. Set differentials; since the film size is always 1x1, we don't
        //    have differentials
        ray.has_differentials = false;

        return { ray, wav_weight };
    }

    ScalarBoundingBox3f bbox() const override {
        // Return an invalid bounding box
        return ScalarBoundingBox3f();
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "RadianceMeter[" << std::endl
            << "  to_world = " << m_to_world << "," << std::endl
            << "  film = " << m_film << "," << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(RadianceMeter, Sensor)
MI_EXPORT_PLUGIN(RadianceMeter, "RadianceMeter");
NAMESPACE_END(mitsuba)
