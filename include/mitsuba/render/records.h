#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/common.h>

NAMESPACE_BEGIN(mitsuba)

// TODO: port these structures
struct Intersection { };
struct MediumSample { };

// -----------------------------------------------------------------------------

/**
 * \brief Generic sampling record for positions
 *
 * This sampling record is used to implement techniques that draw a position
 * from a point, line, surface, or volume domain in 3D and furthermore provide
 * auxilary information about the sample.
 *
 * Apart from returning the position and (optionally) the surface normal, the
 * responsible sampling method must annotate the record with the associated
 * probability density and measure.
 */
template <typename Point3> struct PositionSample {
    using Point2        = point2_t<Point3>;
    using Normal3       = normal3_t<Point3>;
    using Value         = type_t<Point3>;
    using Measure       = like_t<Value, EMeasure>;
    using ObjectPointer = like_t<Value, const Object *>;
    using Intersection  = mitsuba::Intersection; // TODO: support packets of Intersections

    // -------------------------------------------------------------------------

    /// Sampled position
    Point3 p;

    /// Sampled surface normal (if applicable)
    Normal3 n;

    /**
     * \brief Optional: 2D sample position associated with the record
     *
     * In some uses of this record, a sampled position may be associated
     * with an important 2D quantity, such as the texture coordinates on
     * a triangle mesh or a position on the aperture of a sensor. When
     * applicable, such positions are stored in the \c uv attribute.
     */
    Point2 uv;

    /// Associated time value
    Value time;

    /// Probability density at the sample
    Value pdf;

    /**
     * \brief Denotes the measure associated with the sample.
     *
     * This is necessary to deal with quantities that are defined on
     * unusual spaces, e.g. areas that have collapsed to a point
     * or a line.
     */
    Measure measure;

     /**
      * \brief Optional: Pointer to an associated object
      *
      * In some uses of this record, sampling a position also involves
      * choosing one of several objects (shapes, emitters, ..) on which
      * the position lies. In that case, the \c object attribute stores
      * a pointer to this object.
      */
    ObjectPointer object = nullptr;

    // -------------------------------------------------------------------------

    /**
     * \brief Create a new position sampling record that can be
     * passed e.g. to \ref Shape::sample_position
     *
     * \param time
     *    Specifies the time that should be associated with the
     *    position sample. This only matters when things are in motion
     */
    PositionSample(Value time)
        : uv(zero<Point2f>()), time(time) { }

    /**
     * \brief Create a position sampling record from a surface intersection
     *
     * This is useful to determine the hypothetical sampling density on a
     * surface after hitting it using standard ray tracing. This happens for
     * instance in path tracing with multiple importance sampling.
     */
    PositionSample(const Intersection &/*its*/,
                   Measure /*measure = Measure(EArea)*/) {
        // TODO: implement when Intersection is available
        Throw("Not implemented: PositionSample(Intersection, Measure)");
    }

    ENOKI_STRUCT(PositionSample, p, n, uv, time, pdf, measure, object)
    ENOKI_ALIGNED_OPERATOR_NEW()
};

// -----------------------------------------------------------------------------

/**
 * \brief Generic sampling record for directions
 *
 * This sampling record is used to implement techniques that randomly draw a
 * unit vector from a subset of the sphere and furthermore provide
 * auxilary information about the sample.
 *
 * Apart from returning the sampled direction, the responsible sampling method
 * must annotate the record with the associated probability density
 * and measure.
 */
template <typename Vector3>
struct DirectionSample {
    using Value         = type_t<Vector3>;
    using Measure      = like_t<Value, EMeasure>;
    using Intersection = mitsuba::Intersection; // TODO: support packets of Intersections

    // -------------------------------------------------------------------------

    /// Sampled direction
    Vector3 d;

    /// Probability density at the sample
    Value pdf;

    /// Measure associated with the density function
    Measure measure;

    // -------------------------------------------------------------------------

    /**
     * \brief Create a direction sampling record filled with a specified
     * direction.
     *
     * The resulting data structure is meant to be used to query the density of
     * a direction sampling technique.
     *
     * \sa Emitter::pdf_direction
     */
    DirectionSample(const Vector3 &d, Measure measure = ESolidAngle)
        : d(d), measure(measure) { }

    /**
     * \brief Create a direction sampling record
     * from a surface intersection
     *
     * This is useful to determine the hypothetical sampling
     * density of a direction after hitting it using standard
     * ray tracing. This happens for instance when hitting
     * the camera aperture in bidirectional rendering
     * techniques.
     */
    DirectionSample(const Intersection &/*its*/,
                    Measure /*measure = ESolidAngle*/) {
        // TODO: implement when Intersection is available
        Throw("Not implemented: DirectionSample(Intersection, Measure)");
    }

    ENOKI_STRUCT(DirectionSample, d, pdf, measure)
    ENOKI_ALIGNED_OPERATOR_NEW()
};


// -----------------------------------------------------------------------------

/**
 * \brief Record for solid-angle based area sampling techniques
 *
 * This sampling record is used to implement techniques that randomly pick
 * a position on the surface of an object with the goal of importance sampling
 * a quantity that is defined over the sphere seen from a given reference point.
 *
 * This general approach for sampling positions is named "direct" sampling
 * throughout Mitsuba, motivated by direct illumination rendering techniques,
 * which represent the most important application.
 *
 * This record inherits all fields from \ref PositionSample and
 * extends it with two useful quantities that are cached so that they don't
 * need to be recomputed many times: the unit direction and length from the
 * reference position to the sampled point.
 */
template <typename Point3>
struct DirectSample : public PositionSample<Point3> {
    using Base         = PositionSample<Point3>;
    using Vector3      = vector3_t<Point3>;
    using Ray3         = Ray<Point3>;
    using MediumSample = mitsuba::MediumSample; // TODO: support packets of MediumSamples

    using typename Base::Point2;
    using typename Base::ObjectPointer;
    using typename Base::Normal3;
    using typename Base::Value;
    using typename Base::Measure;
    using typename Base::Intersection;

    // Make parent fields visible
    using Base::p;
    using Base::time;
    using Base::n;
    using Base::uv;
    using Base::pdf;
    using Base::measure;
    using Base::object;

    // -------------------------------------------------------------------------

    /// Reference point for direct sampling
    Point3 ref_p;

    /**
     * \brief Optional: normal vector associated with the reference point
     *
     * When nonzero, the direct sampling method can use the normal vector
     * to sample according to the projected solid angle at \c ref.
     */
    Normal3 ref_n;

    /// Unit direction from the reference point to the target direction
    Vector3 d;

    /// Distance from the reference point to the target direction
    Value dist;

    // -------------------------------------------------------------------------

    /**
     * \brief Create an new direct sampling record for a reference point
     * \c ref located somewhere in space (i.e. \a not on a surface)
     *
     * \param ref_p
     *     The reference point
     * \param time
     *     An associated time value
     */
    DirectSample(const Point3 &ref_p, Value time)
        : Base(time), ref_p(ref_p), ref_n(zero<Normal3>()) { }

    /**
     * \brief Create an new direct sampling record for a reference point
     * \c ref located on a surface.
     *
     * \param its
     *     The reference point specified using an intersection record
     */
    DirectSample(const Intersection &/*ref_its*/) {
        // TODO: implement when Intersection is available
        Throw("Not implemented: DirectSample(Intersection)");
    }

    /**
     * \brief Create an new direct sampling record for a reference point
     * \c ref located in a medium
     *
     * \param ms
     *     The reference point specified using an medium sampling record
     */
    DirectSample(const MediumSample &/*ms*/) {
        // TODO: implement when MediumSample is available
        Throw("Not implemented: DirectSample(MediumSample)");
    }

    /**
     * \brief Create a direct sampling record, which can be used to \a query
     * the density of a surface position (where the reference point lies on
     * a \a surface)
     *
     * \param ray
     *     Reference to the ray that generated the intersection \c its.
     *     The ray origin must be located at \c ref_its.p
     *
     * \param its
     *     A surface intersection record (usually on an emitter)
     */
    DirectSample(const Ray3 &/*ray*/, const Intersection &/*its*/,
                 Measure /*measure = ESolidAngle*/) {
        // TODO: implement when Intersection is available
        Throw("Not implemented: DirectSample(Ray3, Intersection, Measure)");
    }

    /// Element-by-element constructor
    DirectSample(const Point3 &p, const Normal3 &n, const Point2 &uv,
                 const Value &time, const Value &pdf, const Measure &measure,
                 const ObjectPointer &object, const Point3 &ref_p,
                 const Normal3 &ref_n, const Vector3 &d, const Value &dist)
        : Base(p, n, uv, time, pdf, measure, object), ref_p(ref_p),
          ref_n(ref_n), d(d), dist(dist) { }

    ENOKI_DERIVED_STRUCT(DirectSample, Base, ref_p, ref_n, d, dist)
    ENOKI_ALIGNED_OPERATOR_NEW()
};

// -----------------------------------------------------------------------------
/// Common type aliases (non-vectorized, packet, dynamic).

using PositionSample3f   = PositionSample<Point3f>;
using PositionSample3fP  = PositionSample<Point3fP>;
using PositionSample3fX  = PositionSample<Point3fX>;

using DirectionSample3f  = DirectionSample<Vector3f>;
using DirectionSample3fP = DirectionSample<Vector3fP>;
using DirectionSample3fX = DirectionSample<Vector3fX>;

using DirectSample3f     = DirectSample<Point3f>;
using DirectSample3fP    = DirectSample<Point3fP>;
using DirectSample3fX    = DirectSample<Point3fX>;

// -----------------------------------------------------------------------------

template <typename Point3>
std::ostream &operator<<(std::ostream &os,
                         const PositionSample<Point3> & record) {
    os << "PositionSample" << type_suffix<Point3>() << "["  << std::endl
       << "  p = " << string::indent(record.p, 6) << "," << std::endl
       << "  n = " << string::indent(record.n, 6) << "," << std::endl
       << "  uv = " << string::indent(record.uv, 7) << "," << std::endl
       << "  time = " << record.time << "," << std::endl
       << "  pdf = " << record.pdf << "," << std::endl
       << "  measure = " << record.measure << "," << std::endl
       << "  object = " << record.object << std::endl
       <<  "]";
    return os;
}

template <typename Vector3>
std::ostream &operator<<(std::ostream &os,
                         const DirectionSample<Vector3> & record) {
    os << "DirectionSample" << type_suffix<Vector3>() << "["  << std::endl
       << "  d = " << string::indent(record.d, 6) << "," << std::endl
       << "  pdf = " << record.pdf << "," << std::endl
       << "  measure = " << record.measure << std::endl
       << "]";
    return os;
}

template <typename Point3>
std::ostream &operator<<(std::ostream &os,
                         const DirectSample<Point3> & record) {
    os << "DirectSample" << type_suffix<Point3>() << "[" << std::endl
       << "  p = " << string::indent(record.p, 6) << "," << std::endl
       << "  time = " << record.time << "," << std::endl
       << "  n = " << string::indent(record.n, 6) << "," << std::endl
       << "  ref_p = " << string::indent(record.ref_p, 10) << "," << std::endl
       << "  ref_n = " << string::indent(record.ref_n, 10) << "," << std::endl
       << "  d = " << string::indent(record.d, 6) << "," << std::endl
       << "  dist = " << record.dist << "," << std::endl
       << "  uv = " << string::indent(record.uv, 7) << "," << std::endl
       << "  pdf = " << record.pdf << "," << std::endl
       << "  measure = " << record.measure << "," << std::endl
       << "  object = " << record.object << std::endl
       << "]";
    return os;
}

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for static & dynamic vectorization
// -----------------------------------------------------------------------

// Support for static & dynamic vectorization
ENOKI_STRUCT_DYNAMIC(mitsuba::PositionSample, p, n, uv, time,
                     pdf, measure, object)

ENOKI_STRUCT_DYNAMIC(mitsuba::DirectionSample, d, pdf, measure)

ENOKI_STRUCT_DYNAMIC(mitsuba::DirectSample, p, n, uv, time, pdf,
                     measure, object, ref_p, ref_n, d, dist)

//! @}
// -----------------------------------------------------------------------
