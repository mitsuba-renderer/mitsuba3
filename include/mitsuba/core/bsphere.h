#pragma once

#include <mitsuba/core/ray.h>

NAMESPACE_BEGIN(mitsuba)

/// Generic n-dimensional bounding sphere data structure
template <typename Point_> struct BoundingSphere {
    static constexpr size_t Size = Point_::Size;
    using Point                  = Point_;
    using Float                  = dr::value_t<Point>;
    using Vector                 = mitsuba::Vector<Float, Size>;
    using Mask                   = dr::mask_t<Float>;

    Point center;
    Float radius;

    /// Construct bounding sphere(s) at the origin having radius zero
    BoundingSphere() : center(0.f), radius(0.f) { }

    /// Create bounding sphere(s) from given center point(s) with given size(s)
    BoundingSphere(const Point &center, const Float &radius)
    : center(center), radius(radius) { }

    /// Equality test against another bounding sphere
    bool operator==(const BoundingSphere &bsphere) const {
        return dr::all_nested(dr::eq(center, bsphere.center) &&
                              dr::eq(radius, bsphere.radius));
    }

    /// Inequality test against another bounding sphere
    bool operator!=(const BoundingSphere &bsphere) const {
        return dr::any_nested(dr::neq(center, bsphere.center) ||
                              dr::neq(radius, bsphere.radius));
    }

    /// Return whether this bounding sphere has a radius of zero or less.
    Mask empty() const {
        return radius <= 0.f;
    }

    /// Expand the bounding sphere radius to contain another point.
    void expand(const Point &p) {
        radius = dr::maximum(radius, dr::norm(p - center));
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
            return dr::squared_norm(p - center) < dr::sqr(radius);
        else
            return dr::squared_norm(p - center) <= dr::sqr(radius);
    }

    /// Check if a ray intersects a bounding box
    template <typename Ray>
    MI_INLINE auto ray_intersect(const Ray &ray) const {
        typename Ray::Vector o = ray.o - center;

        return math::solve_quadratic(
            dr::squared_norm(ray.d),
            2.f * dr::dot(o, ray.d),
            dr::squared_norm(o) - dr::sqr(radius)
        );
    }
};

/// Print a string representation of the bounding sphere
template <typename Point>
std::ostream &operator<<(std::ostream &os, const BoundingSphere<Point> &bsphere) {
    os << "BoundingSphere" << type_suffix<Point>();
    if (dr::all(bsphere.empty()))
        os << "[empty]";
    else
        os << "[" << std::endl
           << "  center = " << bsphere.center << "," << std::endl
           << "  radius = " << bsphere.radius << std::endl
           << "]";
    return os;
}

NAMESPACE_END(mitsuba)


