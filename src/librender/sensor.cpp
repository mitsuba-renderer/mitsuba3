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

    auto pmgr = PluginManager::instance();
    if (!m_film) {
        // Instantiate an high dynamic range film if none was specified
        m_film = static_cast<Film *>(
            pmgr->create_object<Film>(Properties("hdrfilm")));
    }

    if (!m_sampler) {
        // Instantiate an independent filter with 4 samples/pixel if none was specified
        Properties props2("independent");
        props2.set_int("sample_count", 4);
        m_sampler = static_cast<Sampler *>(pmgr->create_object<Sampler>(props2));
    }

    m_aspect = m_film->size().x() / (Float) m_film->size().y();

    m_resolution = Vector2f(m_film->crop_size());
}

Sensor::~Sensor() { }

template <typename Value, typename Point2, typename RayDifferential,
          typename Spectrum, typename Mask>
std::pair<RayDifferential, Spectrum> Sensor::sample_ray_differential_impl(
    Value time, Value sample1, const Point2 &sample2, const Point2 &sample3,
    Mask active) const {

    auto [temp_ray, result_spec] =
        sample_ray(time, sample1, sample2, sample3, active);

    RayDifferential result_ray(temp_ray);

    Vector2f dx(1.f / m_resolution.x(), 0.f);
    Vector2f dy(0.f, 1.f / m_resolution.y());

    // Sample a result_ray for X+1
    Spectrum unused;
    std::tie(temp_ray, unused) =
        sample_ray(time, sample1, sample2 + dx, sample3, active);

    result_ray.o_x = temp_ray.o;
    result_ray.d_x = temp_ray.d;

    // Sample a result_ray for Y+1
    std::tie(temp_ray, unused) =
        sample_ray(time, sample1, sample2 + dy, sample3, active);

    result_ray.o_y = temp_ray.o;
    result_ray.d_y = temp_ray.d;
    result_ray.has_differentials = true;

    return { result_ray, result_spec };
}

std::pair<RayDifferential3f, Spectrumf>
Sensor::sample_ray_differential(Float time, Float sample1,
                                const Point2f &sample2,
                                const Point2f &sample3) const {
    return sample_ray_differential_impl(time, sample1, sample2,
                                        sample3, true);
}
std::pair<RayDifferential3fP, SpectrumfP>
Sensor::sample_ray_differential(FloatP time, FloatP sample1,
                                const Point2fP &sample2,
                                const Point2fP &sample3,
                                MaskP active) const {
    return sample_ray_differential_impl(time, sample1, sample2,
                                        sample3, active);
}

template <typename Value, typename Point2, typename RayDifferential,
          typename Spectrum, typename Mask>
std::pair<RayDifferential, MuellerMatrix<Spectrum>> Sensor::sample_ray_differential_pol_impl(
    Value time, Value sample1, const Point2 &sample2, const Point2 &sample3,
    Mask active) const {
    using Ray3          = Ray<Point<Value, 3>>;
    using MuellerMatrix = MuellerMatrix<Spectrum>;

    Ray3 temp_ray;
    MuellerMatrix result_spec, unused;

    std::tie(temp_ray, result_spec) =
        sample_ray_pol(time, sample1, sample2, sample3, active);

    RayDifferential result_ray(temp_ray);

    Vector2f dx(1.f / m_resolution.x(), 0.f);
    Vector2f dy(0.f, 1.f / m_resolution.y());

    // Sample a result_ray for X+1
    std::tie(temp_ray, unused) =
        sample_ray_pol(time, sample1, sample2 + dx, sample3, active);

    result_ray.o_x = temp_ray.o;
    result_ray.d_x = temp_ray.d;

    // Sample a result_ray for Y+1
    std::tie(temp_ray, unused) =
        sample_ray_pol(time, sample1, sample2 + dy, sample3, active);

    result_ray.o_y = temp_ray.o;
    result_ray.d_y = temp_ray.d;
    result_ray.has_differentials = true;

    return { result_ray, result_spec };
}

std::pair<RayDifferential3f, MuellerMatrixSf>
Sensor::sample_ray_differential_pol(Float time, Float sample1,
                                    const Point2f &sample2,
                                    const Point2f &sample3) const {
    return sample_ray_differential_pol_impl(time, sample1, sample2,
                                            sample3, true);
}
std::pair<RayDifferential3fP, MuellerMatrixSfP>
Sensor::sample_ray_differential_pol(FloatP time, FloatP sample1,
                                    const Point2fP &sample2,
                                    const Point2fP &sample3,
                                    MaskP active) const {
    return sample_ray_differential_pol_impl(time, sample1, sample2,
                                            sample3, active);
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

    if (m_near_clip <= 0.f)
        Throw("The 'near_clip' parameter must be greater than zero!");
    if (m_near_clip >= m_far_clip)
        Throw("The 'near_clip' parameter must be smaller than 'far_clip'.");

}

ProjectiveCamera::~ProjectiveCamera() { }

MTS_IMPLEMENT_CLASS(ProjectiveCamera, Sensor)

NAMESPACE_END(mitsuba)
