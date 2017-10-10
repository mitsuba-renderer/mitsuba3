#include <mitsuba/core/properties.h>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/sensor.h>

NAMESPACE_BEGIN(mitsuba)

// TODO: documentation
class PerspectiveCameraImpl : public PerspectiveCamera {
public:
    // =============================================================
    //! @{ \name Constructors
    // =============================================================

    PerspectiveCameraImpl(const Properties &props) : PerspectiveCamera(props) {
        /* This sensor is the result of a limiting process where the aperture
           radius tends to zero. However, it still has all the cosine
           foreshortening terms caused by the aperture, hence the flag "EOnSurface" */
        m_type |= EDeltaPosition | EPerspectiveCamera
                  | EOnSurface |EDirectionSampleMapsToPixels;

        if (world_transform(0).has_scale()) {
            Log(EError, "Scale factors in the camera-to-world"
                        " transformation are not allowed! Transform:\n%s",
                        world_transform(0));
        }

        configure();
    }

    // PerspectiveCameraImpl(Stream *stream, InstanceManager *manager)
    //         : PerspectiveCamera(stream, manager) {
    //     configure();
    // }

    void configure() {
        using Transform = Transform4f;

        const Vector2i &film_size   = m_film->size();
        const Vector2i &crop_size   = m_film->crop_size();
        const Point2i  &crop_offset = m_film->crop_offset();

        Vector2f rel_size((Float) crop_size.x() / (Float) film_size.x(),
                          (Float) crop_size.y() / (Float) film_size.y());
        Point2f rel_offset((Float) crop_offset.x() / (Float) film_size.x(),
                           (Float) crop_offset.y() / (Float) film_size.y());

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
              Transform::scale(Vector3f(1.0f / rel_size.x(),
                                        1.0f / rel_size.y(), 1.0f))
            * Transform::translate(Vector3f(-rel_offset.x(),
                                            -rel_offset.y(), 0.0f))
            * Transform::scale(Vector3f(-0.5f,
                                        -0.5f * m_aspect, 1.0f))
            * Transform::translate(Vector3f(-1.0f,
                                            -1.0f / m_aspect, 0.0f))
            * Transform::perspective(m_x_fov, m_near_clip, m_far_clip);

        m_sample_to_camera = m_camera_to_sample.inverse();

        // Position differentials on the near plane
        m_dx = m_sample_to_camera * Point3f(m_inv_resolution.x(), 0.0f, 0.0f)
               - m_sample_to_camera * Point3f(0.0f);
        m_dy = m_sample_to_camera * Point3f(0.0f, m_inv_resolution.y(), 0.0f)
               - m_sample_to_camera * Point3f(0.0f);

        // Precompute some data for importance(). Please
        // look at that function for further details.
        Point3f pmin(m_sample_to_camera * Point3f(0, 0, 0)),
                pmax(m_sample_to_camera * Point3f(1, 1, 0));

        m_image_rect.reset();
        m_image_rect.expand(Point2f(pmin.x(), pmin.y()) / pmin.z());
        m_image_rect.expand(Point2f(pmax.x(), pmax.y()) / pmax.z());
        m_normalization = Float(1) / m_image_rect.volume();

        // Clip-space transformation for OpenGL
        m_clip_transform =
            Transform::translate(Vector3f(
                 (1.0f - 2.0f * rel_offset.x()) / rel_size.x() - 1.0f,
                -(1.0f - 2.0f * rel_offset.y()) / rel_size.y() + 1.0f,
                0.0f)
            ) * Transform::scale(Vector3f(
                1.0f / rel_size.x(),
                1.0f / rel_size.y(),
                1.0f)
        );
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Sampling methods (Sensor interface)
    // =============================================================

    template <typename Point2, typename Value>
    auto sample_ray_impl(const Point2 &position_sample,
                         const Point2 &/*aperture_sample*/,
                         Value time_sample) const {
        using Point3 = Point<Value, 3>;
        using Ray = Ray<Point3>;
        using Spectrum = Spectrum<Value>;
        using Vector3 = Vector<Value, 3>;

        Ray ray;
        ray.time = sample_time(time_sample);

        // Compute the corresponding position on the near plane (in local
        // camera space).
        const Point3 nearP = m_sample_to_camera * Point3(
            position_sample.x() * m_inv_resolution.x(),
            position_sample.y() * m_inv_resolution.y(), 0.0f);

        // Turn that into a normalized ray direction, and adjust the ray
        // interval accordingly.
        const auto d = normalize(Vector3(nearP));
        const Value invZ = rcp(d.z());
        ray.mint = m_near_clip * invZ;
        ray.maxt = m_far_clip * invZ;

        const auto &trafo = m_world_transform->lookup(ray.time);
        ray.o = trafo.transform_affine(Point3(0.0f));
        ray.d = trafo * d;

        return std::make_pair(ray, Spectrum(1.0f));
    }

    std::pair<Ray3f, Spectrumf>
    sample_ray(const Point2f &position_sample,
               const Point2f &aperture_sample,
               Float time_sample) const override {
        return sample_ray_impl(position_sample, aperture_sample, time_sample);
    }

    /// Vectorized version of \ref sample_ray
    std::pair<Ray3fP, SpectrumfP>
    sample_ray(const Point2fP &position_sample,
               const Point2fP &aperture_sample,
               FloatP time_sample) const override {
        return sample_ray_impl(position_sample, aperture_sample, time_sample);
    }


    template <typename Point2, typename Value = value_t<Point2>>
    auto sample_ray_differential_impl(const Point2 &position_sample,
                                      const Point2 &/*aperture_sample*/,
                                      Value time_sample) const {
        using Point3 = Point<Value, 3>;
        using RayDifferential = RayDifferential<Point3>;
        using Spectrum = Spectrum<Value>;
        using Vector3 = Vector<Value, 3>;

        RayDifferential ray;
        ray.time = sample_time(time_sample);

        // Compute the corresponding position on the near plane (in local
        // camera space).
        const Point3 nearP = m_sample_to_camera * Point3(
            position_sample.x() * m_inv_resolution.x(),
            position_sample.y() * m_inv_resolution.y(), 0.0f);

        // Turn that into a normalized ray direction, and adjust the ray
        // interval accordingly.
        const auto d = normalize(Vector3(nearP));
        const Value invZ = rcp(d.z());
        ray.mint = m_near_clip * invZ;
        ray.maxt = m_far_clip * invZ;

        const auto &trafo = m_world_transform->lookup(ray.time);
        ray.o = trafo.transform_affine(Point3(0.0f));
        ray.d = trafo * d;

        ray.o_x = ray.o_y = ray.o;

        ray.d_x = trafo * normalize(Vector3(nearP) + m_dx);
        ray.d_y = trafo * normalize(Vector3(nearP) + m_dy);
        ray.has_differentials = true;

        return std::make_pair(ray, Spectrum(1.0f));
    }


    std::pair<RayDifferential3f, Spectrumf> sample_ray_differential(
        const Point2f &sample_position, const Point2f &aperture_sample,
        Float time_sample) const override {
        return sample_ray_differential_impl(sample_position, aperture_sample,
                                            time_sample);
    }

    /// Vectorized version of \ref sample_ray_differential
    std::pair<RayDifferential3fP, SpectrumfP> sample_ray_differential(
        const Point2fP &sample_position, const Point2fP &aperture_sample,
        FloatP time_sample) const override {
        return sample_ray_differential_impl(sample_position, aperture_sample,
                                            time_sample);
    }


    std::pair<bool, Point2f> get_sample_position(
        const PositionSample3f &/*p_rec*/, const DirectionSample3f &/*d_rec*/) const override {
        Log(EError, "get_sample_position(...) not implemented yet.");
        return std::make_pair(false, Point2f(0.0f, 0.0f));
    }

    /// Vectorized version of \ref eval
    std::pair<BoolP, Point2fP> get_sample_position(
        const PositionSample3fP &/*p_rec*/, const DirectionSample3fP &/*d_rec*/) const override {
        Log(EError, "get_sample_position(...) not implemented yet.");
        return std::make_pair(BoolP(false), Point2fP(0.0f));
    }


    std::pair<Spectrumf, Point2f> eval(
        const SurfaceInteraction3f &/*its*/, const Vector3f &/*d*/) const override {
        Log(EError, "evel(...) not implemented yet.");
        return std::make_pair(Spectrumf(0.0f), Point2f(0.0f));
    }

    /// Vectorized version of \ref eval
    std::pair<SpectrumfP, Point2fP> eval(
        const SurfaceInteraction3fP &/*its*/, const Vector3fP &/*d*/) const override {
        Log(EError, "evel(...) not implemented yet.");
        return std::make_pair(SpectrumfP(0.0f), Point2fP(0.0f));
    }

    //! @}
    // =============================================================


    // =============================================================
    //! @{ \name Endpoint interface
    // =============================================================

    // TODO: implement the Endpoint interface.

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Misc
    // =============================================================

    Transform4f projection_transform(const Point2f &/*aperture_sample*/,
            const Point2f &aa_sample) const override {
        Float right = std::tan(Float(0.5) * deg_to_rad(m_x_fov)) * m_near_clip;
        Float left  = -right;
        Float top = right / m_aspect;
        Float bottom = -top;

        Vector2f offset(
            (right-left) / m_film->size().x() * (aa_sample.x() - Float(0.5f)),
            (top-bottom) / m_film->size().y() * (aa_sample.y() - Float(0.5f)));

        return m_clip_transform * Transform4f::gl_frustum(
            left + offset.x(), right + offset.x(),
            bottom + offset.y(), top + offset.y(),
            m_near_clip, m_far_clip
        );
    }

    BoundingBox3f bbox() const override {
        return m_world_transform->translation_bounds();
    }

    std::string to_string() const override {
        using string::indent;

        std::ostringstream oss;
        oss << "PerspectiveCamera[" << std::endl
            << "  fov = [" << x_fov() << ", " << y_fov() << "]," << std::endl

            << "  near_clip = " << m_near_clip << "," << std::endl
            << "  far_clip = " << m_far_clip << "," << std::endl
            << "  focus_distance = " << m_focus_distance << "," << std::endl

            << "  film = " << indent(m_film->to_string()) << "," << std::endl
            << "  sampler = " << indent(m_sampler->to_string()) << "," << std::endl
            << "  resolution = " << m_resolution << "," << std::endl
            << "  shutter_open = " << m_shutter_open << "," << std::endl
            << "  shutter_open_time = " << m_shutter_open_time << "," << std::endl
            << "  aspect = " << m_aspect << "," << std::endl

            << "  world_transform = " << indent(m_world_transform->to_string()) << "," << std::endl
            << "  type = " << m_type << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()

    //! @}
    // =============================================================

private:
    Transform4f m_camera_to_sample;
    Transform4f m_sample_to_camera;
    Transform4f m_clip_transform;
    BoundingBox2f m_image_rect;
    Float m_normalization;
    Vector3f m_dx, m_dy;
};


MTS_IMPLEMENT_CLASS(PerspectiveCameraImpl, PerspectiveCamera);
MTS_EXPORT_PLUGIN(PerspectiveCameraImpl, "Perspective Camera");
NAMESPACE_END(mitsuba)
