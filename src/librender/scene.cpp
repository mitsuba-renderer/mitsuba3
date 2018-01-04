#include <mitsuba/core/properties.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/core/profiler.h>
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
        } else if (sensor) {
            m_sensors.push_back(sensor);
        } else if (integrator) {
            if (m_integrator)
                Throw("Only one integrator can be specified per scene.");
            m_integrator = integrator;
        } else {
            Throw("Tried to add an unsupported object of type %s", kv.second);
        }
    }

    m_kdtree->build();

    // Precompute the emitters PDF.
    m_emitter_distr = DiscreteDistribution(m_emitters.size());
    for (size_t i = 0; i < m_emitters.size(); ++i)
        m_emitter_distr.append(1.f);  // Simple uniform distribution for now.
    if (m_emitters.size() > 0)
        m_emitter_distr.normalize();

    if (m_sensors.size() > 0) {
        if (m_sensors.size() > 1)
            Throw("Only one sensor is supported ATM.");
        m_sampler = m_sensors.front()->sampler();
    }
}

Scene::~Scene() { }

// =============================================================
//! Ray tracing-related methods
// =============================================================

SurfaceInteraction3f Scene::ray_intersect(const Ray3f &ray) const {
    ScopedPhase sp(EProfilerPhase::ERayIntersect);
    Float hit_t, cache[MTS_KD_INTERSECTION_CACHE_SIZE];
    bool hit;
    std::tie(hit, hit_t) = m_kdtree->ray_intersect<false>(ray, cache);

    SurfaceInteraction3f si;
    if (likely(hit)) {
        ScopedPhase sp2(EProfilerPhase::ECreateSurfaceInteraction);
        si = m_kdtree->create_surface_interaction(ray, hit_t, cache);
    }
    return si;
}

SurfaceInteraction3fP Scene::ray_intersect(const Ray3fP &ray, MaskP active) const {
    ScopedPhase sp(EProfilerPhase::ERayIntersectP);
    FloatP hit_t, cache[MTS_KD_INTERSECTION_CACHE_SIZE];
    MaskP hit;
    std::tie(hit, hit_t) = m_kdtree->ray_intersect<false>(ray, cache, active);

    SurfaceInteraction3fP si;
    if (likely(any(hit))) {
        ScopedPhase sp2(EProfilerPhase::ECreateSurfaceInteractionP);
        si = m_kdtree->create_surface_interaction(ray, hit_t, cache, active && hit);
    }
    return si;
}

bool Scene::ray_test(const Ray3f &ray) const {
    ScopedPhase p(EProfilerPhase::ERayTest);
    return m_kdtree->ray_intersect<true>(ray).first;
}

MaskP Scene::ray_test(const Ray3fP &ray, MaskP active) const {
    ScopedPhase p(EProfilerPhase::ERayTestP);
    return m_kdtree->ray_intersect<true>(ray, nullptr, active).first;
}

SurfaceInteraction3f Scene::ray_intersect_naive(const Ray3f &ray) const {
    ScopedPhase sp(EProfilerPhase::ERayIntersect);
    Float hit_t, cache[MTS_KD_INTERSECTION_CACHE_SIZE];
    bool hit;
    std::tie(hit, hit_t) = m_kdtree->ray_intersect_naive<false>(ray, cache);

    SurfaceInteraction3f si;
    if (likely(hit)) {
        ScopedPhase sp2(EProfilerPhase::ECreateSurfaceInteraction);
        si = m_kdtree->create_surface_interaction(ray, hit_t, cache);
    }
    return si;
}

SurfaceInteraction3fP Scene::ray_intersect_naive(const Ray3fP &ray, MaskP active) const {
    ScopedPhase sp(EProfilerPhase::ERayIntersectP);
    FloatP hit_t, cache[MTS_KD_INTERSECTION_CACHE_SIZE];
    MaskP hit;
    std::tie(hit, hit_t) = m_kdtree->ray_intersect_naive<false>(ray, cache, active);

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
Scene::sample_emitter_direction_impl(const Interaction &it, Point2 sample,
                                     bool test_visibility, Mask active) const {
    ScopedPhase sp(is_array<Value>::value ?
            EProfilerPhase::ESampleEmitterDirectionP :
            EProfilerPhase::ESampleEmitterDirection);

    using EmitterPtr = like_t<Value, const Emitter *>;
    using Index = uint_array_t<Value>;
    using Ray = Ray<Point<Value, 3>>;

    // Randomly pick an emitter according to the precomputed emitter distribution
    Index index;
    Value emitter_pdf;
    std::tie(index, emitter_pdf, sample.x()) =
        m_emitter_distr.sample_reuse_pdf(sample.x());
    EmitterPtr emitter = gather<EmitterPtr>(m_emitters.data(), index, active);

    // Sample a direction towards the emitter
    DirectionSample ds;
    Spectrum spec;
    std::tie(ds, spec) = emitter->sample_direction(it, sample, active);
    active = active && neq(ds.pdf, 0.f);

    // Perform a visibility test if requested
    if (test_visibility && any(active)) {
        Ray ray(it.p, ds.d, math::Epsilon,
                ds.dist * (1.f - math::ShadowEpsilon),
                it.time, it.wavelengths);
        Mask hit = ray_test(ray, active);
        masked(spec, hit) = Spectrum(0.f);
    }

    // Account for the discrete probability of sampling this emitter
    ds.pdf *= emitter_pdf;
    spec /= emitter_pdf;

    return { ds, spec };
}


std::pair<DirectionSample3f, Spectrumf>
Scene::sample_emitter_direction(const Interaction3f &it,
                                const Point2f &sample,
                                bool test_visibility) const {
    return sample_emitter_direction_impl(it, sample, test_visibility, true);
}

std::pair<DirectionSample3fP, SpectrumfP>
Scene::sample_emitter_direction(const Interaction3fP &it,
                                const Point2fP &sample,
                                bool test_visibility,
                                MaskP active) const {
    return sample_emitter_direction_impl(it, sample, test_visibility, active);
}

template <typename Interaction, typename DirectionSample, typename Value,
          typename Mask>
Value Scene::pdf_emitter_direction_impl(const Interaction &it,
                                        const DirectionSample &ds,
                                        Mask active) const {
    using EmitterPtr = like_t<Value, const Emitter *>;
    return reinterpret_array<EmitterPtr>(ds.object)->pdf_direction(it, ds, active) *
        m_emitter_distr[0];
}

Float Scene::pdf_emitter_direction(const Interaction3f &it,
                                   const DirectionSample3f &ds) const {
    return pdf_emitter_direction_impl(it, ds, true);
}

FloatP Scene::pdf_emitter_direction(const Interaction3fP &it,
                                    const DirectionSample3fP &ds,
                                    MaskP active) const {
    return pdf_emitter_direction_impl(it, ds, active);
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
