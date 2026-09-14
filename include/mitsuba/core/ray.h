#pragma once

#include <drjit/array.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/spectrum.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Ray cone modeling the spatial and angular extent of a bundle of rays
 *
 * A ray cone :cite:`Amanatides1984Cones` bounds the rays surrounding a
 * traced ray by a circular cross section whose diameter (``width``) grows
 * linearly with distance (``spread``). Mitsuba initializes the cone of each
 * camera ray with the size of a pixel, propagates it to surface hits, and
 * projects it onto the surface to obtain UV-space footprints for filtered
 * texture lookups :cite:`AkenineMoller2019RayCones`.
 *
 * A negative spread describes a converging beam, whose width passes through
 * zero at a focal point and grows again beyond it. A default-constructed
 * `RayCone` describes an infinitely thin ray.
 */
template <typename Float_> struct RayCone {
    using Float = Float_;
    using ScalarFloat = dr::scalar_t<Float>;

    /// Diameter of the cone at the ray origin
    Float width = (ScalarFloat) 0.f;

    /// Change of the diameter per unit distance along the ray
    Float spread = (ScalarFloat) 0.f;

    /// Keeps track of extra scale factors applied to the cone
    Float scale_factor = (ScalarFloat) 1.f;

    /// Construct a cone with the given width and spread
    RayCone(const Float &width, const Float &spread,
            const Float &scale_factor = (ScalarFloat) 1.f)
        : width(width), spread(spread), scale_factor(scale_factor) { }

    /// Return the cone at distance ``t`` along the ray
    RayCone propagate(const Float &t) const {
        return RayCone(dr::fmadd(spread, t, width), spread, scale_factor);
    }

    /// Return a cone whose width and spread are scaled by ``s``
    RayCone scale(const Float &s) const {
        return RayCone(width * s, spread * s, scale_factor * s);
    }

    DRJIT_STRUCT(RayCone, width, spread, scale_factor)
};

/// Return a string representation of the ray cone
template <typename Float>
std::ostream &operator<<(std::ostream &os, const RayCone<Float> &c) {
    os << "RayCone[width=" << c.width << ", spread=" << c.spread
       << ", scale_factor=" << c.scale_factor << "]";
    return os;
}

/**
 * Simple n-dimensional ray segment data structure
 *
 * Along with the ray origin and direction, this data structure additionally
 * stores a maximum ray position ``maxt``, a time value ``time``, the
 * wavelength information associated with the ray, and the ray cone
 * ``cone`` (see `RayCone`).
 */
template <typename Point_, typename Spectrum_> struct Ray {
    static constexpr size_t Size = dr::size_v<Point_>;

    using Point       = Point_;
    // TODO: need this because dr::value_t<MaskedArray<Point3f>> isn't a masked array
    using Float =
        std::conditional_t<dr::is_masked_array_v<Point>,
                           dr::detail::MaskedArray<dr::value_t<Point>>,
                           dr::value_t<Point>>;
    using ScalarFloat = dr::scalar_t<Float>;
    using Vector      = mitsuba::Vector<Float, Size>;
    using Spectrum    = Spectrum_;
    using Wavelength  = wavelength_t<Spectrum_>;

    /// Ray origin
    Point o;
    /// Ray direction
    Vector d;
    /// Maximum position on the ray segment
    Float maxt = dr::Largest<Float>;
    /// Time value associated with this ray
    Float time = (ScalarFloat) 0.f;
    /// Wavelength associated with the ray
    Wavelength wavelengths;
    /// Ray cone bounding the neighboring rays of a pixel-sized image region
    RayCone<Float> cone;

    /// Construct a new ray (o, d) at time ``time``
    Ray(const Point &o, const Vector &d, Float time,
        const Wavelength &wavelengths)
        : o(o), d(d), time(time), wavelengths(wavelengths) { }

    /// Construct a new ray (o, d) with time
    Ray(const Point &o, const Vector &d, const Float &time = (ScalarFloat) 0.f)
        : o(o), d(d), time(time) { }

    /// Construct a new ray (o, d) with bounds
    Ray(const Point &o, const Vector &d, Float maxt, Float time,
        const Wavelength &wavelengths)
        : o(o), d(d), maxt(maxt), time(time), wavelengths(wavelengths) {}

    /// Copy a ray, but change the maxt value
    Ray(const Ray &r, Float maxt)
        : o(r.o), d(r.d), maxt(maxt),
          time(r.time), wavelengths(r.wavelengths), cone(r.cone) { }

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
        result.cone        = RayCone<Float>(cone.width, -cone.spread, cone.scale_factor);
        return result;
    }

    DRJIT_STRUCT(Ray, o, d, maxt, time, wavelengths, cone)
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
        os << "  wavelengths = " << string::indent(r.wavelengths, 16) << "," << std::endl;
    os << "  cone = " << r.cone << std::endl
       << "]";
    return os;
}

NAMESPACE_END(mitsuba)
