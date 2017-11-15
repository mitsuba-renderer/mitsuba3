#include <mitsuba/render/sensor.h>

#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/render/film.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

// =============================================================================
// Sensor interface
// =============================================================================

Sensor::Sensor(const Properties &props)
    : Endpoint(props) {
    m_shutter_open      = props.float_("shutter_open", 0.f);
    m_shutter_open_time = props.float_("shutter_close", 0.f) - m_shutter_open;

    if (m_shutter_open_time < 0)
        Throw("Shutter opening time must be less than or equal to the shutter "
              "closing time!");

    if (m_shutter_open_time == 0)
        m_type |= EDeltaTime;

    for (auto &kv : props.objects()) {
        auto *film = dynamic_cast<Film *>(kv.second.get());
        auto *sampler = dynamic_cast<Sampler *>(kv.second.get());

        if (film) {
            if (m_film)
                Throw("Only one film can be specified per sensor.");
            m_film = film;
        } else if (sampler) {
            if (m_sampler)
                Throw("Only one sampler can be specified per sensor.");
            m_sampler = sampler;
        }
    }

    configure();
}

Sensor::~Sensor() { }

// Sensor::Sensor(Stream *stream, InstanceManager *manager)
//  : Endpoint(stream, manager) {
//     m_film = static_cast<Film *>(manager->get_instance(stream));
//     m_sampler = static_cast<Sampler *>(manager->get_instance(stream));
//     m_shutter_open = stream->readFloat();
//     m_shutter_open_time = stream->readFloat();
// }


// void Sensor::serialize(Stream *stream, InstanceManager *manager) const {
//     Endpoint::serialize(stream, manager);
//     manager->serialize(stream, m_film.get());
//     manager->serialize(stream, m_sampler.get());
//     stream->writeFloat(m_shutter_open);
//     stream->writeFloat(m_shutter_open_time);
// }

void Sensor::configure() {
    if (!m_film) {
        // Instantiate an EXR film by default.
        Properties props("hdrfilm");
        m_film = static_cast<Film *>(
            PluginManager::instance()->create_object<Film>(props)
        );
    }

    if (!m_sampler) {
        // Instantiate an independent filter with 4 samples/pixel by default.
        Properties props("independent");
        props.set_int("sample_count", 4);
        m_sampler = static_cast<Sampler *>(
            PluginManager::instance()->create_object<Sampler>(props)
        );
    }

    m_aspect = m_film->size().x() / (Float) m_film->size().y();

    m_resolution = Vector2f(m_film->crop_size());
    m_inv_resolution = Vector2f(1.0f / m_resolution.x(),
                                1.0f / m_resolution.y());
}


template <typename Value, typename Point2, typename RayDifferential,
          typename Spectrum, typename Mask>
std::pair<RayDifferential, Spectrum> Sensor::sample_ray_differential_impl(
    const Point2 &sample_position, const Point2 &aperture_sample,
    Value time_sample, const Mask &active) const {
    using Ray3 = Ray<Point<Value, 3>>;

    Ray3 temp_ray;
    Spectrum result, unused;

    std::tie(temp_ray, result) = sample_ray(
        sample_position, aperture_sample, time_sample, active);
    RayDifferential ray(temp_ray);

    // Sample a ray for X+1
    std::tie(temp_ray, unused) = sample_ray(
        sample_position + Vector2f(1, 0), aperture_sample, time_sample, active);
    ray.o_x = temp_ray.o;
    ray.d_x = temp_ray.d;

    // Sample a ray for Y+1
    std::tie(temp_ray, unused) = sample_ray(
        sample_position + Vector2f(0, 1), aperture_sample, time_sample, active);
    ray.o_y = temp_ray.o;
    ray.d_y = temp_ray.d;
    ray.has_differentials = true;

    return { ray, result };
}

std::pair<RayDifferential3f, Spectrumf> Sensor::sample_ray_differential(
        const Point2f &sample_position, const Point2f &aperture_sample,
        Float time_sample) const {
    return sample_ray_differential_impl(sample_position, aperture_sample,
                                        time_sample);
}
std::pair<RayDifferential3fP, SpectrumfP> Sensor::sample_ray_differential(
        const Point2fP &sample_position, const Point2fP &aperture_sample,
        FloatP time_sample, const mask_t<FloatP> &active) const {
    return sample_ray_differential_impl(sample_position, aperture_sample,
                                        time_sample, active);
}

std::pair<Spectrumf, Point2f> Sensor::eval(const SurfaceInteraction3f &/*its*/,
                                           const Vector3f &/*d*/) const {
    Log(EError, "eval(const SurfaceInteraction3f &, const Vector3f &)"
                " is not implemented!");
    return { Spectrumf(0.0f), Point2f(0.0f) };
}
std::pair<SpectrumfP, Point2fP> Sensor::eval(
        const SurfaceInteraction3fP &/*its*/, const Vector3fP &/*d*/,
        const mask_t<FloatP> &/*active*/) const {
    Log(EError, "eval(const SurfaceInteraction3fP &, const Vector3fP &)"
                " is not implemented!");
    return { SpectrumfP(0.0f), Point2fP(0.0f) };
}

std::pair<bool, Point2f> Sensor::get_sample_position(
        const PositionSample3f &/*pRec*/,
        const DirectionSample3f &/*dRec*/) const {
    Log(EError, "get_sample_position(const PositionSample3f &, "
                "const DirectionSample3f &) is not implemented!");
    return { false, Point2f(0.0f) };
}
std::pair<BoolP, Point2fP> Sensor::get_sample_position(
        const PositionSample3fP &/*pRec*/, const DirectionSample3fP &/*dRec*/,
        const mask_t<FloatP> &/*active*/) const {
    Log(EError, "get_sample_position(const PositionSample3fP &, "
                "const DirectionSample3fP &) is not implemented!");
    return { BoolP(false), Point2fP(0.0f) };
}

template <typename Value, typename Measure, typename Ray, typename Mask>
Value Sensor::pdf_time(const Ray &/*ray*/, Measure measure,
                       const Mask &/*active*/) const {
    Value res(0.0f);

    if (m_shutter_open_time == 0)
        masked(res, eq(measure, EDiscrete)) = 1.0f;
    else if (m_shutter_open_time > 0)
        masked(res, eq(measure, ELength)) = 1.0f / Value(m_shutter_open_time);

    // All other cases: 0.0f.

    return res;
}

void Sensor::set_shutter_open_time(Float time) {
    m_shutter_open_time = time;
    if (m_shutter_open_time == 0)
        m_type |= EDeltaTime;
    else
        m_type &= ~EDeltaTime;
}

MTS_IMPLEMENT_CLASS(Sensor, Endpoint)


// =============================================================================
// ProjectiveCamera interface
// =============================================================================

ProjectiveCamera::ProjectiveCamera(const Properties &props) : Sensor(props) {
    /* Distance to the near clipping plane */
    m_near_clip = props.float_("near_clip", 1e-2f);
    /* Distance to the far clipping plane */
    m_far_clip = props.float_("far_clip", 1e4f);
    /* Distance to the focal plane */
    m_focus_distance = props.float_("focus_distance", m_far_clip);

    if (m_near_clip <= 0.0f)
        Log(EError, "The 'near_clip' parameter must be greater than zero!");
    if (m_near_clip >= m_far_clip)
        Log(EError, "The 'near_clip' parameter must be smaller than 'far_clip'.");

    m_type |= EProjectiveCamera;

    configure();
}

// ProjectiveCamera::ProjectiveCamera(Stream *stream, InstanceManager *manager)
//     : Sensor(stream, manager) {
//     m_near_clip = stream->readFloat();
//     m_far_clip = stream->readFloat();
//     m_focus_distance = stream->readFloat();
// }

// void ProjectiveCamera::serialize(Stream *stream, InstanceManager *manager) const {
//     Sensor::serialize(stream, manager);
//     stream->writeFloat(m_near_clip);
//     stream->writeFloat(m_far_clip);
//     stream->writeFloat(m_focus_distance);
// }

ProjectiveCamera::~ProjectiveCamera() { }

void ProjectiveCamera::set_focus_distance(Float focus_distance) {
    if (m_focus_distance != focus_distance) {
        m_focus_distance = focus_distance;
        m_properties.set_float("focus_distance", focus_distance, false);
    }
}

void ProjectiveCamera::set_near_clip(Float near_clip) {
    if (m_near_clip != near_clip) {
        m_near_clip = near_clip;
        m_properties.set_float("near_clip", near_clip, false);
    }
}

void ProjectiveCamera::set_far_clip(Float far_clip) {
    if (m_far_clip != far_clip) {
        m_far_clip = far_clip;
        m_properties.set_float("far_clip", far_clip, false);
    }
}

MTS_IMPLEMENT_CLASS(ProjectiveCamera, Sensor)


// =============================================================================
// PerspectiveCamera interface
// =============================================================================

PerspectiveCamera::PerspectiveCamera(const Properties &props)
    : ProjectiveCamera(props), m_x_fov(0.0f) {
    props.mark_queried("fov");
    props.mark_queried("fov_axis");
    props.mark_queried("focal_length");

    if (m_properties.has_property("fov") && m_properties.has_property("focal_length"))
        Log(EError, "Please specify either a focal length ('focal_length') or a "
            "field of view ('fov')!");

    configure();
}

// PerspectiveCamera::PerspectiveCamera(Stream *stream, InstanceManager *manager)
//     : ProjectiveCamera(stream, manager), m_x_fov(0.0f) {
//     set_x_fov(stream->readFloat());
// }

// void PerspectiveCamera::serialize(Stream *stream, InstanceManager *manager) const {
//     ProjectiveCamera::serialize(stream, manager);
//     stream->writeFloat(m_x_fov);
// }

PerspectiveCamera::~PerspectiveCamera() { }

void PerspectiveCamera::configure() {
    if (m_x_fov != 0)
        return;

    if (m_properties.has_property("fov")) {
        Float fov = m_properties.float_("fov");

        std::string fov_axis =
            string::to_lower(m_properties.string("fov_axis", "x"));

        if (fov_axis == "smaller")
            fov_axis = m_aspect > 1 ? "y" : "x";
        else if (fov_axis == "larger")
            fov_axis = m_aspect > 1 ? "x" : "y";

        if (fov_axis == "x")
            set_x_fov(fov);
        else if (fov_axis == "y")
            set_y_fov(fov);
        else if (fov_axis == "diagonal")
            set_diagonal_fov(fov);
        else
            Log(EError, "The 'fov_axis' parameter must be set "
                "to one of 'smaller', 'larger', 'diagonal', 'x', or 'y'!");
    } else {
        std::string f = m_properties.string("focal_length", "50mm");
        if (string::ends_with(f, "mm"))
            f = f.substr(0, f.length()-2);

        char *end_ptr = nullptr;
        Float value = (Float) strtod(f.c_str(), &end_ptr);
        if (*end_ptr != '\0') {
            Log(EError, "Could not parse the focal length (must be of the form "
                "<x>mm, where <x> is a positive integer)!");
        }

        m_properties.remove_property("focal_length");
        set_diagonal_fov(2 * rad_to_deg(
            std::atan(std::sqrt(Float(36*36+24*24)) / (2.0f * value))
        ));
    }
}

void PerspectiveCamera::set_x_fov(Float x_fov) {
    if (x_fov <= 0.0f || x_fov >= 180.0f) {
        Log(EError, "The horizontal field of view must be "
                    "in the interval (0, 180)!");
    }
    if (x_fov != m_x_fov) {
        m_x_fov = x_fov;
        m_properties.set_float("fov", x_fov, false);
        m_properties.set_string("fov_axis", "x", false);
    }
}

void PerspectiveCamera::set_y_fov(Float yfov) {
    set_x_fov(rad_to_deg(2 * std::atan(
        std::tan(0.5f * deg_to_rad(yfov)) * m_aspect
    )));
}

void PerspectiveCamera::set_diagonal_fov(Float dfov) {
    Float diagonal = 2 * std::tan(0.5f * deg_to_rad(dfov));
    Float width = diagonal / std::sqrt(1.0f + 1.0f / (m_aspect*m_aspect));
    set_x_fov(rad_to_deg(2.0f * std::atan(width*0.5f)));
}


Float PerspectiveCamera::y_fov() const {
    return rad_to_deg(2.0f * std::atan(
        std::tan(0.5f * deg_to_rad(m_x_fov)) / m_aspect));
}

Float PerspectiveCamera::diagonal_fov() const {
    Float width = std::tan(0.5f * deg_to_rad(m_x_fov));
    Float diagonal = width * std::sqrt(1.0f + 1.0f / (m_aspect*m_aspect));
    return rad_to_deg(2 * std::atan(diagonal));
}

MTS_IMPLEMENT_CLASS(PerspectiveCamera, ProjectiveCamera)


// =============================================================================
// Template instantiations
// =============================================================================

template MTS_EXPORT_RENDER Float  Sensor::sample_time(Float, const bool &) const;
template MTS_EXPORT_RENDER FloatP Sensor::sample_time(FloatP, const mask_t<FloatP> &) const;
template MTS_EXPORT_RENDER Float  Sensor::pdf_time(const Ray3f &, EMeasure, const bool &) const;
template MTS_EXPORT_RENDER FloatP Sensor::pdf_time(const Ray3fP &, like_t<FloatP, EMeasure>, const mask_t<FloatP> &) const;


NAMESPACE_END(mitsuba)
