#include <mitsuba/core/fwd.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _sensor-batch:

Batch sensor (:monosp:`batch`)
--------------------------------------------

.. pluginparameters::

 * - srf
   - |spectrum|
   - Sensor Response Function that defines the :ref:`spectral sensitivity <explanation_srf_sensor>`
     of the sensor (Default: :monosp:`none`)

This meta-sensor groups multiple sub-sensors so that they can be rendered
simultaneously. This reduces tracing overheads in applications that need to
render many viewpoints, particularly in the context of differentiable
rendering.

This plugin can currently only be used in path tracing-style integrators, and
it is incompatible with the particle tracer. The horizontal resolution of the
film associated with this sensor must be a multiple of the number of
sub-sensors.
*/

MI_VARIANT class BatchSensor final : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sensor, m_film, m_shape, sample_wavelengths)
    MI_IMPORT_TYPES(Shape)

    BatchSensor(const Properties &props) : Base(props) {
        for (auto [unused, o] : props.objects()) {
            ref<Base> sensor(dynamic_cast<Base *>(o.get()));

            if (sensor)
                m_sensors.push_back(sensor);
        }

        if (m_sensors.empty())
            Throw("BatchSensor: at least one child sensor must be specified!");

        ScalarPoint2u size = m_film->size();
        uint32_t sub_size = size.x() / (uint32_t) m_sensors.size();
        if (sub_size * (uint32_t) m_sensors.size() != size.x())
            Throw("BatchSensor: the horizontal resolution (currently %u) must "
                  "be divisible by the number of child sensors (%zu)!",
                  size.x(), m_sensors.size());

        for (size_t i = 0; i < m_sensors.size(); ++i) {
            m_sensors[i]->film()->set_size(ScalarPoint2u(sub_size, size.y()));
            m_sensors[i]->parameters_changed();
        }
    }

    std::pair<RayDifferential3f, Spectrum>
    sample_ray_differential(Float time, Float wavelength_sample,
                            const Point2f &position_sample,
                            const Point2f &aperture_sample,
                            Mask active) const override {

        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        UInt32 index(position_sample.x() * (ScalarFloat) m_sensors.size());
        RayDifferential3f result_1 = dr::zeros<RayDifferential3f>();
        Spectrum result_2 = dr::zeros<Spectrum>();

        for (size_t i = 0; i < m_sensors.size(); ++i) {
            Mask active_i = active && dr::eq(index, i);
            Point2f position_sample_2(
                dr::fmadd(position_sample.x(), (ScalarFloat) m_sensors.size(), -(ScalarFloat) i),
                position_sample.y()
            );
            auto [rv_1, rv_2] = m_sensors[i]->sample_ray_differential(
                time, wavelength_sample, position_sample_2, aperture_sample,
                active_i);
            result_1[active_i] = rv_1;
            result_2[active_i] = rv_2;
        }
        m_last_index = index;

        return { result_1, result_2 };
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample, Mask active) const override {
        DirectionSample3f result_1 = dr::zeros<DirectionSample3f>();
        Spectrum result_2 = dr::zeros<Spectrum>();

        for (size_t i = 0; i < m_sensors.size(); ++i) {
            Mask active_i = active && dr::eq(m_last_index, i);
            auto [rv_1, rv_2] =
                m_sensors[i]->sample_direction(it, sample, active_i);
            rv_1.uv.x() += i * m_sensors[i]->film()->size().x();
            result_1[active_i] = rv_1;
            result_2[active_i] = rv_2;
        }

        return { result_1, result_2 };
    }

    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        Float result = dr::zeros<Float>();
        for (size_t i = 0; i < m_sensors.size(); ++i) {
            Mask active_i = active && dr::eq(m_last_index, i);
            dr::masked(result, active_i) = m_sensors[i]->pdf_direction(it, ds, active_i);
        }
        return result;
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        Spectrum result = dr::zeros<Spectrum>();
        for (size_t i = 0; i < m_sensors.size(); ++i) {
            Mask active_i = active && dr::eq(m_last_index, i);
            result[active_i] = m_sensors[i]->eval(si, active_i);
        }
        return result;
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f result;
        for (size_t i = 0; i < m_sensors.size(); ++i)
            result.expand(m_sensors[i]->bbox());
        return result;
    }

    MI_DECLARE_CLASS()
private:
    std::vector<ref<Base>> m_sensors;
    mutable UInt32 m_last_index;
};

MI_IMPLEMENT_CLASS_VARIANT(BatchSensor, Sensor)
MI_EXPORT_PLUGIN(BatchSensor, "BatchSensor");
NAMESPACE_END(mitsuba)
