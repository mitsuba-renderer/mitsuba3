#pragma once

#include <mitsuba/core/bbox.h>
#include <mitsuba/core/frame.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Spherical rectangle
 *
 * This class represents the projection of a planar rectangle onto the unit
 * sphere centered at a reference point ``p``. It samples directions towards
 * the rectangle uniformly in solid angle using the area-preserving
 * parametrization by Ureña, Fajardo and King (EGSR 2013).
 *
 * The constructor builds a frame with ``p`` at its origin so that the
 * rectangle lies in the plane :math:`z = z_0 \le 0` and covers ``rect``.
 */
template <typename Float_> struct SphericalRectangle {
    using Float        = Float_;
    using Mask         = dr::mask_t<Float>;
    using Point2       = Point<Float, 2>;
    using Point3       = Point<Float, 3>;
    using Vector2      = Vector<Float, 2>;
    using Vector3      = Vector<Float, 3>;
    using BoundingBox2 = BoundingBox<Point2>;

    Frame<Float> frame;
    BoundingBox2 rect;
    Float z0;

    /// Parametrization constants used by ``sample()``
    Float b0, b1, k;

    /// Solid angle subtended by the rectangle
    Float solid_angle;

    /**
     * Set when the solid angle is too small or the rectangle is seen at a
     * grazing angle, in which case the parametrization is numerically
     * unreliable and ``pdf()`` returns zero.
     */
    Mask degenerate;

    /**
     * Prepare a rectangle with the given center and orthogonal half-edge
     * vectors ``dx`` and ``dy`` as seen from the reference point ``p``.
     */
    SphericalRectangle(const Point3 &p, const Point3 &center,
                       const Vector3 &dx, const Vector3 &dy) {
        Vector3 dir = center - p;
        Float hx = dr::norm(dx), hy = dr::norm(dy);

        Vector3 x = dx / hx,
                y = dy / hy,
                z = dr::cross(x, y);
        z = dr::select(dr::dot(dir, z) > 0.f, -z, z);
        frame = Frame<Float>(x, y, z);

        Vector3 c = frame.to_local(dir);
        Point2 c2(c.x(), c.y());
        Vector2 h(hx, hy);
        rect = BoundingBox2(c2 - h, c2 + h);
        z0 = c.z();

        Float x0 = rect.min.x(), y0 = rect.min.y(),
              x1 = rect.max.x(), y1 = rect.max.y();

        // n0..n3: z components of the unit normals of the planes through p
        // and the edges y = y0, x = x1, y = y1 and x = x0. g0..g3: pi/2 minus
        // the interior angles of the spherical rectangle at its corners.
        Float z0_2 = dr::square(z0),
              n0   = -y0 * dr::rsqrt(dr::square(y0) + z0_2),
              n1   =  x1 * dr::rsqrt(dr::square(x1) + z0_2),
              n2   =  y1 * dr::rsqrt(dr::square(y1) + z0_2),
              n3   = -x0 * dr::rsqrt(dr::square(x0) + z0_2),
              g0   = dr::safe_asin(-n0 * n1),
              g1   = dr::safe_asin(-n1 * n2),
              g2   = dr::safe_asin(-n2 * n3),
              g3   = dr::safe_asin(-n3 * n0);

        solid_angle = -(g0 + g1 + g2 + g3);
        b0 = n0;
        b1 = n2;
        k  = g2 + g3;

        Float n_min = dr::minimum(dr::minimum(dr::square(n0), dr::square(n1)),
                                  dr::minimum(dr::square(n2), dr::square(n3)));

        // NaN-safe conditionals
        degenerate = !(solid_angle >= 1e-5f && n_min <= 0.99999f);
    }

    /// Check whether a ray from the reference point in direction ``d`` crosses the rectangle
    Mask contains(const Vector3 &d) const {
        Vector3 l = frame.to_local(d);
        Float t = z0 / l.z();
        return l.z() < 0.f && rect.contains(Point2(t * l.x(), t * l.y()));
    }

    /// Sample a direction towards the rectangle uniformly with respect to solid angle
    Vector3 sample(const Point2 &sample) const {
        // Angle along the first edge, then solve for the x coordinate
        Float au = dr::fmadd(sample.x(), solid_angle, k);
        auto [sin_au, cos_au] = dr::sincos(au);
        Float fu = dr::select(sin_au != 0.f,
                              dr::fmadd(cos_au, b0, b1) / sin_au, 0.f),
              cu = dr::clip(dr::copysign(dr::rsqrt(dr::fmadd(fu, fu, dr::square(b0))), fu),
                            -1.f, 1.f),
              xu = dr::clip(-(cu * z0) / dr::maximum(dr::safe_sqrt(1.f - dr::square(cu)), 1e-7f),
                            rect.min.x(), rect.max.x());

        // The y coordinate follows from the great circle through the x line
        Float d2  = dr::fmadd(xu, xu, dr::square(z0)),
              y0  = rect.min.y(),
              y1  = rect.max.y(),
              h0  = y0 * dr::rsqrt(d2 + dr::square(y0)),
              h1  = y1 * dr::rsqrt(d2 + dr::square(y1)),
              hv  = dr::fmadd(sample.y(), h1 - h0, h0),
              hv2 = dr::square(hv),
              yv  = dr::select(hv2 < 1.f - 1e-6f, hv * dr::sqrt(d2 / (1.f - hv2)), y1);

        return dr::normalize(frame.to_world(Vector3(xu, yv, z0)));
    }

    /// Solid angle density of ``sample()``, zero for directions that miss the rectangle
    Float pdf(const Vector3 &d) const {
        return dr::select(contains(d) && !degenerate, dr::rcp(solid_angle), 0.f);
    }
};

template <typename Stream, typename Float>
Stream &operator<<(Stream &os, const SphericalRectangle<Float> &r) {
    os << "SphericalRectangle" << type_suffix<Vector<Float, 3>>() << "[" << std::endl
       << "  frame = " << string::indent(r.frame, 10) << "," << std::endl
       << "  rect = " << string::indent(r.rect, 9) << "," << std::endl
       << "  z0 = " << r.z0 << "," << std::endl
       << "  solid_angle = " << r.solid_angle << "," << std::endl
       << "  degenerate = " << r.degenerate << std::endl
       << "]";
    return os;
}

NAMESPACE_END(mitsuba)
