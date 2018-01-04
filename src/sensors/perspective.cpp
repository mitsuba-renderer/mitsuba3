#include <mitsuba/render/sensor.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

class PerspectiveCamera : public ProjectiveCamera {
public:
    // =============================================================
    //! @{ \name Constructors
    // =============================================================

    PerspectiveCamera(const Properties &props) : ProjectiveCamera(props) {
        if (props.has_property("fov") && props.has_property("focal_length"))
            Throw("Please specify either a focal length ('focal_length') or a "
                  "field of view ('fov')!");

        Float fov;
        std::string fov_axis;

        if (props.has_property("fov")) {
            fov = props.float_("fov");

            fov_axis = string::to_lower(props.string("fov_axis", "x"));

            if (fov_axis == "smaller")
                fov_axis = m_aspect > 1 ? "y" : "x";
            else if (fov_axis == "larger")
                fov_axis = m_aspect > 1 ? "x" : "y";
        } else {
            std::string f = props.string("focal_length", "50mm");
            if (string::ends_with(f, "mm"))
                f = f.substr(0, f.length()-2);

            char *end_ptr = nullptr;
            Float value = (Float) strtod(f.c_str(), &end_ptr);
            if (*end_ptr != '\0')
                Throw("Could not parse the focal length (must be of the form "
                    "<x>mm, where <x> is a positive integer)!");

            fov = 2.f * rad_to_deg(std::atan(std::sqrt(Float(36 * 36 + 24 * 24)) / (2.f * value)));
            fov_axis = "diagonal";
        }

        if (fov_axis == "x") {
            m_x_fov = fov;
        } else if (fov_axis == "y") {
            m_x_fov = rad_to_deg(
                2.f * std::atan(std::tan(.5f * deg_to_rad(fov)) * m_aspect));
        } else if (fov_axis == "diagonal") {
            Float diagonal = 2.f * std::tan(.5f * deg_to_rad(fov));
            Float width = diagonal / std::sqrt(1.f + 1.f / (m_aspect*m_aspect));
            m_x_fov = rad_to_deg(2.f * std::atan(width*.5f));
        } else {
            Throw("The 'fov_axis' parameter must be set to one of 'smaller', "
                  "'larger', 'diagonal', 'x', or 'y'!");
        }

        if (m_x_fov <= 0.f || m_x_fov >= 180.f)
            Throw("The horizontal field of view must be in the range [0, 180]!");

        if (m_world_transform->has_scale())
            Throw("Scale factors in the camera-to-world transformation are not allowed!");

        Vector2f film_size   = Vector2f(m_film->size()),
                 crop_size   = Vector2f(m_film->crop_size()),
                 rel_size    = crop_size / film_size;

        Point2f  crop_offset = Point2f(m_film->crop_offset()),
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
              Transform4f::scale(Vector3f(1.f / rel_size.x(), 1.f / rel_size.y(), 1.f))
            * Transform4f::translate(Vector3f(-rel_offset.x(), -rel_offset.y(), 0.f))
            * Transform4f::scale(Vector3f(-0.5f, -0.5f * m_aspect, 1.f))
            * Transform4f::translate(Vector3f(-1.f, -1.f / m_aspect, 0.f))
            * Transform4f::perspective(m_x_fov, m_near_clip, m_far_clip);

        m_sample_to_camera = m_camera_to_sample.inverse();

        // Position differentials on the near plane
        m_dx = m_sample_to_camera * Point3f(1.f / m_resolution.x(), 0.f, 0.f)
             - m_sample_to_camera * Point3f(0.f);
        m_dy = m_sample_to_camera * Point3f(0.f, 1.f / m_resolution.y(), 0.f)
             - m_sample_to_camera * Point3f(0.f);

        /* Precompute some data for importance(). Please
           look at that function for further details. */
        Point3f pmin(m_sample_to_camera * Point3f(0.f, 0.f, 0.f)),
                pmax(m_sample_to_camera * Point3f(1.f, 1.f, 0.f));

        m_image_rect.reset();
        m_image_rect.expand(Point2f(pmin.x(), pmin.y()) / pmin.z());
        m_image_rect.expand(Point2f(pmax.x(), pmax.y()) / pmax.z());
        m_normalization = 1.f / m_image_rect.volume();
        m_needs_sample_3 = false;
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Sampling methods (Sensor interface)
    // =============================================================

    template <typename Point2, typename Value, typename Mask = mask_t<Value>>
    auto sample_ray_impl(Value time,
                         Value wavelength_sample,
                         const Point2& position_sample,
                         const Point2& /* aperture_sample */,
                         Mask active) const {
        using Point3   = Point<Value, 3>;
        using Ray      = Ray<Point3>;
        using Spectrum = Spectrum<Value>;
        using Vector3  = Vector<Value, 3>;

        Ray ray;
        Spectrum spec_weight;
        std::tie(ray.wavelengths, spec_weight) = sample_rgb_spectrum(
            enoki::sample_shifted<Spectrum>(wavelength_sample));
        ray.time = time;

        /* Compute the sample position on the near plane (local camera space). */
        Point3 near_p = m_sample_to_camera *
                        Point3(position_sample.x(), position_sample.y(), 0.f);

        /* Convert into a normalized ray direction; adjust the ray interval accordingly. */
        Vector3 d = normalize(Vector3(near_p));

        Value inv_z = rcp(d.z());
        ray.mint = m_near_clip * inv_z;
        ray.maxt = m_far_clip * inv_z;

        auto trafo = m_world_transform->eval(ray.time, active);
        ray.o = trafo.translation();
        ray.d = trafo * d;
        ray.update();

        return std::make_pair(ray, spec_weight);
    }

    template <typename Point2, typename Value = value_t<Point2>,
              typename Mask = mask_t<Value>>
    auto sample_ray_differential_impl(Value time,
                                      Value wavelength_sample,
                                      const Point2&  position_sample,
                                      const Point2& /*aperture_sample*/,
                                      Mask active) const {
        using Point3 = Point<Value, 3>;
        using RayDifferential = RayDifferential<Point3>;
        using Spectrum = Spectrum<Value>;
        using Vector3 = Vector<Value, 3>;

        RayDifferential ray;
        Spectrum spec_weight;
        std::tie(ray.wavelengths, spec_weight) = sample_rgb_spectrum(
            enoki::sample_shifted<Spectrum>(wavelength_sample));
        ray.time = time;

        /* Compute the sample position on the near plane (local camera space). */
        Point3 near_p = m_sample_to_camera *
                        Point3(position_sample.x(), position_sample.y(), 0.f);

        /* Convert into a normalized ray direction; adjust the ray interval accordingly. */
        Vector3 d = normalize(Vector3(near_p));
        Value inv_z = rcp(d.z());
        ray.mint = m_near_clip * inv_z;
        ray.maxt = m_far_clip * inv_z;

        auto trafo = m_world_transform->eval(ray.time, active);
        ray.o = trafo.transform_affine(Point3(0.f));
        ray.d = trafo * d;
        ray.update();

        ray.o_x = ray.o_y = ray.o;

        ray.d_x = trafo * normalize(Vector3(near_p) + m_dx);
        ray.d_y = trafo * normalize(Vector3(near_p) + m_dy);
        ray.has_differentials = true;

        return std::make_pair(ray, spec_weight);
    }

    BoundingBox3f bbox() const override {
        return m_world_transform->translation_bounds();
    }

    template <typename Interaction, typename Mask,
              typename Value = typename Interaction::Value,
              typename Point2 = typename Interaction::Point2,
              typename Point3 = typename Interaction::Point3,
              typename Spectrum = typename Interaction::Spectrum,
              typename DirectionSample = DirectionSample<Point3>>
    std::pair<DirectionSample, Spectrum>
    sample_direction_impl(const Interaction & /* it */, const Point2 & /* sample */, Mask /* active */) const {
        NotImplementedError("sample_direct");
    }

    template <typename Interaction, typename DirectionSample, typename Mask,
              typename Value = typename DirectionSample::Value>
    Value pdf_direction_impl(const Interaction & /* it */, const DirectionSample & /* ds */, Mask /* active */) const {
        NotImplementedError("pdf_direct");
    }

    template <typename SurfaceInteraction, typename Mask,
              typename Spectrum = typename SurfaceInteraction::Spectrum,
              typename Frame = Frame<typename SurfaceInteraction::Point3>>
    Spectrum eval_impl(const SurfaceInteraction & /* si */, Mask /* active */) const {
        NotImplementedError("eval");
    }

    //! @}
    // =============================================================

    std::string to_string() const override {
        using string::indent;

        std::ostringstream oss;
        oss << "PerspectiveCamera[" << std::endl
            << "  x_fov = " << m_x_fov << "," << std::endl
            << "  near_clip = " << m_near_clip << "," << std::endl
            << "  far_clip = " << m_far_clip << "," << std::endl
            << "  focus_distance = " << m_focus_distance << "," << std::endl
            << "  film = " << indent(m_film->to_string()) << "," << std::endl
            << "  sampler = " << indent(m_sampler->to_string()) << "," << std::endl
            << "  resolution = " << m_resolution << "," << std::endl
            << "  shutter_open = " << m_shutter_open << "," << std::endl
            << "  shutter_open_time = " << m_shutter_open_time << "," << std::endl
            << "  aspect = " << m_aspect << "," << std::endl
            << "  world_transform = " << indent(m_world_transform)  << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_SENSOR()
    MTS_DECLARE_CLASS()

private:
    Transform4f m_camera_to_sample;
    Transform4f m_sample_to_camera;
    BoundingBox2f m_image_rect;
    Float m_normalization;
    Float m_x_fov;
    Vector3f m_dx, m_dy;
};


MTS_IMPLEMENT_CLASS(PerspectiveCamera, ProjectiveCamera);
MTS_EXPORT_PLUGIN(PerspectiveCamera, "Perspective Camera");
NAMESPACE_END(mitsuba)
