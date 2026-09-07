#pragma once

#include <mitsuba/core/srect.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/scene.h>
#include <drjit/while_loop.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Portal sampling strategy for environment emitters
 *
 * This helper holds the scene's packed portal records (see
 * ``Scene::portal_data()``) and samples directions towards the portals that
 * face the reference point, uniformly in solid angle. An emitter calls
 * ``choose()`` to decide between this strategy and its own one, ``sample()``
 * to generate the portal direction, and ``pdf()`` to mix the two densities.
 */
template <typename Float, typename Spectrum>
struct PortalSampler {
    MI_IMPORT_CORE_TYPES()
    using Scene      = mitsuba::Scene<Float, Spectrum>;
    using PortalData = typename Scene::PortalData;

    PortalData data;

    void set_scene(const Scene *scene) { data = scene->portal_data(); }

    bool empty() const { return data.count.scalar() == 0; }

    /// Load portal ``i`` as seen from ``p``, and whether it can be sampled from there
    std::pair<SphericalRectangle3f, Mask> load(const UInt32 &i, const Point3f &p) const {
        auto r = dr::gather<dr::Array<Float, 12>>(data.records, i, i < data.count.value());
        Point3f o(r[0], r[1], r[2]);
        Vector3f dx(r[3], r[4], r[5]), dy(r[6], r[7], r[8]);
        Normal3f n(r[9], r[10], r[11]);
        SphericalRectangle3f rect(p, o, dx, dy);
        return { rect, dr::dot(n, p - o) > 0.f && !rect.degenerate };
    }

    /// Outcome of ``choose()``
    struct Choice {
        Mask use_portal;
        /// Sample remapped to [0, 1)^2 for the chosen strategy
        Point2f sample;
        /// Number of portals usable from the reference point
        UInt32 n_usable;
    };

    /**
     * Choose the portal strategy with the scene's ``portal_weight``
     * probability if any portal is usable from ``p``, and remap the sample
     */
    Choice choose(const Point3f &p, const Point2f &sample) const {
        if (empty())
            return { false, sample, 0 };

        UInt32 i = 0, n_usable = 0;
        dr::tie(i, n_usable) = dr::while_loop(
            dr::make_tuple(i, n_usable),
            [n = data.count.value()](const UInt32 &i, const UInt32 &) { return i < n; },
            [&](UInt32 &i, UInt32 &n_usable) {
                n_usable += UInt32(load(i, p).second);
                ++i;
            },
            "Portal count");

        Float w = dr::select(n_usable > 0, Float(data.weight), 0.f);
        Mask use_portal = sample.x() < w;
        Point2f s(dr::select(use_portal, sample.x() / w, (sample.x() - w) / (1.f - w)),
                  sample.y());

        return { use_portal, s, n_usable };
    }

    /// Sample a direction through a uniformly chosen usable portal
    Vector3f sample(const Point3f &p, const Choice &c) const {
        Float sx = c.sample.x() * Float(c.n_usable);
        UInt32 k = dr::minimum(UInt32(sx), c.n_usable - 1);
        sx -= Float(k);

        // Find the k-th usable portal
        UInt32 i = 0, seen = 0, chosen = 0;
        dr::tie(i, seen, chosen) = dr::while_loop(
            dr::make_tuple(i, seen, chosen),
            [n = data.count.value(), k](const UInt32 &i, const UInt32 &seen, const UInt32 &) {
                return i < n && seen <= k;
            },
            [&](UInt32 &i, UInt32 &seen, UInt32 &chosen) {
                Mask usable = load(i, p).second;
                chosen = dr::select(usable && seen == k, i, chosen);
                seen += UInt32(usable);
                ++i;
            },
            "Portal selection");

        return load(chosen, p).first.sample(Point2f(sx, c.sample.y()));
    }

    /**
     * Mix the emitter's own density ``base_pdf`` of direction ``d`` with the
     * portal density, which averages over all usable portals
     */
    Float pdf(const Point3f &p, const Vector3f &d, const Float &base_pdf) const {
        UInt32 i = 0, n_usable = 0;
        Float sum = 0.f;

        dr::tie(i, n_usable, sum) = dr::while_loop(
            dr::make_tuple(i, n_usable, sum),
            [n = data.count.value()](const UInt32 &i, const UInt32 &, const Float &) { return i < n; },
            [&](UInt32 &i, UInt32 &n_usable, Float &sum) {
                auto [rect, usable] = load(i, p);
                n_usable += UInt32(usable);
                sum += dr::select(usable, rect.pdf(d), 0.f);
                ++i;
            },
            "Portal pdf");

        return dr::select(n_usable > 0,
                          dr::lerp(base_pdf, sum / Float(n_usable), Float(data.weight)),
                          base_pdf);
    }

    DRJIT_TRAVERSE(PortalSampler, data)
};

NAMESPACE_END(mitsuba)
