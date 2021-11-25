#include <mitsuba/render/sensor.h>

#include <mitsuba/core/plugin.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

// =============================================================================
// Sensor interface
// =============================================================================

MTS_VARIANT Sensor<Float, Spectrum>::Sensor(const Properties &props) : Base(props) {
    m_shutter_open      = props.get<ScalarFloat>("shutter_open", 0.f);
    m_shutter_open_time = props.get<ScalarFloat>("shutter_close", 0.f) - m_shutter_open;

    if (m_shutter_open_time < 0)
        Throw("Shutter opening time must be less than or equal to the shutter "
              "closing time!");

    for (auto &[name, obj] : props.objects(false)) {
        auto *film = dynamic_cast<Film *>(obj.get());
        auto *sampler = dynamic_cast<Sampler *>(obj.get());

        if (film) {
            if (m_film)
                Throw("Only one film can be specified per sensor.");
            m_film = film;
            props.mark_queried(name);
        } else if (sampler) {
            if (m_sampler)
                Throw("Only one sampler can be specified per sensor.");
            m_sampler = sampler;
            props.mark_queried(name);
        }
    }

    auto pmgr = PluginManager::instance();
    if (!m_film) {
        // Instantiate an high dynamic range film if none was specified
        Properties props_film("hdrfilm");
        m_film = static_cast<Film *>(pmgr->create_object<Film>(props_film));
    }

    if (!m_sampler) {
        // Instantiate an independent filter with 4 samples/pixel if none was specified
        Properties props_sampler("independent");
        props_sampler.set_int("sample_count", 4);
        m_sampler = static_cast<Sampler *>(pmgr->create_object<Sampler>(props_sampler));
    }

    m_resolution = ScalarVector2f(m_film->crop_size());
}

MTS_VARIANT Sensor<Float, Spectrum>::~Sensor() {}

MTS_VARIANT std::pair<typename Sensor<Float, Spectrum>::RayDifferential3f, Spectrum>
Sensor<Float, Spectrum>::sample_ray_differential(Float time, Float sample1, const Point2f &sample2,
                                                 const Point2f &sample3, const Float &volume_sample, 
                                                 Mask active) const {
    MTS_MASKED_FUNCTION(ProfilerPhase::EndpointSampleRay, active);

    auto [temp_ray, result_spec] = sample_ray(time, sample1, sample2, sample3, volume_sample, active);

    RayDifferential result_ray(temp_ray);

    Vector2f dx(1.f / m_resolution.x(), 0.f);
    Vector2f dy(0.f, 1.f / m_resolution.y());

    // Sample a result_ray for X+1
    std::tie(temp_ray, std::ignore) = sample_ray(time, sample1, sample2 + dx, sample3, volume_sample, active);

    result_ray.o_x = temp_ray.o;
    result_ray.d_x = temp_ray.d;

    // Sample a result_ray for Y+1
    std::tie(temp_ray, std::ignore) = sample_ray(time, sample1, sample2 + dy, sample3, volume_sample, active);

    result_ray.o_y = temp_ray.o;
    result_ray.d_y = temp_ray.d;
    result_ray.has_differentials = true;

    return { result_ray, result_spec };
}

// =============================================================================
// ProjectiveCamera interface
// =============================================================================

MTS_VARIANT ProjectiveCamera<Float, Spectrum>::ProjectiveCamera(const Properties &props)
    : Base(props) {
    /* Distance to the near clipping plane */
    m_near_clip = props.get<ScalarFloat>("near_clip", 1e-2f);
    /* Distance to the far clipping plane */
    m_far_clip = props.get<ScalarFloat>("far_clip", 1e4f);
    /* Distance to the focal plane */
    m_focus_distance = props.get<ScalarFloat>("focus_distance", (float) m_far_clip);

    if (m_near_clip <= 0.f)
        Throw("The 'near_clip' parameter must be greater than zero!");
    if (m_near_clip >= m_far_clip)
        Throw("The 'near_clip' parameter must be smaller than 'far_clip'.");

}

MTS_VARIANT ProjectiveCamera<Float, Spectrum>::~ProjectiveCamera() { }

// =============================================================================
// Helper functions
// =============================================================================

double parse_fov(const Properties &props, double aspect) {
    if (props.has_property("fov") && props.has_property("focal_length"))
        Throw("Please specify either a focal length ('focal_length') or a "
                "field of view ('fov')!");

    double fov;
    std::string fov_axis;

    if (props.has_property("fov")) {
        fov = props.get<double>("fov");

        fov_axis = string::to_lower(props.string("fov_axis", "x"));

        if (fov_axis == "smaller")
            fov_axis = aspect > 1 ? "y" : "x";
        else if (fov_axis == "larger")
            fov_axis = aspect > 1 ? "x" : "y";
    } else {
        std::string f = props.string("focal_length", "50mm");
        if (string::ends_with(f, "mm"))
            f = f.substr(0, f.length() - 2);

        double value;
        try {
            value = string::stof<double>(f);
        } catch (...) {
            Throw("Could not parse the focal length (must be of the form "
                "<x>mm, where <x> is a positive integer)!");
        }

        fov = 2.0 *
                ek::rad_to_deg(ek::atan(ek::sqrt(double(36 * 36 + 24 * 24)) / (2.0 * value)));
        fov_axis = "diagonal";
    }

    double result;
    if (fov_axis == "x") {
        result = fov;
    } else if (fov_axis == "y") {
        result = ek::rad_to_deg(
            2.0 * ek::atan(ek::tan(0.5 * ek::deg_to_rad(fov)) * aspect));
    } else if (fov_axis == "diagonal") {
        double diagonal = 2.0 * ek::tan(0.5 * ek::deg_to_rad(fov));
        double width    = diagonal / ek::sqrt(1.0 + 1.0 / (aspect * aspect));
        result = ek::rad_to_deg(2.0 * ek::atan(width * 0.5));
    } else {
        Throw("The 'fov_axis' parameter must be set to one of 'smaller', "
                "'larger', 'diagonal', 'x', or 'y'!");
    }

    if (result <= 0.0 || result >= 180.0)
        Throw("The horizontal field of view must be in the range [0, 180]!");

    return result;
}

MTS_IMPLEMENT_CLASS_VARIANT(Sensor, Endpoint, "sensor")
MTS_IMPLEMENT_CLASS_VARIANT(ProjectiveCamera, Sensor)

MTS_INSTANTIATE_CLASS(Sensor)
MTS_INSTANTIATE_CLASS(ProjectiveCamera)
NAMESPACE_END(mitsuba)
