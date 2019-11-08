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

template <typename Float, typename Spectrum>
Sensor<Float, Spectrum>::Sensor(const Properties &props) : Base(props) {
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
        Properties props_film("hdrfilm");
        m_film = static_cast<Film *>(pmgr->create_object<Film>(props_film));
    }

    if (!m_sampler) {
        // Instantiate an independent filter with 4 samples/pixel if none was specified
        Properties props_sampler("independent");
        props_sampler.set_int("sample_count", 4);
        m_sampler = static_cast<Sampler *>(pmgr->create_object<Sampler>(props_sampler));
    }

    m_aspect = m_film->size().x() / (ScalarFloat) m_film->size().y();
    m_resolution = ScalarVector2f(m_film->crop_size());
}

template <typename Float, typename Spectrum> Sensor<Float, Spectrum>::~Sensor() {}

template <typename Float, typename Spectrum>
std::pair<typename Sensor<Float, Spectrum>::RayDifferential3f, Spectrum>
Sensor<Float, Spectrum>::sample_ray_differential(Float time, Float sample1, const Point2f &sample2,
                                                 const Point2f &sample3, Mask active) const {

    auto [temp_ray, result_spec] = sample_ray(time, sample1, sample2, sample3, active);

    RayDifferential result_ray(temp_ray);

    Vector2f dx(1.f / m_resolution.x(), 0.f);
    Vector2f dy(0.f, 1.f / m_resolution.y());

    // Sample a result_ray for X+1
    Spectrum unused;
    std::tie(temp_ray, unused) = sample_ray(time, sample1, sample2 + dx, sample3, active);

    result_ray.o_x = temp_ray.o;
    result_ray.d_x = temp_ray.d;

    // Sample a result_ray for Y+1
    std::tie(temp_ray, unused) = sample_ray(time, sample1, sample2 + dy, sample3, active);

    result_ray.o_y = temp_ray.o;
    result_ray.d_y = temp_ray.d;
    result_ray.has_differentials = true;

    return { result_ray, result_spec };
}

template <typename Float, typename Spectrum>
void Sensor<Float, Spectrum>::set_crop_window(const ScalarVector2i &crop_size,
                                              const ScalarPoint2i &crop_offset) {
    m_film->set_crop_window(crop_size, crop_offset);
    m_resolution = ScalarVector2f(m_film->crop_size());
}

// =============================================================================
// ProjectiveCamera interface
// =============================================================================

template <typename Float, typename Spectrum>
ProjectiveCamera<Float, Spectrum>::ProjectiveCamera(const Properties &props) : Base(props) {
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

template <typename Float, typename Spectrum>
ProjectiveCamera<Float, Spectrum>::~ProjectiveCamera() { }

MTS_INSTANTIATE_OBJECT(Sensor)
MTS_INSTANTIATE_OBJECT(ProjectiveCamera)
NAMESPACE_END(mitsuba)
