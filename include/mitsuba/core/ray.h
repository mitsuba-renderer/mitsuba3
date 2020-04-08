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
    using Float                  = value_t<Point>;
    using Vector                 = mitsuba::Vector<Float, Size>;
    using Spectrum               = Spectrum_;
    using Wavelength             = wavelength_t<Spectrum_>;

    Point o;                              ///< Ray origin
    Vector d;                             ///< Ray direction
    Vector d_rcp;                         ///< Componentwise reciprocals of the ray direction
    Float mint = math::RayEpsilon<Float>; ///< Minimum position on the ray segment
    Float maxt = math::Infinity<Float>;   ///< Maximum position on the ray segment
    Float time = 0.f;                     ///< Time value associated with this ray
    Wavelength wavelengths;               ///< Wavelength packet associated with the ray

    /// Construct a new ray (o, d) at time 'time'
    Ray(const Point &o, const Vector &d, Float time,
        const Wavelength &wavelengths)
        : o(o), d(d), d_rcp(rcp(d)), time(time),
          wavelengths(wavelengths) { }

    /// Construct a new ray (o, d) with time
    Ray(const Point &o, const Vector &d, const Float &t)
        : o(o), d(d), time(t) {
        update();
    }

    /// Construct a new ray (o, d) with bounds
    Ray(const Point &o, const Vector &d, Float mint, Float maxt,
        Float time, const Wavelength &wavelengths)
        : o(o), d(d), d_rcp(rcp(d)), mint(mint), maxt(maxt),
          time(time), wavelengths(wavelengths) { }

    /// Copy a ray, but change the [mint, maxt] interval
    Ray(const Ray &r, Float mint, Float maxt)
        : o(r.o), d(r.d), d_rcp(r.d_rcp), mint(mint), maxt(maxt),
          time(r.time), wavelengths(r.wavelengths) { }

    /// Update the reciprocal ray directions after changing 'd'
    void update() { d_rcp = rcp(d); }

    /// Return the position of a point along the ray
    Point operator() (Float t) const { return fmadd(d, t, o); }

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
};

/**
 * \brief Ray differential -- enhances the basic ray class with
 * offset rays for two adjacent pixels on the view plane
 */
template <typename Point_, typename Spectrum_>
struct RayDifferential : Ray<Point_, Spectrum_> {
    using Base = Ray<Point_, Spectrum_>;
    using Base::Base;

    using typename Base::Float;
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

    void scale_differential(Float amount) {
        o_x = fmadd(o_x - o, amount, o);
        o_y = fmadd(o_y - o, amount, o);
        d_x = fmadd(d_x - d, amount, d);
        d_y = fmadd(d_y - d, amount, d);
    }

    ENOKI_DERIVED_STRUCT(RayDifferential, Base,
        ENOKI_BASE_FIELDS(o, d, d_rcp, mint, maxt, time, wavelengths),
        ENOKI_DERIVED_FIELDS(o_x, o_y, d_x, d_y)
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
    if (r.wavelengths.size() > 0)
        os << "  wavelengths = " << string::indent(r.wavelengths, 16) << std::endl;
    os << "]";
    return os;
}

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for static & dynamic vectorization
// -----------------------------------------------------------------------

// Support for static & dynamic vectorization
ENOKI_STRUCT_SUPPORT(mitsuba::Ray, o, d, d_rcp, mint, maxt, time, wavelengths)

ENOKI_STRUCT_SUPPORT(mitsuba::RayDifferential, o, d, d_rcp, mint, maxt,
                     time, wavelengths, o_x, o_y, d_x, d_y)

//! @}
// -----------------------------------------------------------------------
