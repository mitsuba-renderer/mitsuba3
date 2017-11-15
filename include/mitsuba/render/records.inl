#pragma once

#include <mitsuba/render/records.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Point3> typename RadianceRecord<Point3>::Mask
RadianceRecord<Point3>::ray_intersect(const RayDifferential &ray,
                                      const Mask &active) {
    Mask hit;
    // Intersect against the scene, and fill this record's fields at the same
    // time if there's any interesting intersection.
    std::tie(hit, dist) = scene->ray_intersect(ray, ray.mint, ray.maxt,
                                               its, active);

    if (unlikely(any(active & hit & its.is_medium_transition())))
        Throw("Not implemented case: intersection with medium transition.");
    else if (unlikely(any(active & ~hit & neq(medium, nullptr))))
        Throw("Not implemented case: no intersection, but medium present.");

    masked(alpha, active) = select(hit, Value(1.0f), Value(0.0f));
    return hit;
}

NAMESPACE_END(mitsuba)
