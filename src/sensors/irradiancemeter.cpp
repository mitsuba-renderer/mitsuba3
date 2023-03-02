#include <mitsuba/core/fwd.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _sensor-irradiancemeter:

Irradiance meter (:monosp:`irradiancemeter`)
--------------------------------------------

.. pluginparameters::

 * - srf
   - |spectrum|
   - Sensor Response Function that defines the :ref:`spectral sensitivity <explanation_srf_sensor>`
     of the sensor (Default: :monosp:`none`)

This sensor plugin implements an irradiance meter, which measures
the incident power per unit area over a shape which it is attached to.
This sensor is used with films of 1 by 1 pixels.

If the irradiance meter is attached to a mesh-type shape, it will measure the
irradiance over all triangles in the mesh.

This sensor is not instantiated on its own but must be defined as a child
object to a shape in a scene. To create an irradiance meter,
simply instantiate the desired sensor shape and specify an
:monosp:`irradiancemeter` instance as its child:

.. tabs::
    .. code-tab:: xml
        :name: sphere-meter

        <shape type="sphere">
            <sensor type="irradiancemeter">
                <!-- film -->
            </sensor>
        </shape>

    .. code-tab:: python

        'type': 'sphere',
        'sensor': {
            'type': 'irradiancemeter'
            'film': {
                # ...
            }
        }
*/

MI_VARIANT class IrradianceMeter final : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sensor, m_film, m_shape, sample_wavelengths)
    MI_IMPORT_TYPES(Shape)

    IrradianceMeter(const Properties &props) : Base(props) {
        if (props.has_property("to_world"))
            Throw("Found a 'to_world' transformation -- this is not allowed. "
                  "The irradiance meter inherits this transformation from its parent "
                  "shape.");

        if (m_film->rfilter()->radius() > .5f + math::RayEpsilon<Float>)
            Log(Warn, "This sensor should only be used with a reconstruction filter"
               "of radius 0.5 or lower (e.g. default 'box' filter)");
    }

    std::pair<RayDifferential3f, Spectrum>
    sample_ray_differential(Float time, Float wavelength_sample,
                            const Point2f & sample2,
                            const Point2f & sample3,
                            Mask active) const override {

        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        // 1. Sample spatial component
        PositionSample3f ps = m_shape->sample_position(time, sample2, active);

        // 2. Sample directional component
        Vector3f local = warp::square_to_cosine_hemisphere(sample3);

        // 3. Sample spectrum
        auto [wavelengths, wav_weight] =
            sample_wavelengths(dr::zeros<SurfaceInteraction3f>(),
                               wavelength_sample,
                               active);

        Vector3f d = Frame3f(ps.n).to_world(local);
        Point3f o = ps.p + d * math::RayEpsilon<Float>;

        return {
            RayDifferential3f(o, d, time, wavelengths),
            depolarizer<Spectrum>(wav_weight) * dr::Pi<ScalarFloat>
        };
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample, Mask active) const override {
        return { m_shape->sample_direction(it, sample, active), dr::Pi<ScalarFloat> };
    }

    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        return m_shape->pdf_direction(it, ds, active);
    }

    Spectrum eval(const SurfaceInteraction3f &/*si*/, Mask /*active*/) const override {
        return dr::Pi<ScalarFloat> / m_shape->surface_area();
    }

    ScalarBoundingBox3f bbox() const override { return m_shape->bbox(); }

    std::string to_string() const override {
        using string::indent;

        std::ostringstream oss;
        oss << "IrradianceMeter[" << std::endl << "  surface_area = ";

        if (m_shape)
            oss << m_shape->surface_area();
        else
            oss << " <no shape attached!>";
        oss << "," << std::endl;

        oss << "  film = " << indent(m_film) << "," << std::endl << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
};

MI_IMPLEMENT_CLASS_VARIANT(IrradianceMeter, Sensor)
MI_EXPORT_PLUGIN(IrradianceMeter, "IrradianceMeter");
NAMESPACE_END(mitsuba)
