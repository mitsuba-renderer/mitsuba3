#include <mitsuba/render/scene.h>
#include <mitsuba/render/kdtree.h>

NAMESPACE_BEGIN(mitsuba)

// =============================================================
//! Ray tracing-related methods
// =============================================================

SurfaceInteraction3f Scene::ray_intersect(const Ray3f &ray) const {
    ScopedPhase sp(EProfilerPhase::ERayIntersect);
    Float cache[MTS_KD_INTERSECTION_CACHE_SIZE];

    auto [hit, hit_t] = m_kdtree->ray_intersect_havran<false>(ray, cache);

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

    auto [hit, hit_t] = m_kdtree->ray_intersect<false>(ray, cache, active);

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
    return m_kdtree->ray_intersect_havran<true>(ray).first;
}

MaskP Scene::ray_test(const Ray3fP &ray, MaskP active) const {
    ScopedPhase p(EProfilerPhase::ERayTestP);

    return m_kdtree->ray_intersect<true>(ray, nullptr, active).first;
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

NAMESPACE_END(mitsuba)
