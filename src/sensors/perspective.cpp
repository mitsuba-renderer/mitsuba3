#include <mitsuba/render/sensor.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class PerspectiveCamera final : public ProjectiveCamera<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN(PerspectiveCamera, ProjectiveCamera);
    MTS_USING_BASE(ProjectiveCamera, m_world_transform, m_needs_sample_3, m_film, m_sampler,
                   m_resolution, m_shutter_open, m_shutter_open_time, m_aspect, m_near_clip,
                   m_far_clip, m_focus_distance);
    using Scalar = scalar_t<Float>;

    // =============================================================
    //! @{ \name Constructors
    // =============================================================

    PerspectiveCamera(const Properties &props) : Base(props) {
        if (props.has_property("fov") && props.has_property("focal_length"))
            Throw("Please specify either a focal length ('focal_length') or a "
                  "field of view ('fov')!");

        Scalar fov;
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
            Scalar value = static_cast<Scalar>(strtod(f.c_str(), &end_ptr));
            if (*end_ptr != '\0')
                Throw("Could not parse the focal length (must be of the form "
                    "<x>mm, where <x> is a positive integer)!");

            fov = 2.f * rad_to_deg(std::atan(std::sqrt(Scalar(36 * 36 + 24 * 24)) / (2.f * value)));
            fov_axis = "diagonal";
        }

        if (fov_axis == "x") {
            m_x_fov = fov;
        } else if (fov_axis == "y") {
            m_x_fov = rad_to_deg(
                2.f * std::atan(std::tan(.5f * deg_to_rad(fov)) * m_aspect));
        } else if (fov_axis == "diagonal") {
            Scalar diagonal = 2.f * std::tan(.5f * deg_to_rad(fov));
            Scalar width = diagonal / std::sqrt(1.f + 1.f / (m_aspect*m_aspect));
            m_x_fov = rad_to_deg(2.f * std::atan(width*.5f));
        } else {
            Throw("The 'fov_axis' parameter must be set to one of 'smaller', "
                  "'larger', 'diagonal', 'x', or 'y'!");
        }

        if (m_x_fov <= 0.f || m_x_fov >= 180.f)
            Throw("The horizontal field of view must be in the range [0, 180]!");

        if (m_world_transform->has_scale())
            Throw("Scale factors in the camera-to-world transformation are not allowed!");

        update_camera_transforms();
        m_needs_sample_3 = false;
    }

    void update_camera_transforms() {
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

    std::pair<Ray3f, Spectrum> sample_ray(Float time, Float wavelength_sample,
                                          const Point2f &position_sample,
                                          const Point2f & /*aperture_sample*/,
                                          Mask active) const override {

        auto wav_sample = math::sample_shifted<Wavelength>(wavelength_sample);
        Wavelength wavelengths;
        Spectrum spec_weight;
        if constexpr (is_monochrome_v<Spectrum>) {
            std::tie(wavelengths, spec_weight) = sample_uniform_spectrum(wav_sample);
        } else if constexpr (is_rgb_v<Spectrum>) {
            NotImplementedError("Sampling rays in RGB mode");
        } else {
            std::tie(wavelengths, spec_weight) = sample_rgb_spectrum(wav_sample);
        }

        Ray3f ray;
        ray.time = time;
        ray.wavelength = wavelengths;

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

        return std::make_pair(ray, spec_weight);
    }

    std::pair<RayDifferential3f, Spectrum>
    sample_ray_differential(Float time, Float wavelength_sample, const Point2f &position_sample,
                            const Point2f & /*aperture_sample*/, Mask active) const override {
        // TODO: refactor this to avoid code duplication
        auto wav_sample = math::sample_shifted<Wavelength>(wavelength_sample);
        Wavelength wavelengths;
        Spectrum spec_weight;
        if constexpr (is_monochrome_v<Spectrum>) {
            std::tie(wavelengths, spec_weight) = sample_uniform_spectrum(wav_sample);
        } else if constexpr (is_rgb_v<Spectrum>) {
            NotImplementedError("Sampling rays in RGB mode");
        } else {
            std::tie(wavelengths, spec_weight) = sample_rgb_spectrum(wav_sample);
        }

        RayDifferential3f ray;
        ray.time = time;
        ray.wavelength = wavelengths;

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

        return std::make_pair(ray, spec_weight);
    }

    BoundingBox3f bbox() const override {
        return m_world_transform->translation_bounds();
    }

    void set_crop_window(const Vector2i &crop_size, const Point2i &crop_offset) override {
        Base::set_crop_window(crop_size, crop_offset);
        update_camera_transforms();
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

private:
    Transform4f m_camera_to_sample;
    Transform4f m_sample_to_camera;
    BoundingBox2f m_image_rect;
    Float m_normalization;
    Float m_x_fov;
    Vector3f m_dx, m_dy;
};

MTS_IMPLEMENT_PLUGIN(PerspectiveCamera, ProjectiveCamera, "Perspective Camera");
NAMESPACE_END(mitsuba)
