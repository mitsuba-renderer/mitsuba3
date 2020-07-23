#pragma once

#include <mitsuba/core/ray.h>

NAMESPACE_BEGIN(mitsuba)

/// Generic n-dimensional bounding sphere data structure
template <typename Point_> struct BoundingSphere {
    static constexpr size_t Size = Point_::Size;
    using Point                  = Point_;
    using Float                  = ek::value_t<Point>;
    using Vector                 = mitsuba::Vector<Float, Size>;
    using Mask                   = ek::mask_t<Float>;

    Point center;
    Float radius;

    /// Construct bounding sphere(s) at the origin having radius zero
    BoundingSphere() : center(0.f), radius(0.f) { }

    /// Create bounding sphere(s) from given center point(s) with given size(s)
    BoundingSphere(const Point &center, const Float &radius)
    : center(center), radius(radius) { }

    /// Equality test against another bounding sphere
    bool operator==(const BoundingSphere &bsphere) const {
        return ek::all_nested(ek::eq(center, bsphere.center) &&
                              ek::eq(radius, bsphere.radius));
    }

    /// Inequality test against another bounding sphere
    bool operator!=(const BoundingSphere &bsphere) const {
        return ek::any_nested(ek::neq(center, bsphere.center) ||
                              ek::neq(radius, bsphere.radius));
    }

    /// Return whether this bounding sphere has a radius of zero or less.
    Mask empty() const {
        return radius <= 0.f;
    }

    /// Expand the bounding sphere radius to contain another point.
    void expand(const Point &p) {
        radius = ek::max(radius, ek::norm(p - center));
    }

    /**
     * \brief Check whether a point lies \a on or \a inside the bounding sphere
     *
     * \param p The point to be tested
     *
     * \tparam Strict Set this parameter to \c true if the bounding
     *                sphere boundary should be excluded in the test
     *
     * \remark In the Python bindings, the 'Strict' argument is a normal
     *         function parameter with default value \c False.
     */
    template <bool Strict = false>
    Mask contains(const Point &p) const {
        if constexpr (Strict)
            return ek::squared_norm(p - center) < ek::sqr(radius);
        else
            return ek::squared_norm(p - center) <= ek::sqr(radius);
    }

    /// Check if a ray intersects a bounding box
    template <typename Ray>
    MTS_INLINE auto ray_intersect(const Ray &ray) const {
        typename Ray::Vector o = ray.o - center;

        return math::solve_quadratic(
            ek::squared_norm(ray.d),
            2.f * ek::dot(o, ray.d),
            ek::squared_norm(o) - ek::sqr(radius)
        );
    }
};

/// Print a string representation of the bounding sphere
template <typename Point>
std::ostream &operator<<(std::ostream &os, const BoundingSphere<Point> &bsphere) {
    os << "BoundingSphere" << type_suffix<Point>();
    if (all(bsphere.empty()))
        os << "[empty]";
    else
        os << "[" << std::endl
           << "  center = " << bsphere.center << "," << std::endl
           << "  radius = " << bsphere.radius << std::endl
           << "]";
    return os;
}

NAMESPACE_END(mitsuba)


