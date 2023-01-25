#pragma once

#include <drjit/struct.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Simple n-dimensional ray segment data structure
 *
 * Along with the ray origin and direction, this data structure additionally
 * stores a maximum ray position \c maxt, a time value \c time as well a the
 * wavelength information associated with the ray.
 */
template <typename Point_, typename Spectrum_> struct Ray {
    static constexpr size_t Size = dr::array_size_v<Point_>;

    using Point      = Point_;
    // TODO: need this because dr::value_t<MaskedArray<Point3f>> isn't a masked array
    using Float =
        std::conditional_t<dr::is_masked_array_v<Point>,
                           dr::detail::MaskedArray<dr::value_t<Point>>,
                           dr::value_t<Point>>;
    using Vector     = mitsuba::Vector<Float, Size>;
    using Spectrum   = Spectrum_;
    using Wavelength = wavelength_t<Spectrum_>;

    /// Ray origin
    Point o;
    /// Ray direction
    Vector d;
    /// Maximum position on the ray segment
    Float maxt = dr::Largest<Float>;
    /// Time value associated with this ray
    Float time = 0.f;
    /// Wavelength associated with the ray
    Wavelength wavelengths;

    /// Construct a new ray (o, d) at time 'time'
    Ray(const Point &o, const Vector &d, Float time,
        const Wavelength &wavelengths)
        : o(o), d(d), time(time), wavelengths(wavelengths) { }

    /// Construct a new ray (o, d) with time
    Ray(const Point &o, const Vector &d, const Float &time=0.f)
        : o(o), d(d), time(time) { }

    /// Construct a new ray (o, d) with bounds
    Ray(const Point &o, const Vector &d, Float maxt, Float time,
        const Wavelength &wavelengths)
        : o(o), d(d), maxt(maxt), time(time), wavelengths(wavelengths) {}

    /// Copy a ray, but change the maxt value
    Ray(const Ray &r, Float maxt)
        : o(r.o), d(r.d), maxt(maxt),
          time(r.time), wavelengths(r.wavelengths) { }

    /// Return the position of a point along the ray
    Point operator() (Float t) const { return dr::fmadd(d, t, o); }

    /// Return a ray that points into the opposite direction
    Ray reverse() const {
        Ray result;
        result.o           = o;
        result.d           = -d;
        result.maxt        = maxt;
        result.time        = time;
        result.wavelengths = wavelengths;
        return result;
    }

    DRJIT_STRUCT(Ray, o, d, maxt, time, wavelengths)
};

/**
 * \brief Ray differential -- enhances the basic ray class with
 * offset rays for two adjacent pixels on the view plane
 */
template <typename Point_, typename Spectrum_>
struct RayDifferential : Ray<Point_, Spectrum_> {
    using Base = Ray<Point_, Spectrum_>;

    MI_USING_TYPES(Float, Point, Vector, Wavelength)
    MI_USING_MEMBERS(o, d, maxt, time, wavelengths)

    Point o_x, o_y;
    Vector d_x, d_y;
    bool has_differentials = false;

    /// Construct from a Ray instance
    RayDifferential(const Base &ray)
        : Base(ray), o_x(0), o_y(0), d_x(0), d_y(0), has_differentials(false) {}

    /// Construct a new ray (o, d) at time 'time'
    RayDifferential(const Point &o_, const Vector &d_, Float time_ = 0.f,
                    const Wavelength &wavelengths_ = Wavelength())
        : o_x(0), o_y(0), d_x(0), d_y(0), has_differentials(false) {
        o           = o_;
        d           = d_;
        time        = time_;
        wavelengths = wavelengths_;
    }

    void scale_differential(Float amount) {
        o_x = dr::fmadd(o_x - o, amount, o);
        o_y = dr::fmadd(o_y - o, amount, o);
        d_x = dr::fmadd(d_x - d, amount, d);
        d_y = dr::fmadd(d_y - d, amount, d);
    }

    DRJIT_STRUCT(RayDifferential, o, d, maxt, time,
                 wavelengths, o_x, o_y, d_x, d_y)
};

/// Return a string representation of the ray
template <typename Point, typename Spectrum>
std::ostream &operator<<(std::ostream &os, const Ray<Point, Spectrum> &r) {
    os << "Ray" << type_suffix<Point>() << "[" << std::endl
       << "  o = " << string::indent(r.o, 6) << "," << std::endl
       << "  d = " << string::indent(r.d, 6) << "," << std::endl
       << "  maxt = " << r.maxt << "," << std::endl
       << "  time = " << r.time << "," << std::endl;
    if (r.wavelengths.size() > 0)
        os << "  wavelengths = " << string::indent(r.wavelengths, 16) << std::endl;
    os << "]";
    return os;
}

NAMESPACE_END(mitsuba)
