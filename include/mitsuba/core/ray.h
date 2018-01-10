#pragma once

#include <mitsuba/core/vector.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/spectrum.h>

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
 * ray-object intersection code may produce undefined results.
 */
template <typename Point_> struct Ray {
    using Point                  = Point_;
    using Vector                 = typename Point::Vector;
    using Value                  = value_t<Point>;
    using Spectrum               = mitsuba::Spectrum<Value>;
    static constexpr size_t Size = Point::Size;

    Point o;                     ///< Ray origin
    Vector d;                    ///< Ray direction
    Vector d_rcp;                ///< Componentwise reciprocals of the ray direction
    Value mint = math::Epsilon;  ///< Minimum position on the ray segment
    Value maxt = math::Infinity; ///< Maximum position on the ray segment
    Value time = 0;              ///< Time value associated with this ray
    Spectrum wavelengths;        ///< Wavelength packet associated with the ray

    /// Construct a new ray (o, d) at time 'time'
    Ray(const Point &o, const Vector &d, Value time, const Spectrum &wavelengths)
        : o(o), d(d), d_rcp(rcp(d)), time(time), wavelengths(wavelengths) { }

    /// Construct a new ray (o, d) with time
    Ray(const Point &o, const Vector &d, const Value &t) : o(o), d(d), time(t) {
        update();
    }

    /// Construct a new ray (o, d) with bounds
    Ray(const Point &o, const Vector &d, Value mint, Value maxt, Value time, const Spectrum &wavelengths)
        : o(o), d(d), d_rcp(rcp(d)), mint(mint), maxt(maxt), time(time), wavelengths(wavelengths) { }

    /// Copy a ray, but change the [mint, maxt] interval
    Ray(const Ray &r, Value mint, Value maxt)
        : o(r.o), d(r.d), d_rcp(r.d_rcp), mint(mint), maxt(maxt), time(r.time) { }

    /// Update the reciprocal ray directions after changing 'd'
    void update() { d_rcp = rcp(d); }

    /// Return the position of a point along the ray
    Point operator() (Value t) const { return fmadd(d, t, o); }

    /// Return a ray that points into the opposite direction
    Ray reverse() const {
        Ray result;
        result.o           = o;
        result.d           = -d;
        result.d_rcp       = -d_rcp;
        result.mint        = mint;
        result.maxt        = maxt;
        result.time        = time;
        result.wavelengths = wavelengths;
        return result;
    }

    ENOKI_STRUCT(Ray, o, d, d_rcp, mint, maxt, time, wavelengths)
    ENOKI_ALIGNED_OPERATOR_NEW()
};

/** \brief Ray differential -- enhances the basic ray class with
    offset rays for two adjacent pixels on the view plane */
template <typename Point_> struct RayDifferential : Ray<Point_> {
    using Base = Ray<Point_>;
    using Base::Base;

    using typename Base::Point;
    using typename Base::Vector;
    using typename Base::Value;
    using typename Base::Spectrum;
    using Base::o;
    using Base::d;
    using Base::d_rcp;
    using Base::mint;
    using Base::maxt;
    using Base::time;

    Point o_x, o_y;
    Vector d_x, d_y;
    bool has_differentials = false;

    /// Construct from a Ray instance
    RayDifferential(const Base &ray)
        : Base(ray), has_differentials(false) { }

    /// Element-by-element constructor
    RayDifferential(const Point &o, const Vector &d, const Vector &d_rcp,
                    const Value &mint, const Value &maxt, const Value &time,
                    const Spectrum &wavelengths, const Point &o_x,
                    const Point &o_y, const Vector &d_x, const Vector &d_y,
                    const bool &has_differentials)
        : Base(o, d, d_rcp, mint, maxt, time, wavelengths), o_x(o_x), o_y(o_y),
          d_x(d_x), d_y(d_y), has_differentials(has_differentials) {}

    void scale_differential(Float amount) {
        o_x = fmadd(o_x - o, amount, o);
        o_y = fmadd(o_y - o, amount, o);
        d_x = fmadd(d_x - d, amount, d);
        d_y = fmadd(d_y - d, amount, d);
    }

    ENOKI_DERIVED_STRUCT(RayDifferential, Base, o_x, o_y,
                         d_x, d_y, has_differentials)
    ENOKI_ALIGNED_OPERATOR_NEW()
};

/// Return a string representation of the ray
template <typename Point>
std::ostream &operator<<(std::ostream &os, const Ray<Point> &r) {
    os << "Ray" << type_suffix<Point>() << "[" << std::endl
       << "  o = " << string::indent(r.o, 6) << "," << std::endl
       << "  d = " << string::indent(r.d, 6) << "," << std::endl
       << "  mint = " << r.mint << "," << std::endl
       << "  maxt = " << r.maxt << "," << std::endl
       << "  time = " << r.time << "," << std::endl
       << "  wavelengths = " << r.wavelengths << std::endl
       << "]";
    return os;
}

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for static & dynamic vectorization
// -----------------------------------------------------------------------

// Support for static & dynamic vectorization
ENOKI_STRUCT_DYNAMIC(mitsuba::Ray, o, d, d_rcp, mint, maxt, time, wavelengths)

ENOKI_STRUCT_DYNAMIC(mitsuba::RayDifferential, o, d, d_rcp, mint, maxt,
                     time, wavelengths, o_x, o_y, d_x, d_y, has_differentials)

//! @}
// -----------------------------------------------------------------------
