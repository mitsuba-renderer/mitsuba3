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

    Point o;                     ///< Ray origin
    Vector d;                    ///< Ray direction
    Vector d_rcp;                ///< Componentwise reciprocals of the ray direction
    Value mint = math::Epsilon;  ///< Minimum position on the ray segment
    Value maxt = math::Infinity; ///< Maximum position on the ray segment
    Value time = 0;              ///< Time value associated with this ray

    /// Construct a new ray
    Ray() { }

    /// Copy constructor
    Ray(const Ray &ray) = default;

    /// Conversion from other Ray types
    template <typename T> Ray(const Ray<T> &r)
        : o(r.o), d(r.d), d_rcp(r.d_rcp), mint(r.mint), maxt(r.maxt), time(r.time) { }

    /// Conversion from other Ray types
    template <typename T> Ray &operator=(const Ray<T> &r) {
        o = r.o;
        d = r.d;
        d_rcp = r.d_rcp;
        mint = r.mint;
        maxt = r.maxt;
        time = r.time;
        return *this;
    }

    /// Construct a new ray
    Ray(const Point &o, const Vector &d) : o(o), d(d) {
        update();
    }

    /// Construct a new ray
    Ray(const Point &o, const Vector &d, Value mint, Value maxt)
        : o(o), d(d), d_rcp(rcp(d)), mint(mint), maxt(maxt) {
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
        result.mint = mint; result.maxt = maxt; result.time = time;
        return result;
    }
};

/** \brief %Ray differential -- enhances the basic ray class with
   information about the rays of adjacent pixels on the view plane
*/
template <typename Point_> struct RayDifferential : Ray<Point_> {
    using Base = Ray<Point_>;
    using Base::Base;

    using typename Base::Point;
    using typename Base::Vector;
    using typename Base::Value;
    using Base::o;
    using Base::d;

    Point o_x, o_y;
    Vector d_x, d_y;
    bool has_differentials = false;

    /// Copy constructor
    RayDifferential(const RayDifferential &ray) = default;

    /// Conversion from other Ray types
    template <typename T> RayDifferential(const RayDifferential<T> &r)
        : Base(r), o_x(r.o_x), o_y(r.o_y), d_x(r.d_x), d_y(r.d_y),
          has_differentials(r.has_differentials) { }

    RayDifferential(const Point &o, const Vector &d, const Vector &d_rcp,
                    Value mint, Value maxt, const Point &o_x, const Point &o_y,
                    const Vector &d_x, const Vector &d_y,
                    bool has_differentials)
        : Base(o, d, d_rcp, mint, maxt), o_x(o_x), o_y(o_y), d_x(d_x),
          d_y(d_y), has_differentials(has_differentials) { }

    void scale(Float amount) {
        o_x = o + (o_x - o) * amount;
        o_y = o + (o_y - o) * amount;
        d_x = d + (d_x - d) * amount;
        d_y = d + (d_y - d) * amount;
    }
};

/// Return a string representation of the bounding box
template <typename Point>
std::ostream &operator<<(std::ostream &os,
                         const Ray<Point> &r) {
    os << "Ray" << type_suffix<Point>() << "[\n"
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

template <typename T> struct dynamic_support<mitsuba::Ray<T>> {
    static constexpr bool is_dynamic_nested = enoki::is_dynamic_nested<T>::value;
    using dynamic_t = mitsuba::Ray<enoki::make_dynamic_t<T>>;
    using Value = mitsuba::Ray<T>;

    static ENOKI_INLINE size_t dynamic_size(const Value &value) {
        return enoki::dynamic_size(value.o);
    }

    static ENOKI_INLINE size_t packets(const Value &value) {
        return enoki::packets(value.o);
    }

    static ENOKI_INLINE void dynamic_resize(Value &value, size_t size) {
        enoki::dynamic_resize(value.o, size);
        enoki::dynamic_resize(value.d, size);
        enoki::dynamic_resize(value.d_rcp, size);
        enoki::dynamic_resize(value.mint, size);
        enoki::dynamic_resize(value.maxt, size);
    }

    template <typename T2>
    static ENOKI_INLINE auto packet(T2 &&value, size_t i) {
        return mitsuba::Ray<decltype(enoki::packet(value.o, i))>(
            enoki::packet(value.o, i), enoki::packet(value.d, i),
            enoki::packet(value.d_rcp, i), enoki::packet(value.mint, i),
            enoki::packet(value.maxt, i)
        );
    }

    template <typename T2>
    static ENOKI_INLINE auto slice(T2 &&value, size_t i) {
        return mitsuba::Ray<decltype(enoki::slice(value.o, i))>(
            enoki::slice(value.o, i), enoki::slice(value.d, i),
            enoki::slice(value.d_rcp, i), enoki::slice(value.mint, i),
            enoki::slice(value.maxt, i)
        );
    }

    template <typename T2> static ENOKI_INLINE auto ref_wrap(T2 &&value) {
        return mitsuba::Ray<decltype(enoki::ref_wrap(value.o))>(
            enoki::ref_wrap(value.o), enoki::ref_wrap(value.d),
            enoki::ref_wrap(value.d_rcp), enoki::ref_wrap(value.mint),
            enoki::ref_wrap(value.maxt)
        );
    }
};

template <typename T> struct dynamic_support<mitsuba::RayDifferential<T>> {
    static constexpr bool is_dynamic_nested = enoki::is_dynamic_nested<T>::value;
    using dynamic_t = mitsuba::RayDifferential<enoki::make_dynamic_t<T>>;
    using Value = mitsuba::RayDifferential<T>;

    static ENOKI_INLINE size_t dynamic_size(const Value &value) {
        return enoki::dynamic_size(value.o);
    }

    static ENOKI_INLINE size_t packets(const Value &value) {
        return enoki::packets(value.o);
    }

    static ENOKI_INLINE void dynamic_resize(Value &value, size_t size) {
        enoki::dynamic_resize(value.o, size);
        enoki::dynamic_resize(value.d, size);
        enoki::dynamic_resize(value.d_rcp, size);
        enoki::dynamic_resize(value.mint, size);
        enoki::dynamic_resize(value.maxt, size);
        enoki::dynamic_resize(value.o_x, size);
        enoki::dynamic_resize(value.o_y, size);
        enoki::dynamic_resize(value.d_x, size);
        enoki::dynamic_resize(value.d_y, size);
    }

    template <typename T2>
    static ENOKI_INLINE auto packet(T2 &&value, size_t i) {
        return mitsuba::RayDifferential<decltype(enoki::packet(value.o, i))>(
            enoki::packet(value.o, i), enoki::packet(value.d, i),
            enoki::packet(value.d_rcp, i), enoki::packet(value.mint, i),
            enoki::packet(value.maxt, i), enoki::packet(value.o_x, i),
            enoki::packet(value.o_y, i), enoki::packet(value.d_x, i),
            enoki::packet(value.d_y, i), value.has_differentials
        );
    }

    template <typename T2>
    static ENOKI_INLINE auto slice(T2 &&value, size_t i) {
        return mitsuba::RayDifferential<decltype(enoki::slice(value.o, i))>(
            enoki::slice(value.o, i), enoki::slice(value.d, i),
            enoki::slice(value.d_rcp, i), enoki::slice(value.mint, i),
            enoki::slice(value.maxt, i), enoki::slice(value.o_x, i),
            enoki::slice(value.o_y, i), enoki::slice(value.d_x, i),
            enoki::slice(value.d_y, i), value.has_differentials
        );
    }

    template <typename T2>
    static ENOKI_INLINE auto ref_wrap(T2 &&value) {
        return mitsuba::RayDifferential<decltype(enoki::ref_wrap(value.o))>(
            enoki::ref_wrap(value.o), enoki::ref_wrap(value.d),
            enoki::ref_wrap(value.d_rcp), enoki::ref_wrap(value.mint),
            enoki::ref_wrap(value.maxt), enoki::ref_wrap(value.o_x),
            enoki::ref_wrap(value.o_y), enoki::ref_wrap(value.d_x),
            enoki::ref_wrap(value.d_y), value.has_differentials
        );
    }
};

NAMESPACE_END(enoki)

//! @}
// -----------------------------------------------------------------------
