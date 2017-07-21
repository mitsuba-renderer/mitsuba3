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

    ENOKI_STRUCT(Ray, o, d, d_rcp, mint, maxt, time)

    /// Construct a new ray
    Ray(const Point &o, const Vector &d) : o(o), d(d) {
        update();
    }

    /// Construct a new ray
    Ray(const Point &o, const Vector &d, Value mint, Value maxt)
        : o(o), d(d), d_rcp(rcp(d)), mint(mint), maxt(maxt) {
        update();
    }

    /// Copy a ray, but change the covered segment of the copy
    Ray(const Ray &r, Value mint, Value maxt)
        : o(r.o), d(r.d), d_rcp(r.d_rcp), mint(mint), maxt(maxt) { }

    /// Update the reciprocal ray directions after changing 'd'
    void update() { d_rcp = rcp(d); }

    /// Return the position of a point along the ray
    Point operator() (Value t) const { return fmadd(d, t, o); }
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
    using Bool = bool_array_t<Value>;
    using Base::o;
    using Base::d;
    using Base::d_rcp;
    using Base::mint;
    using Base::maxt;
    using Base::time;

    Point o_x, o_y;
    Vector d_x, d_y;
    Bool has_differentials = false;

    ENOKI_DERIVED_STRUCT(RayDifferential, Base, o_x, o_y, d_x, d_y,
                         has_differentials)

    /// Element-by-element constructor
    RayDifferential(const Point &o, const Vector &d, const Vector &d_rcp,
                    const Value &mint, const Value &maxt, const Value &time,
                    const Point &o_x, const Point &o_y, const Vector &d_x,
                    const Vector &d_y, const Bool &has_differentials)
        : Base(o, d, d_rcp, mint, maxt, time), o_x(o_x), o_y(o_y),
          d_x(d_x), d_y(d_y), has_differentials(has_differentials) { }

    void scale(Float amount) {
        o_x = o + (o_x - o) * amount;
        o_y = o + (o_y - o) * amount;
        d_x = d + (d_x - d) * amount;
        d_y = d + (d_y - d) * amount;
    }
};

/// Return a string representation of the bounding box
template <typename Point>
std::ostream &operator<<(std::ostream &os, const Ray<Point> &r) {
    os << "Ray" << type_suffix<Point>() << "[" << std::endl
       << "  o = " << string::indent(r.o, 6) << "," << std::endl
       << "  d = " << string::indent(r.d, 6) << "," << std::endl
       << "  mint = " << r.mint << "," << std::endl
       << "  maxt = " << r.maxt << "," << std::endl
       << "  time = " << r.time
       << "]";
    return os;
}

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for static & dynamic vectorization
// -----------------------------------------------------------------------

// Support for static & dynamic vectorization
ENOKI_STRUCT_DYNAMIC(mitsuba::Ray, o, d, d_rcp, mint, maxt, time)

ENOKI_STRUCT_DYNAMIC(mitsuba::RayDifferential, o, d, d_rcp, mint, maxt,
                     time, o_x, o_y, d_x, d_y, has_differentials)

//! @}
// -----------------------------------------------------------------------
