#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/string.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/common.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/interaction.h>

NAMESPACE_BEGIN(mitsuba)

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
template <typename Point3_> struct PositionSample {

    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    using Point3               = Point3_;
    using Point2               = point2_t<Point3>;
    using Normal3              = normal3_t<Point3>;
    using Value                = value_t<Point3>;
    using Measure              = like_t<Value, EMeasure>;
    using ObjectPtr            = like_t<Value, const Object *>;
    using SurfaceInteraction   = mitsuba::SurfaceInteraction<Point3>;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Sampled position
    Point3 p;

    /// Sampled surface normal (if applicable)
    Normal3 n;

    /**
     * \brief Optional: 2D sample position associated with the record
     *
     * In some uses of this record, a sampled position may be associated with
     * an important 2D quantity, such as the texture coordinates on a triangle
     * mesh or a position on the aperture of a sensor. When applicable, such
     * positions are stored in the \c uv attribute.
     */
    Point2 uv;

    /// Associated time value
    Value time;

    /// Probability density at the sample
    Value pdf;

    /**
     * \brief Denotes the measure associated with the sample.
     *
     * This is necessary to deal with quantities that are defined on unusual
     * spaces, e.g. areas that have collapsed to a point or a line.
     */
    Measure measure;

    /**
      * \brief Optional: pointer to an associated object
      *
      * In some uses of this record, sampling a position also involves choosing
      * one of several objects (shapes, emitters, ..) on which the position
      * lies. In that case, the \c object attribute stores a pointer to this
      * object.
      */
    ObjectPtr object = nullptr;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Constructors, methods, etc.
    // =============================================================

    /**
     * \brief Create a new position sampling record that can be passed e.g. to
     * \ref Shape::sample_position
     *
     * \param time
     *    Specifies the time that should be associated with the position
     *    sample. This only matters when things are in motion
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
    PositionSample(const SurfaceInteraction &si, Measure measure = EArea)
        : p(si.p), n(si.sh_frame.n), uv(si.uv), time(si.time),
          measure(measure) { }

    //! @}
    // =============================================================

    ENOKI_STRUCT(PositionSample, p, n, uv, time, pdf, measure, object)
    ENOKI_ALIGNED_OPERATOR_NEW()
};

// -----------------------------------------------------------------------------

/**
 * \brief Generic sampling record for directions
 *
 * This sampling record is used to implement techniques that randomly draw a
 * unit vector from a subset of the sphere and furthermore provide auxilary
 * information about the sample.
 *
 * Apart from returning the sampled direction, the responsible sampling method
 * must annotate the record with the associated probability density and
 * measure.
 */
template <typename Vector3_> struct DirectionSample {

    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    using Vector3               = Vector3_;
    using Value                 = value_t<Vector3>;
    using Measure               = like_t<Value, EMeasure>;
    using Point3                = point3_t<Vector3>;
    using SurfaceInteraction    = mitsuba::SurfaceInteraction<Point3>;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Sampled direction
    Vector3 d;

    /// Probability density at the sample
    Value pdf;

    /// Measure associated with the density function
    Measure measure;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Constructors, methods, etc.
    // =============================================================

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
     * \brief Create a direction sampling record from a surface intersection
     *
     * This is useful to determine the hypothetical sampling density of a
     * direction after hitting it using standard ray tracing. This happens for
     * instance when hitting the camera aperture in bidirectional rendering
     * techniques.
     */
    DirectionSample(const SurfaceInteraction &its,
                    Measure measure = ESolidAngle)
        : d(its.to_world(its.wi)), measure(measure) { }

    //! @}
    // =============================================================

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
 * This overall approach for sampling positions is named \a direct sampling
 * throughout Mitsuba as in <em>direct illumination</em> rendering techniques,
 * which represent the most important use case. However, the concept is used in
 * a wider bidirectional sense, e.g. to sample positions on a sensor.
 *
 * This record inherits all fields from \ref PositionSample and extends it with
 * two useful quantities that are cached so that they don't need to be
 * recomputed many times: the unit direction and length from the reference
 * position to the sampled point.
 */
template <typename Point3_> struct DirectSample : public PositionSample<Point3_> {

    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    using Base                 = PositionSample<Point3_>;
    using Vector3              = vector3_t<Point3_>;
    using Ray3                 = Ray<Point3_>;
    using MediumInteraction    = mitsuba::MediumInteraction<Point3_>;

    using typename Base::Point2;
    using typename Base::Point3;
    using typename Base::ObjectPtr;
    using typename Base::Normal3;
    using typename Base::Value;
    using typename Base::Measure;
    using typename Base::SurfaceInteraction;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    // Make parent fields visible
    using Base::p;
    using Base::time;
    using Base::n;
    using Base::uv;
    using Base::pdf;
    using Base::measure;
    using Base::object;

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

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Constructors, methods, etc.
    // =============================================================

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
     * \param ref_si
     *     The reference point specified using an intersection record
     */
    DirectSample(const SurfaceInteraction &ref_si)
        : Base(ref_si), ref_p(ref_si.p) {
        auto active = neq(ref_si.shape, nullptr);
        auto bsdf = ref_si.bsdf(active);
        Assert(none(active & eq(bsdf, nullptr)));
        auto front_only_material = eq(bsdf->flags(active)
                                      & (BSDF::ETransmission | BSDF::EBackSide), 0u)
                                   & active;
        ref_n = select(front_only_material, ref_si.sh_frame.n, Normal3(0.f));
    }

    /**
     * \brief Create an new direct sampling record for a reference point
     * \c ref located in a medium
     *
     * \param ms
     *     The reference point specified using an medium sampling record
     */
    DirectSample(const MediumInteraction &/*ms*/) {
        // TODO: implement when MediumInteraction is available
        Throw("Not implemented: DirectSample(MediumInteraction)");
    }

    /**
     * \brief Create a direct sampling record, which can be used to \a query
     * the density of a surface position (where the reference point lies on a
     * \a surface)
     *
     * \param ray
     *     Reference to the ray that generated the intersection \c ref_si.
     *     The ray origin must be located at \c ref_si.p
     *
     * \param ref_si
     *     A surface intersection record (usually on an emitter)
     */
    DirectSample(const Ray3 &ray, const SurfaceInteraction &si,
                 Measure measure = ESolidAngle)
    : Base(si, measure), d(ray.d), dist(si.t) {
        object = si.shape->emitter();
    }

    /// Element-by-element constructor
    DirectSample(const Point3 &p, const Normal3 &n, const Point2 &uv,
                 const Value &time, const Value &pdf, const Measure &measure,
                 const ObjectPtr &object, const Point3 &ref_p,
                 const Normal3 &ref_n, const Vector3 &d, const Value &dist)
        : Base(p, n, uv, time, pdf, measure, object), ref_p(ref_p),
          ref_n(ref_n), d(d), dist(dist) { }

      /**
       * \brief Setup this record so that it can be used to \a query
       * the density of a surface position (where the reference point lies on
       * a \a surface).
       *
       * \param ray
       *     Reference to the ray that generated the intersection \c si
       *     The ray origin must be located at \c si.p
       *
       * \param si
       *     A surface intersection record (usually on an emitter).
       */
    void set_query(const Ray3 &ray, const SurfaceInteraction &si,
                   const Measure &_measure = ESolidAngle) {
        p = si.p;
        n = si.sh_frame.n;
        uv = si.uv;
        measure = _measure;
        object = si.shape->emitter();
        d = ray.d;
        dist = si.t;
    }

    //! @}
    // =============================================================

    ENOKI_DERIVED_STRUCT(DirectSample, Base, ref_p, ref_n, d, dist)
    ENOKI_ALIGNED_OPERATOR_NEW()
};

// -----------------------------------------------------------------------------

/**
 * \brief Radiance query record data structure used by \ref SamplingIntegrator
 * \ingroup librender
 */
template <typename Point3> struct RadianceSample {

    // =============================================================
    //! @{ \name Type declarations
    // =============================================================
    using Value              = value_t<Point3>;
    using Int                = int_array_t<Value>;
    using Mask               = mask_t<Value>;
    using Point2             = point2_t<Point3>;
    using SurfaceInteraction = SurfaceInteraction<Point3>;
    using RayDifferential    = RayDifferential<Point3>;
    using MediumPtr          = like_t<Value, const Medium *>;
    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // An asterisk (*) marks entries which may be overwritten by the callee.
    //
    // Note: the field \c dist have been moved to \ref SurfaceInteraction::t.
    // =============================================================

    /// Pointer to the associated scene
    const Scene *scene = nullptr;

    /// Sample generator
    Sampler *sampler = nullptr;

    /// Pointer to the current medium (*)
    MediumPtr medium = nullptr;

    /// Current depth value (# of light bounces) (*)
    Int depth = 0;

    /// Surface interaction data structure (*)
    SurfaceInteraction its;

    /// Opacity value of the associated pixel (*)
    Value alpha = 0.0f;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Constructor and convenience methods.
    // =============================================================
    /// Construct a radiance query record for the given scene and sampler
    RadianceSample(const Scene *scene, Sampler *sampler)
        : scene(scene), sampler(sampler), medium(nullptr),
          depth(0), its(), alpha(0) { }

    /// Begin a new query
    void new_query(const MediumPtr &_medium) {
        medium = _medium;
        depth = 1;
        alpha = 1;
    }

    /// Initialize the query record for a recursive query
    void recursive_query(const RadianceSample &parent) {
        scene = parent.scene;
        sampler = parent.sampler;
        depth = parent.depth+1;
        medium = parent.medium;
    }

    /**
     * \brief Search for a ray intersection
     *
     * This function does several things at once: if the
     * intersection has already been provided, it returns.
     *
     * Otherwise, it
     * 1. performs the ray intersection
     * 2. computes the transmittance due to participating media
     *   and stores it in \c transmittance.
     * 3. sets the alpha value
     * 4. sets the distance value
     *
     * \return \c true if there is a valid intersection.
     *
     * \note See \file records.inl for implementation.
     */
    Mask ray_intersect(const RayDifferential &ray, const Mask &active);


    /// Retrieve a 1D sample (\ref sampler must point to a valid sampler).
    Value next_sample_1d() { return sampler->next<Value, mask_t<Value>>(); }

    /// Retrieve a 2D sample (\ref sampler must point to a valid sampler).
    Point2 next_sample_2d() { return sampler->next<Point2, mask_t<Value>>(); }
    //! @}
    // =============================================================

    ENOKI_STRUCT(RadianceSample, scene, sampler, medium, depth, its, alpha)
    ENOKI_ALIGNED_OPERATOR_NEW()
};

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

template <typename Point3>
std::ostream &operator<<(std::ostream &os,
                         const RadianceSample<Point3> & record) {
    os << "RadianceSample[" << std::endl
       << "  scene = ";
    if (record.scene == nullptr) os << "[ not set ]"; else os << record.scene;
    os << "," << std::endl
       << "  sampler = ";
    if (record.sampler == nullptr) os << "[ not set ]"; else os << record.sampler;
    os << "," << std::endl
       << "  medium = ";
    if (record.medium == nullptr) os << "[ not set ]"; else os << record.medium;
    os << "," << std::endl
       << "  depth = " << record.depth << "," << std::endl
       << "  its = " << string::indent(record.its) << std::endl
       << "  alpha = " << record.alpha << "," << std::endl
       << "]" << std::endl;
    return os;
}

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for dynamic vectorization
// -----------------------------------------------------------------------

ENOKI_STRUCT_DYNAMIC(mitsuba::PositionSample, p, n, uv, time,
                     pdf, measure, object)

ENOKI_STRUCT_DYNAMIC(mitsuba::DirectionSample, d, pdf, measure)

ENOKI_STRUCT_DYNAMIC(mitsuba::DirectSample, p, n, uv, time, pdf,
                     measure, object, ref_p, ref_n, d, dist)

ENOKI_STRUCT_DYNAMIC(mitsuba::RadianceSample, scene, sampler, medium, depth,
                     its, alpha)

//! @}
// -----------------------------------------------------------------------
