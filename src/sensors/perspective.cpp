#include <mitsuba/render/sensor.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _sensor-perspective:

Perspective pinhole camera (:monosp:`perspective`)
--------------------------------------------------

.. pluginparameters::

 * - to_world
   - |transform|
   - Specifies an optional camera-to-world transformation.
     (Default: none (i.e. camera space = world space))
 * - focal_length
   - |string|
   - Denotes the camera's focal length specified using :monosp:`35mm` film equivalent units.
     See the main description for further details. (Default: :monosp:`50mm`)
 * - fov
   - |float|
   - An alternative to :monosp:`focal_length`: denotes the camera's field of view in degrees---must be
     between 0 and 180, excluding the extremes.
 * - fov_axis
   - |string|
   - When the parameter :monosp:`fov` is given (and only then), this parameter further specifies
     the image axis, to which it applies.

     1. :monosp:`x`: :monosp:`fov` maps to the :monosp:`x`-axis in screen space.
     2. :monosp:`y`: :monosp:`fov` maps to the :monosp:`y`-axis in screen space.
     3. :monosp:`diagonal`: :monosp:`fov` maps to the screen diagonal.
     4. :monosp:`smaller`: :monosp:`fov` maps to the smaller dimension
        (e.g. :monosp:`x` when :monosp:`width` < :monosp:`height`)
     5. :monosp:`larger`: :monosp:`fov` maps to the larger dimension
        (e.g. :monosp:`y` when :monosp:`width` < :monosp:`height`)

     The default is :monosp:`x`.
 * - near_clip, far_clip
   - |float|
   - Distance to the near/far clip planes. (Default: :monosp:`near_clip=1e-2` (i.e. :monosp:`0.01`)
     and :monosp:`far_clip=1e4` (i.e. :monosp:`10000`))

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/sensor_perspective.jpg
   :caption: The material test ball viewed through a perspective pinhole camera. (:monosp:`fov=28`)
.. subfigure:: ../../resources/data/docs/images/render/sensor_perspective_large_fov.jpg
   :caption: The material test ball viewed through a perspective pinhole camera. (:monosp:`fov=40`)
.. subfigend::
   :label: fig-perspective

This plugin implements a simple idealizied perspective camera model, which
has an infinitely small aperture. This creates an infinite depth of field,
i.e. no optical blurring occurs.

By default, the camera's field of view is specified using a 35mm film
equivalent focal length, which is first converted into a diagonal field
of view and subsequently applied to the camera. This assumes that
the film's aspect ratio matches that of 35mm film (1.5:1), though the
parameter still behaves intuitively when this is not the case.
Alternatively, it is also possible to specify a field of view in degrees
along a given axis (see the :monosp:`fov` and :monosp:`fov_axis` parameters).

The exact camera position and orientation is most easily expressed using the
:monosp:`lookat` tag, i.e.:

.. code-block:: xml

    <sensor type="perspective">
        <transform name="to_world">
            <!-- Move and rotate the camera so that looks from (1, 1, 1) to (1, 2, 1)
                and the direction (0, 0, 1) points "up" in the output image -->
            <lookat origin="1, 1, 1" target="1, 2, 1" up="0, 0, 1"/>
        </transform>
    </sensor>

 */

template <typename Float, typename Spectrum>
class PerspectiveCamera final : public ProjectiveCamera<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(ProjectiveCamera, m_world_transform, m_needs_sample_3, m_film, m_sampler,
                    m_resolution, m_shutter_open, m_shutter_open_time, m_aspect, m_near_clip,
                    m_far_clip, m_focus_distance)
    MTS_IMPORT_TYPES()

    // =============================================================
    //! @{ \name Constructors
    // =============================================================

    PerspectiveCamera(const Properties &props) : Base(props) {
        m_x_fov = parse_fov(props, m_aspect);

        if (m_world_transform->has_scale())
            Throw("Scale factors in the camera-to-world transformation are not allowed!");

        update_camera_transforms();
        m_needs_sample_3 = false;
    }

    // TODO duplicate code with ThinLens
    void update_camera_transforms() {
        ScalarVector2f film_size = ScalarVector2f(m_film->size()),
                       crop_size = ScalarVector2f(m_film->crop_size()),
                       rel_size  = crop_size / film_size;

        ScalarPoint2f crop_offset = ScalarPoint2f(m_film->crop_offset()),
                      rel_offset  = crop_offset / film_size;

        /**
         * These do the following (in reverse order):
         *
         * 1. Create transform from camera space to [-1,1]x[-1,1]x[0,1] clip
         *    coordinates (not taking account of the aspect ratio yet)
         *
         * 2+3. Translate and scale to shift the clip coordinates into the
         *    range from zero to one, and take the aspect ratio into account.
         *
         * 4+5. Translate and scale the coordinates once more to account
         *     for a cropping window (if there is any)
         */
        m_camera_to_sample =
            ScalarTransform4f::scale(ScalarVector3f(1.f / rel_size.x(), 1.f / rel_size.y(), 1.f)) *
            ScalarTransform4f::translate(ScalarVector3f(-rel_offset.x(), -rel_offset.y(), 0.f)) *
            ScalarTransform4f::scale(ScalarVector3f(-0.5f, -0.5f * m_aspect, 1.f)) *
            ScalarTransform4f::translate(ScalarVector3f(-1.f, -1.f / m_aspect, 0.f)) *
            ScalarTransform4f::perspective(m_x_fov, m_near_clip, m_far_clip);

        m_sample_to_camera = m_camera_to_sample.inverse();

        // Position differentials on the near plane
        m_dx = m_sample_to_camera * ScalarPoint3f(1.f / m_resolution.x(), 0.f, 0.f) -
               m_sample_to_camera * ScalarPoint3f(0.f);
        m_dy = m_sample_to_camera * ScalarPoint3f(0.f, 1.f / m_resolution.y(), 0.f)
             - m_sample_to_camera * ScalarPoint3f(0.f);

        /* Precompute some data for importance(). Please
           look at that function for further details. */
        ScalarPoint3f pmin(m_sample_to_camera * ScalarPoint3f(0.f, 0.f, 0.f)),
                      pmax(m_sample_to_camera * ScalarPoint3f(1.f, 1.f, 0.f));

        m_image_rect.reset();
        m_image_rect.expand(ScalarPoint2f(pmin.x(), pmin.y()) / pmin.z());
        m_image_rect.expand(ScalarPoint2f(pmax.x(), pmax.y()) / pmax.z());
        m_normalization = 1.f / m_image_rect.volume();
        m_needs_sample_3 = false;
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Sampling methods (Sensor interface)
    // =============================================================

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &position_sample,
                                          const Point2f & /*aperture_sample*/,
                                          Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        auto [wavelengths, wav_weight] = sample_wavelength<Float, Spectrum>(wavelength_sample);
        Ray3f ray;
        ray.time = time;
        ray.wavelengths = wavelengths;

        // Compute the sample position on the near plane (local camera space).
        Point3f near_p = m_sample_to_camera *
                         Point3f(position_sample.x(), position_sample.y(), 0.f);

        // Convert into a normalized ray direction; adjust the ray interval accordingly.
        Vector3f d = normalize(Vector3f(near_p));

        Float inv_z = rcp(d.z());
        ray.mint = m_near_clip * inv_z;
        ray.maxt = m_far_clip * inv_z;

        auto trafo = m_world_transform->eval(ray.time, active);
        ray.o = trafo.translation();
        ray.d = trafo * d;
        ray.update();

        return std::make_pair(ray, wav_weight);
    }

    std::pair<RayDifferential3f, Spectrum>
    sample_ray_differential(Float time, Float wavelength_sample, const Point2f &position_sample,
                            const Point2f & /*aperture_sample*/, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

        auto [wavelengths, wav_weight] = sample_wavelength<Float, Spectrum>(wavelength_sample);
        RayDifferential3f ray;
        ray.time = time;
        ray.wavelengths = wavelengths;

        // Compute the sample position on the near plane (local camera space).
        Point3f near_p = m_sample_to_camera *
                         Point3f(position_sample.x(), position_sample.y(), 0.f);

        // Convert into a normalized ray direction; adjust the ray interval accordingly.
        Vector3f d = normalize(Vector3f(near_p));
        Float inv_z = rcp(d.z());
        ray.mint = m_near_clip * inv_z;
        ray.maxt = m_far_clip * inv_z;

        auto trafo = m_world_transform->eval(ray.time, active);
        ray.o = trafo.transform_affine(Point3f(0.f));
        ray.d = trafo * d;
        ray.update();

        ray.o_x = ray.o_y = ray.o;

        ray.d_x = trafo * normalize(Vector3f(near_p) + m_dx);
        ray.d_y = trafo * normalize(Vector3f(near_p) + m_dy);
        ray.has_differentials = true;

        return std::make_pair(ray, wav_weight);
    }

    ScalarBoundingBox3f bbox() const override {
        return m_world_transform->translation_bounds();
    }

    void set_crop_window(const ScalarVector2i &crop_size,
                         const ScalarPoint2i &crop_offset) override {
        Base::set_crop_window(crop_size, crop_offset);
        update_camera_transforms();
    }

    //! @}
    // =============================================================

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
        // TODO x_fov
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        Base::parameters_changed(keys);
        // TODO
    }

    std::string to_string() const override {
        using string::indent;

        std::ostringstream oss;
        oss << "PerspectiveCamera[" << std::endl
            << "  x_fov = " << m_x_fov << "," << std::endl
            << "  near_clip = " << m_near_clip << "," << std::endl
            << "  far_clip = " << m_far_clip << "," << std::endl
            << "  focus_distance = " << m_focus_distance << "," << std::endl
            << "  film = " << indent(m_film) << "," << std::endl
            << "  sampler = " << indent(m_sampler) << "," << std::endl
            << "  resolution = " << m_resolution << "," << std::endl
            << "  shutter_open = " << m_shutter_open << "," << std::endl
            << "  shutter_open_time = " << m_shutter_open_time << "," << std::endl
            << "  aspect = " << m_aspect << "," << std::endl
            << "  world_transform = " << indent(m_world_transform)  << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ScalarTransform4f m_camera_to_sample;
    ScalarTransform4f m_sample_to_camera;
    ScalarBoundingBox2f m_image_rect;
    ScalarFloat m_normalization;
    ScalarFloat m_x_fov;
    ScalarVector3f m_dx, m_dy;
};

MTS_IMPLEMENT_CLASS_VARIANT(PerspectiveCamera, ProjectiveCamera)
MTS_EXPORT_PLUGIN(PerspectiveCamera, "Perspective Camera");
NAMESPACE_END(mitsuba)
