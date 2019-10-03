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
template <typename Point_, typename Spectrum_> struct Ray {
    static constexpr size_t Size = array_size_v<Point_>;

    using Point                  = Point_;
    using Value                  = value_t<Point>;
    using Vector                 = mitsuba::Vector<Value, Size>;
    using Spectrum               = Spectrum_;
    using Wavelength             = wavelength_t<Spectrum_>;

    Point o;                     ///< Ray origin
    Vector d;                    ///< Ray direction
    Vector d_rcp;                ///< Componentwise reciprocals of the ray direction
    Value mint = math::Epsilon<Value>;  ///< Minimum position on the ray segment
    Value maxt = math::Infinity<Value>; ///< Maximum position on the ray segment
    Value time = 0.f;            ///< Time value associated with this ray
    Wavelength wavelength;       ///< Wavelength packet associated with the ray

    /// Construct a new ray (o, d) at time 'time'
    Ray(const Point &o, const Vector &d, Value time,
        const Wavelength &wavelength)
        : o(o), d(d), d_rcp(rcp(d)), time(time),
          wavelength(wavelength) { }

    /// Construct a new ray (o, d) with time
    Ray(const Point &o, const Vector &d, const Value &t)
        : o(o), d(d), time(t) {
        update();
    }

    /// Construct a new ray (o, d) with bounds
    Ray(const Point &o, const Vector &d, Value mint, Value maxt,
        Value time, const Wavelength &wavelength)
        : o(o), d(d), d_rcp(rcp(d)), mint(mint), maxt(maxt),
          time(time), wavelength(wavelength) { }

    /// Copy a ray, but change the [mint, maxt] interval
    Ray(const Ray &r, Value mint, Value maxt)
        : o(r.o), d(r.d), d_rcp(r.d_rcp), mint(mint), maxt(maxt),
          time(r.time) { }

    /// Update the reciprocal ray directions after changing 'd'
    void update() { d_rcp = rcp(d); }

    /// Return the position of a point along the ray
    Point operator() (Value t) const { return fmadd(d, t, o); }

    /// Return a ray that points into the opposite direction
    Ray reverse() const {
        Ray result;
        result.o          = o;
        result.d          = -d;
        result.d_rcp      = -d_rcp;
        result.mint       = mint;
        result.maxt       = maxt;
        result.time       = time;
        result.wavelength = wavelength;
        return result;
    }

    ENOKI_STRUCT(Ray, o, d, d_rcp, mint, maxt, time, wavelength)
};

/** \brief Ray differential -- enhances the basic ray class with
    offset rays for two adjacent pixels on the view plane */
template <typename Point_, typename Spectrum_>
struct RayDifferential : Ray<Point_, Spectrum_> {
    using Base = Ray<Point_, Spectrum_>;
    using Base::Base;

    using typename Base::Value;
    using typename Base::Point;
    using typename Base::Vector;
    using Base::o;
    using Base::d;

    Point o_x, o_y;
    Vector d_x, d_y;
    bool has_differentials = false;

    /// Construct from a Ray instance
    RayDifferential(const Base &ray)
        : Base(ray), has_differentials(false) { }

    void scale_differential(Value amount) {
        o_x = fmadd(o_x - o, amount, o);
        o_y = fmadd(o_y - o, amount, o);
        d_x = fmadd(d_x - d, amount, d);
        d_y = fmadd(d_y - d, amount, d);
    }

    ENOKI_DERIVED_STRUCT(RayDifferential, Base,
        ENOKI_BASE_FIELDS(o, d, d_rcp, mint, maxt, time, wavelength),
        ENOKI_DERIVED_FIELDS(o_x, o_y, d_x, d_y, has_differentials)
    )
};

/// Return a string representation of the ray
template <typename Point, typename Spectrum>
std::ostream &operator<<(std::ostream &os, const Ray<Point, Spectrum> &r) {
    os << "Ray" << type_suffix<Point>() << "[" << std::endl
       << "  o = " << string::indent(r.o, 6) << "," << std::endl
       << "  d = " << string::indent(r.d, 6) << "," << std::endl
       << "  mint = " << r.mint << "," << std::endl
       << "  maxt = " << r.maxt << "," << std::endl
       << "  time = " << r.time << "," << std::endl;
    if constexpr (sizeof(r.wavelength) > 0)
        os << "  wavelength = " << string::indent(r.wavelength, 15) << std::endl;
    os << "]";
    return os;
}

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for static & dynamic vectorization
// -----------------------------------------------------------------------

// Support for static & dynamic vectorization
ENOKI_STRUCT_SUPPORT(mitsuba::Ray, o, d, d_rcp, mint, maxt, time, wavelength)

ENOKI_STRUCT_SUPPORT(mitsuba::RayDifferential, o, d, d_rcp, mint, maxt,
                     time, wavelength, o_x, o_y, d_x, d_y, has_differentials)

//! @}
// -----------------------------------------------------------------------
