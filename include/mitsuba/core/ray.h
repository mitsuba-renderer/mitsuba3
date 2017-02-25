#pragma once

#include <mitsuba/core/vector.h>
#include <mitsuba/core/math.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Simple n-dimensional ray segment data structure
 *
 * Along with the ray origin and direction, this data structure additionally
 * stores a ray segment [mint, maxt] (whose entries may include positive/negative
 * infinity), as well as the componentwise reciprocals of the ray direction.
 * That is just done for convenience, as these values are frequently required.
 *
 * \remark Important: be careful when changing the ray direction. You must call
 * \ref update() to compute the componentwise reciprocals as well, or Mitsuba's
 * ray-triangle intersection code will produce undefined results.
 */
template <typename Point_> struct Ray {
    using Point                  = Point_;
    using Vector                 = typename Point::Vector;
    using Value                  = value_t<Point>;
    static constexpr size_t Size = Point::Size;

    Point o;      ///< Ray origin
    Vector d;     ///< Ray direction
    Vector d_rcp; ///< Componentwise reciprocals of the ray direction
    Value mint;   ///< Minimum position on the ray segment
    Value maxt;   ///< Maximum position on the ray segment

    /// Construct a new ray
    Ray() : mint(math::Epsilon), maxt(math::Infinity) { }

    /// Copy constructor
    Ray(const Ray &ray) = default;

    /// Conversion from other Ray types
    template <typename Point2> Ray(const Ray<Point2> &r)
        : o(r.o), d(r.d), d_rcp(r.d_rcp), mint(r.mint), maxt(r.maxt) { }

    /// Construct a new ray
    Ray(const Point &o, const Vector &d)
        : o(o), d(d), mint(math::Epsilon), maxt(math::Infinity) {
        update();
    }

    /// Construct a new ray
    Ray(const Point &o, const Vector &d, Value mint, Value maxt)
        : o(o), d(d), mint(mint), maxt(maxt) {
        update();
    }

    /// Construct a new ray
    Ray(const Point &o, const Vector &d, const Vector &d_rcp, Value mint, Value maxt)
        : o(o), d(d), d_rcp(d_rcp), mint(mint), maxt(maxt) { }

    /// Copy a ray, but change the covered segment of the copy
    Ray(const Ray &r, Value mint, Value maxt)
        : o(r.o), d(r.d), d_rcp(r.d_rcp), mint(mint), maxt(maxt) { }

    /// Update the reciprocal ray directions after changing 'd'
    void update() { d_rcp = rcp(d); }

    /// Return the position of a point along the ray
    Point operator() (Value t) const { return o + t * d; }

    /// Return a ray that points into the opposite direction
    Ray reverse() const {
        Ray result;
        result.o = o; result.d = -d; result.d_rcp = -d_rcp;
        result.mint = mint; result.maxt = maxt;
        return result;
    }
};

/// Return a string representation of the bounding box
template <typename Point>
std::ostream &operator<<(std::ostream &os,
                         const Ray<Point> &r) {
    os << "Ray" << type_suffix<typename Point::Value>() << "[\n"
       << "  o = " << r.o << ","
       << "  d = " << r.d << ","
       << "  mint = " << r.mint << ","
       << "  maxt = " << r.maxt
       << "]";
    return os;
}

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for static & dynamic vectorization
// -----------------------------------------------------------------------

NAMESPACE_BEGIN(enoki)
/* Is this type dynamic? */
template <typename Point> struct is_dynamic_impl<mitsuba::Ray<Point>> {
    static constexpr bool value = is_dynamic<Point>::value;
};

/* Create a dynamic version of this type on demand */
template <typename Point> struct dynamic_impl<mitsuba::Ray<Point>> {
    using type = mitsuba::Ray<dynamic_t<Point>>;
};

/* How many packets are stored in this instance? */
template <typename Point> size_t packets(const mitsuba::Ray<Point> &r) {
    return packets(r.o);
}

/* What is the size of the dynamic dimension of this instance? */
template <typename Point> size_t dynamic_size(const mitsuba::Ray<Point> &r) {
    return dynamic_size(r.o);
}

/* Resize the dynamic dimension of this instance */
template <typename Point>
void dynamic_resize(mitsuba::Ray<Point> &r, size_t size) {
    dynamic_resize(r.o, size);
    dynamic_resize(r.d, size);
    dynamic_resize(r.d_rcp, size);
    dynamic_resize(r.mint, size);
    dynamic_resize(r.maxt, size);
}

/* Construct a wrapper that references the data of this instance */
template <typename Point> auto ref_wrap(mitsuba::Ray<Point> &r) {
    using T2 = decltype(ref_wrap(r.o));
    return mitsuba::Ray<T2>(
        ref_wrap(r.o),
        ref_wrap(r.d),
        ref_wrap(r.d_rcp),
        ref_wrap(r.mint),
        ref_wrap(r.maxt)
    );
}

/* Construct a wrapper that references the data of this instance (const) */
template <typename Point> auto ref_wrap(const mitsuba::Ray<Point> &r) {
    using T2 = decltype(ref_wrap(r.o));
    return mitsuba::Ray<T2>(
        ref_wrap(r.o),
        ref_wrap(r.d),
        ref_wrap(r.d_rcp),
        ref_wrap(r.mint),
        ref_wrap(r.maxt)
    );
}

/* Return the i-th packet */
template <typename Point>
auto packet(mitsuba::Ray<Point> &r, size_t i) {
    using T2 = decltype(packet(r.o, 0));
    return mitsuba::Ray<T2>(
        packet(r.o, i),
        packet(r.d, i),
        packet(r.d_rcp, i),
        packet(r.mint, i),
        packet(r.maxt, i)
    );
}

/* Return the i-th packet (const) */
template <typename Point>
auto packet(const mitsuba::Ray<Point> &r, size_t i) {
    using T2 = decltype(packet(r.o, 0));
    return mitsuba::Ray<T2>(
        packet(r.o, i),
        packet(r.d, i),
        packet(r.d_rcp, i),
        packet(r.mint, i),
        packet(r.maxt, i)
    );
}

NAMESPACE_END(enoki)

//! @}
// -----------------------------------------------------------------------
