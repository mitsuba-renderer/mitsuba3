#include <mitsuba/render/sensor.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _sensor-orthographic:

Orthographic camera (:monosp:`orthographic`)
--------------------------------------------------

.. pluginparameters::

 * - to_world
   - |transform|
   - Specifies an optional camera-to-world transformation.
     (Default: none (i.e. camera space = world space))
   - |exposed|

 * - near_clip, far_clip
   - |float|
   - Distance to the near/far clip planes. (Default: :monosp:`near_clip=1e-2` (i.e. :monosp:`0.01`)
     and :monosp:`far_clip=1e4` (i.e. :monosp:`10000`))
   - |exposed|

 * - srf
   - |spectrum|
   - Sensor Response Function that defines the :ref:`spectral sensitivity <explanation_srf_sensor>`
     of the sensor (Default: :monosp:`none`)


.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/sensor_orthographic.jpg
   :caption: The material test ball viewed through an orthographic camera. Note the complete lack of perspective.)
.. subfigure:: ../../resources/data/docs/images/render/sensor_orthographic_2.jpg
   :caption: A rendering of the Cornell box
.. subfigend::
   :label: fig-orthographic

This plugin implements a simple orthographic camera, i.e. a sensor
based on an orthographic projection without any form of perspective.
It can be thought of as a planar sensor that measures the radiance
along its normal direction. By default, this is the region $[-1, 1]^2$ inside
the XY-plane facing along the positive Z direction. Transformed versions
can be instantiated e.g. as follows:

The exact camera position and orientation is most easily expressed using the
:monosp:`look_at` tag, i.e.:

.. tabs::
    .. code-tab:: xml

        <sensor type="orthographic">
            <transform name="to_world">
                <!-- Resize the sensor plane to 20x20 world space units -->
                <scale x="10" y="10"/>

                <!-- Move and rotate the camera so that looks from (1, 1, 1) to (1, 2, 1)
                    and the direction (0, 0, 1) points "up" in the output image -->
                <look_at origin="1, 1, 1" target="1, 2, 1" up="0, 0, 1"/>
            </transform>
        </sensor>

    .. code-tab:: python

        'type': 'orthographic',
        'to_world': mi.ScalarTransform4f().look_at(
            origin=[1, 1, 1],
            target=[1, 2, 1],
            up=[0, 0, 1]
        ) @ mi.ScalarTransform4f().scale([10, 10, 1])

 */

template <typename Float, typename Spectrum>
class OrthographicCamera final : public ProjectiveCamera<Float, Spectrum> {
public:
    MI_IMPORT_BASE(ProjectiveCamera, m_to_world, m_needs_sample_3,
                    m_film, m_sampler, m_resolution, m_shutter_open,
                    m_shutter_open_time, m_near_clip, m_far_clip,
                    sample_wavelengths)
    MI_IMPORT_TYPES()

    OrthographicCamera(const Properties &props) : Base(props) {
        update_camera_transforms();
        m_needs_sample_3 = false;
    }

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
        callback->put_parameter("to_world", *m_to_world.ptr(), +ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        Base::parameters_changed(keys);
        update_camera_transforms();
    }

    void update_camera_transforms() {
        m_camera_to_sample = orthographic_projection(
            m_film->size(), m_film->crop_size(), m_film->crop_offset(),
            Float(m_near_clip), Float(m_far_clip));

        m_sample_to_camera = m_camera_to_sample.inverse();

        // Position differentials on the near plane
        m_dx = m_sample_to_camera * Point3f(1.f / m_resolution.x(), 0.f, 0.f) -
               m_sample_to_camera * Point3f(0.f);
        m_dy = m_sample_to_camera * Point3f(0.f, 1.f / m_resolution.y(), 0.f)
             - m_sample_to_camera * Point3f(0.f);

        m_normalization = 1.f / m_image_rect.volume();
        dr::make_opaque(m_camera_to_sample, m_sample_to_camera, m_dx, m_dy, m_normalization);
    }

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &position_sample,
                                          const Point2f & /*aperture_sample*/,
                                          Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        auto [wavelengths, wav_weight] =
            sample_wavelengths(dr::zeros<SurfaceInteraction3f>(),
                               wavelength_sample,
                               active);
        Ray3f ray;
        ray.time = time;
        ray.wavelengths = wavelengths;

        // Compute the sample position on the near plane (local camera space).
        Point3f near_p = m_sample_to_camera *
                         Point3f(position_sample.x(), position_sample.y(), 0.f);

        ray.o = m_to_world.value() * near_p;
        ray.d = dr::normalize(m_to_world.value() * Vector3f(0, 0, 1));
        ray.maxt = m_far_clip - m_near_clip;

        return { ray, wav_weight };
    }

    std::pair<RayDifferential3f, Spectrum> sample_ray_differential(
        Float time, Float wavelength_sample, const Point2f &position_sample,
        const Point2f & /*aperture_sample*/, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        auto [wavelengths, wav_weight] =
            sample_wavelengths(dr::zeros<SurfaceInteraction3f>(),
                               wavelength_sample,
                               active);
        RayDifferential3f ray;
        ray.time = time;
        ray.wavelengths = wavelengths;

        // Compute the sample position on the near plane (local camera space).
        Point3f near_p = m_sample_to_camera *
                         Point3f(position_sample.x(), position_sample.y(), 0.f);

        ray.o = m_to_world.value() * near_p;
        ray.d = dr::normalize(m_to_world.value() * Vector3f(0, 0, 1));
        ray.maxt = m_far_clip - m_near_clip;

        ray.o_x = m_to_world.value() * (near_p + m_dx);
        ray.o_y = m_to_world.value() * (near_p + m_dy);
        ray.d_x = ray.d_y = ray.d;
        ray.has_differentials = true;

        return { ray, wav_weight };
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarPoint3f p = m_to_world.scalar() * ScalarPoint3f(0.f);
        return ScalarBoundingBox3f(p, p);
    }

    std::string to_string() const override {
        using string::indent;

        std::ostringstream oss;
        oss << "OrthographicCamera[" << std::endl
            << "  near_clip = " << m_near_clip << "," << std::endl
            << "  far_clip = " << m_far_clip << "," << std::endl
            << "  film = " << indent(m_film) << "," << std::endl
            << "  sampler = " << indent(m_sampler) << "," << std::endl
            << "  resolution = " << m_resolution << "," << std::endl
            << "  shutter_open = " << m_shutter_open << "," << std::endl
            << "  shutter_open_time = " << m_shutter_open_time << "," << std::endl
            << "  world_transform = " << indent(m_to_world)  << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    Transform4f m_camera_to_sample;
    Transform4f m_sample_to_camera;
    BoundingBox2f m_image_rect;
    Float m_normalization;
    Vector3f m_dx, m_dy;
};

MI_IMPLEMENT_CLASS_VARIANT(OrthographicCamera, ProjectiveCamera)
MI_EXPORT_PLUGIN(OrthographicCamera, "Orthographic Camera");
NAMESPACE_END(mitsuba)
