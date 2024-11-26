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
sub-sensors. In addition, all of the sub-sensors' films, samplers and shutter
timings are typically ignored and superseded by the film, sampler and shutter
timings specified for the `batch` sensor itself.

.. tabs::
    .. code-tab:: xml
        :name: batch-sensor

        <sensor type="batch">
            <!-- Two perpendicular viewing directions -->
            <sensor name="sensor_1" type="perspective">
                <float name="fov" value="45"/>
                <transform name="to_world">
                    <lookat origin="0, 0, 1" target="0, 0, 0" up="0, 1, 0"/>
                </transform>
            </sensor>
            <sensor name="sensor_2" type="perspective">
                <float name="fov" value="45"/>
                <transform name="to_world">
                    <look_at origin="1, 0, 0" target="1, 2, 1" up="0, 1, 0"/>
                </transform>
            </sensor>
            <!-- film -->
            <!-- sampler -->
        </sensor>

    .. code-tab:: python

        'type': 'batch',
        # Two perpendicular viewing directions
        'sensor1': {
            'type': 'perspective',
            'fov': 45,
            'to_world': mi.ScalarTransform4f().look_at(
                origin=[0, 0, 1],
                target=[0, 0, 0],
                up=[0, 1, 0]
            )
        },
        'sensor2': {
            'type': 'perspective',
            'fov': 45,
            'to_world': mi.ScalarTransform4f().look_at(
                origin=[1, 0, 0],
                target=[0, 0, 0],
                up=[0, 1, 0]
            ),
        }
        'film_id': {
            'type': '<film_type>',
            # ...
        },
        'sampler_id': {
            'type': '<sampler_type>',
            # ...
        }
*/

MI_VARIANT class BatchSensor final : public Sensor<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Sensor, m_film, m_shape, m_needs_sample_3, sample_wavelengths)
    MI_IMPORT_TYPES(Shape, SensorPtr)

    BatchSensor(const Properties &props) : Base(props) {
        for (auto [unused, o] : props.objects()) {
            ref<Base> sensor(dynamic_cast<Base *>(o.get()));
            ref<Shape> shape(dynamic_cast<Shape *>(o.get()));

            if (sensor) {
                m_sensors.push_back(sensor);
            } else if (shape) {
                if (shape->is_sensor())
                    m_sensors.push_back(shape->sensor());
                else
                    Throw("BatchSensor: shapes can only be specified as "
                          "children if a sensor is associated with them!");
            }
        }

        if (m_sensors.empty())
            Throw("BatchSensor: at least one child sensor must be specified!");

        ScalarPoint2u size = m_film->size();
        uint32_t sub_size = size.x() / (uint32_t) m_sensors.size();
        if (sub_size * (uint32_t) m_sensors.size() != size.x())
            Throw("BatchSensor: the horizontal resolution (currently %u) must "
                  "be divisible by the number of child sensors (%zu)!",
                  size.x(), m_sensors.size());

        m_needs_sample_3 = false;
        for (size_t i = 0; i < m_sensors.size(); ++i) {
            m_sensors[i]->film()->set_size(ScalarPoint2u(sub_size, size.y()));
            m_sensors[i]->parameters_changed();
            m_needs_sample_3 |= m_sensors[i]->needs_aperture_sample();
        }

        m_sensors_dr = dr::load<DynamicBuffer<SensorPtr>>(m_sensors.data(),
                                                          m_sensors.size());
    }

    virtual std::pair<Ray3f, Spectrum>
    sample_ray(Float time, Float wavelength_sample,
               const Point2f &position_sample, const Point2f &aperture_sample,
               Mask active = true) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        Float  idx_f = position_sample.x() * (ScalarFloat) m_sensors.size();
        UInt32 idx_u = UInt32(idx_f);

        UInt32 index = dr::minimum(idx_u, (uint32_t) (m_sensors.size() - 1));
        SensorPtr sensor = dr::gather<SensorPtr>(m_sensors_dr, index, active);


        Point2f position_sample_2(idx_f - Float(idx_u), position_sample.y());

        auto [ray, spec] =
            sensor->sample_ray(time, wavelength_sample, position_sample_2,
                               aperture_sample, active);

        /* The `m_last_index` variable **needs** to be updated after the
         * virtual function call above. In recorded JIT modes, the tracing will
         * also cover this function and hence overwrite `m_last_index` as part
         * of that process. To "undo" that undesired side_effect, we must
         * update `m_last_index` after that virtual function call. */
        m_last_index = index;

        return { ray, spec };
    }

    std::pair<RayDifferential3f, Spectrum>
    sample_ray_differential(Float time, Float wavelength_sample,
                            const Point2f &position_sample,
                            const Point2f &aperture_sample,
                            Mask active) const override {

        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        Float  idx_f = position_sample.x() * (ScalarFloat) m_sensors.size();
        UInt32 idx_u = UInt32(idx_f);

        UInt32 index = dr::minimum(idx_u, (uint32_t) (m_sensors.size() - 1));
        SensorPtr sensor = dr::gather<SensorPtr>(m_sensors_dr, index, active);

        Point2f position_sample_2(idx_f - Float(idx_u), position_sample.y());

        auto [ray, spec] = sensor->sample_ray_differential(
            time, wavelength_sample, position_sample_2, aperture_sample,
            active);

        /* The `m_last_index` variable **needs** to be updated after the
         * virtual function call above. In recorded JIT modes, the tracing will
         * also cover this function and hence overwrite `m_last_index` as part
         * of that process. To "undo" that undesired side_effect, we must
         * update `m_last_index` after that virtual function call. */
        m_last_index = index;

        return { ray, spec };
    }

    std::pair<DirectionSample3f, Spectrum>
    sample_direction(const Interaction3f &it, const Point2f &sample, Mask active) const override {
        DirectionSample3f result_1 = dr::zeros<DirectionSample3f>();
        Spectrum result_2 = dr::zeros<Spectrum>();

        // The behavior of random sampling a sensor instead of
        // querying `m_last_index` is useful for ptracer rendering. But it
        // is not desired if we call `sensor.sample_direction()` to
        // re-attach gradients. We detect the latter case by checking if
        // `it` has gradient tracking enabled.

        if (dr::grad_enabled(it)) {
            for (size_t i = 0; i < m_sensors.size(); ++i) {
                Mask active_i = active && (m_last_index == i);
                auto [rv_1, rv_2] =
                    m_sensors[i]->sample_direction(it, sample, active_i);
                rv_1.uv.x() += i * m_sensors[i]->film()->size().x();
                result_1[active_i] = rv_1;
                result_2[active_i] = rv_2;
            }
        } else {
            // Randomly sample a valid connection to a sensor
            Point2f sample_(sample);
            UInt32 valid_count(0u);

            for (size_t i = 0; i < m_sensors.size(); ++i) {
                auto [rv_1, rv_2] =
                    m_sensors[i]->sample_direction(it, sample_, active);
                rv_1.uv.x() += i * m_sensors[i]->film()->size().x();

                Mask active_i = active && (rv_1.pdf != 0.f);
                valid_count += dr::select(active_i, 1u, 0u);

                // Should we put this sample into the reservoir?
                Float  idx_f = sample_.x() * valid_count;
                UInt32 idx_u = UInt32(idx_f);
                Mask   accept = active_i && (idx_u == valid_count - 1u);

                // Reuse sample_.x
                sample_.x() = dr::select(active_i, idx_f - idx_u, sample_.x());

                // Update the result
                result_1[accept] = rv_1;
                result_2[accept] = rv_2;
            }

            // Account for reservoir sampling probability
            Float reservoir_pdf = dr::select(valid_count > 0u, valid_count, 1u) / (ScalarFloat) m_sensors.size();
            result_1.pdf /= reservoir_pdf;
            result_2 *= reservoir_pdf;
        }


        return { result_1, result_2 };
    }

    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        Float result = dr::zeros<Float>();
        for (size_t i = 0; i < m_sensors.size(); ++i) {
            Mask active_i = active && (m_last_index == i);
            dr::masked(result, active_i) = m_sensors[i]->pdf_direction(it, ds, active_i);
        }
        return result;
    }

    Spectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        Spectrum result = dr::zeros<Spectrum>();
        for (size_t i = 0; i < m_sensors.size(); ++i) {
            Mask active_i = active && (m_last_index == i);
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

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
        std::string id;
        for(size_t i = 0; i < m_sensors.size(); i++) {
            id = m_sensors.at(i)->id();
            if (id.empty() || string::starts_with(id, "_unnamed_"))
                id = "sensor" + std::to_string(i);
            callback->put_object(id, m_sensors.at(i).get(), +ParamFlags::NonDifferentiable);
        }
    }

    MI_DECLARE_CLASS()
private:
    std::vector<ref<Base>> m_sensors;
    DynamicBuffer<SensorPtr> m_sensors_dr;
    mutable UInt32 m_last_index;
};

MI_IMPLEMENT_CLASS_VARIANT(BatchSensor, Sensor)
MI_EXPORT_PLUGIN(BatchSensor, "BatchSensor");
NAMESPACE_END(mitsuba)
