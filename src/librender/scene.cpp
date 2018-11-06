#include <mitsuba/core/properties.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/plugin.h>
#include <enoki/stl.h>

NAMESPACE_BEGIN(mitsuba)

Scene::Scene(const Properties &props) {
    m_kdtree = new ShapeKDTree(props);

    for (auto &kv : props.objects()) {
        Shape *shape           = dynamic_cast<Shape *>(kv.second.get());
        Emitter *emitter       = dynamic_cast<Emitter *>(kv.second.get());
        Sensor *sensor         = dynamic_cast<Sensor *>(kv.second.get());
        Integrator *integrator = dynamic_cast<Integrator *>(kv.second.get());

        if (shape) {
            m_kdtree->add_shape(shape);
            if (shape->emitter())
                m_emitters.push_back(shape->emitter());
        } else if (emitter) {
            m_emitters.push_back(emitter);
            if (emitter->is_environment()) {
                if (m_environment)
                    Throw("Only one environment emitter can be specified per scene.");
                m_environment = emitter;
            }
        } else if (sensor) {
            m_sensors.push_back(sensor);
        } else if (integrator) {
            if (m_integrator)
                Throw("Only one integrator can be specified per scene.");
            m_integrator = integrator;
        }
    }
    m_kdtree->build();

    if (m_sensors.size() == 0) {
        Log(EWarn, "No sensors found! Instantiating a perspective camera..");
        Properties props("perspective");
        props.set_float("fov", 45.0f);

        /* Create a perspective camera with a 45 deg. field of view
           and positioned so that it can see the entire scene */
        BoundingBox3f bbox = m_kdtree->bbox();
        if (bbox.valid()) {
            Point3f center = bbox.center();
            Vector3f extents = bbox.extents();

            Float distance =
                hmax(extents) / (2.f * std::tan(45.f * .5f * math::Pi / 180));

            props.set_float("far_clip", hmax(extents) * 5 + distance);
            props.set_float("near_clip", distance / 100);

            props.set_float("focus_distance", distance + extents.z() / 2);
            props.set_transform(
                "to_world", Transform4f::translate(Vector3f(
                                center.x(), center.y(), bbox.min.z() - distance)));
        }

        m_sensors.push_back(
            PluginManager::instance()->create_object<Sensor>(props));
    }

    if (!m_integrator) {
        Log(EWarn, "No integrator found! Instantiating a path tracer..");
        m_integrator = PluginManager::instance()->create_object<Integrator>(
            Properties("path"));
    }

    // Precompute a discrete distribution over emitters
    m_emitter_distr = DiscreteDistribution(m_emitters.size());
    for (size_t i = 0; i < m_emitters.size(); ++i)
        m_emitter_distr.append(1.f);  // Simple uniform distribution for now.
    if (m_emitters.size() > 0)
        m_emitter_distr.normalize();

    // Create emitters' shapes (environment luminaires)
    for (auto emitter: m_emitters)
        emitter->create_shape(this);

    if (!m_sensors.empty()) {
        m_sampler = m_sensors.front()->sampler();
        if (m_sensors.size() > 1)
            Throw("Only one sensor is supported at the moment.");
    }
}

Scene::~Scene() { }

// =============================================================
//! Ray tracing-related methods
// =============================================================

SurfaceInteraction3f Scene::ray_intersect(const Ray3f &ray) const {
    ScopedPhase sp(EProfilerPhase::ERayIntersect);
    Float cache[MTS_KD_INTERSECTION_CACHE_SIZE];

    #if defined(MTS_DISABLE_KDTREE)
        auto [hit, hit_t] = m_kdtree->ray_intersect_naive<false>(ray, cache);
    #else
        auto [hit, hit_t] = m_kdtree->ray_intersect<false>(ray, cache);
    #endif

    SurfaceInteraction3f si;

    if (likely(hit)) {
        ScopedPhase sp2(EProfilerPhase::ECreateSurfaceInteraction);
        si = m_kdtree->create_surface_interaction(ray, hit_t, cache);
    } else {
        si.wavelengths = ray.wavelengths;
        si.wi = -ray.d;
    }

    return si;
}

SurfaceInteraction3fP Scene::ray_intersect(const Ray3fP &ray, MaskP active) const {
    ScopedPhase sp(EProfilerPhase::ERayIntersectP);
    FloatP cache[MTS_KD_INTERSECTION_CACHE_SIZE];

    #if defined(MTS_DISABLE_KDTREE)
        auto [hit, hit_t] = m_kdtree->ray_intersect_naive<false>(ray, cache, active);
    #else
        auto [hit, hit_t] = m_kdtree->ray_intersect<false>(ray, cache, active);
    #endif

    SurfaceInteraction3fP si;

    if (likely(any(hit))) {
        ScopedPhase sp2(EProfilerPhase::ECreateSurfaceInteractionP);
        si = m_kdtree->create_surface_interaction(ray, hit_t, cache, active && hit);
    } else {
        si.wavelengths = ray.wavelengths;
        si.wi = -ray.d;
    }

    return si;
}

bool Scene::ray_test(const Ray3f &ray) const {
    ScopedPhase p(EProfilerPhase::ERayTest);

    #if defined(MTS_DISABLE_KDTREE)
        return m_kdtree->ray_intersect_naive<true>(ray).first;
    #else
        return m_kdtree->ray_intersect<true>(ray).first;
    #endif
}

MaskP Scene::ray_test(const Ray3fP &ray, MaskP active) const {
    ScopedPhase p(EProfilerPhase::ERayTestP);

    #if defined(MTS_DISABLE_KDTREE)
        return m_kdtree->ray_intersect_naive<true>(ray, (FloatP *) nullptr, active).first;
    #else
        return m_kdtree->ray_intersect<true>(ray, nullptr, active).first;
    #endif
}

SurfaceInteraction3f Scene::ray_intersect_naive(const Ray3f &ray) const {
    ScopedPhase sp(EProfilerPhase::ERayIntersect);
    Float cache[MTS_KD_INTERSECTION_CACHE_SIZE];
    auto [hit, hit_t] = m_kdtree->ray_intersect_naive<false>(ray, cache);

    SurfaceInteraction3f si;
    if (likely(hit)) {
        ScopedPhase sp2(EProfilerPhase::ECreateSurfaceInteraction);
        si = m_kdtree->create_surface_interaction(ray, hit_t, cache);
    }
    return si;
}

SurfaceInteraction3fP Scene::ray_intersect_naive(const Ray3fP &ray, MaskP active) const {
    ScopedPhase sp(EProfilerPhase::ERayIntersectP);
    FloatP cache[MTS_KD_INTERSECTION_CACHE_SIZE];
    auto [hit, hit_t] = m_kdtree->ray_intersect_naive<false>(ray, cache, active);

    SurfaceInteraction3fP si;
    if (likely(any(hit))) {
        ScopedPhase sp2(EProfilerPhase::ECreateSurfaceInteractionP);
        si = m_kdtree->create_surface_interaction(ray, hit_t, cache, active && hit);
    }
    return si;
}


// =============================================================
//! Sampling-related methods
// =============================================================

template <typename Interaction, typename Value, typename Spectrum,
          typename Point3, typename Point2, typename DirectionSample,
          typename Mask>
std::pair<DirectionSample, Spectrum>
Scene::sample_emitter_direction_impl(const Interaction &ref, Point2 sample,
                                     bool test_visibility, Mask active) const {
    ScopedPhase sp(is_array_v<Value> ?
            EProfilerPhase::ESampleEmitterDirectionP :
            EProfilerPhase::ESampleEmitterDirection);

    using EmitterPtr = replace_scalar_t<Value, const Emitter *>;
    using Index = uint_array_t<Value>;
    using Ray = Ray<Point<Value, 3>>;

    DirectionSample ds;
    Spectrum spec = 0.f;

    if (likely(!m_emitters.empty())) {
        // Randomly pick an emitter according to the precomputed emitter distribution
        Index index;
        Value emitter_pdf;
        std::tie(index, emitter_pdf, sample.x()) =
            m_emitter_distr.sample_reuse_pdf(sample.x());
        EmitterPtr emitter = gather<EmitterPtr>(m_emitters.data(), index, active);

        // Sample a direction towards the emitter
        std::tie(ds, spec) = emitter->sample_direction(ref, sample, active);
        active &= neq(ds.pdf, 0.f);

        // Perform a visibility test if requested
        if (test_visibility && any(active)) {
            Ray ray(ref.p, ds.d, math::Epsilon * (1.f + hmax(abs(ref.p))),
                    ds.dist * (1.f - math::ShadowEpsilon),
                    ref.time, ref.wavelengths);
            spec[ray_test(ray, active)] = 0.f;
        }

        // Account for the discrete probability of sampling this emitter
        ds.pdf *= emitter_pdf;
        spec *= rcp(emitter_pdf);
    }

    return { ds, spec };
}

std::pair<DirectionSample3f, Spectrumf>
Scene::sample_emitter_direction(const Interaction3f &ref,
                                const Point2f &sample,
                                bool test_visibility) const {
    return sample_emitter_direction_impl(ref, sample, test_visibility, true);
}

std::pair<DirectionSample3fP, SpectrumfP>
Scene::sample_emitter_direction(const Interaction3fP &ref,
                                const Point2fP &sample,
                                bool test_visibility,
                                MaskP active) const {
    return sample_emitter_direction_impl(ref, sample, test_visibility, active);
}

template <typename Interaction, typename DirectionSample, typename Value,
          typename Mask>
Value Scene::pdf_emitter_direction_impl(const Interaction &ref,
                                        const DirectionSample &ds,
                                        Mask active) const {
    using EmitterPtr = replace_scalar_t<Value, const Emitter *>;
    return reinterpret_array<EmitterPtr>(ds.object)->pdf_direction(ref, ds, active) *
        m_emitter_distr.eval(0);
}

Float Scene::pdf_emitter_direction(const Interaction3f &ref,
                                   const DirectionSample3f &ds) const {
    return pdf_emitter_direction_impl(ref, ds, true);
}

FloatP Scene::pdf_emitter_direction(const Interaction3fP &ref,
                                    const DirectionSample3fP &ds,
                                    MaskP active) const {
    return pdf_emitter_direction_impl(ref, ds, active);
}

// =============================================================
//! @{ \name Explicit template instantiations & misc.
// =============================================================
std::string Scene::to_string() const {
    return tfm::format("Scene[\n"
        "  kdtree = %s\n"
        "]",
        string::indent(m_kdtree->to_string())
    );
}

MTS_IMPLEMENT_CLASS(Scene, Object)
NAMESPACE_END(mitsuba)
