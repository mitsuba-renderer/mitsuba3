#pragma once

#include <mitsuba/render/fwd.h>
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
 * probability density and delta.
 */
template <typename Point3_> struct PositionSample {
    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    using Point3               = Point3_;
    using Point2               = point2_t<Point3>;
    using Normal3              = normal3_t<Point3>;
    using Value                = value_t<Point3>;
    using ObjectPtr            = like_t<Value, const Object *>;
    using SurfaceInteraction   = mitsuba::SurfaceInteraction<Point3>;
    using Mask                 = mask_t<Value>;

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

    /// Set if the sample was drawn from a degenerate (Dirac delta) distribution
    Mask delta;

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
     * \brief Create a position sampling record from a surface intersection
     *
     * This is useful to determine the hypothetical sampling density on a
     * surface after hitting it using standard ray tracing. This happens for
     * instance in path tracing with multiple importance sampling.
     */
    PositionSample(const SurfaceInteraction &si)
        : p(si.p), n(si.sh_frame.n), uv(si.uv), time(si.time), pdf(0.f),
          delta(false), object(reinterpret_array<ObjectPtr>(si.shape)) { }

    //! @}
    // =============================================================

    ENOKI_STRUCT(PositionSample, p, n, uv, time, pdf, delta, object)
    ENOKI_ALIGNED_OPERATOR_NEW()
};

// -----------------------------------------------------------------------------

/**
 * \brief Record for solid-angle based area sampling techniques
 *
 * This data structure is used in techniques that sample positions relative to
 * a fixed reference position in the scene. For instance, <em>direct
 * illumination strategies</em> importance sample the incident radiance
 * received by a given surface location. Mitsuba uses this approach in a wider
 * bidirectional sense: sampling the incident importance due to a sensor also
 * uses the same data structures and strategies, which are referred to as
 * <em>direct sampling</em>.
 *
 * This record inherits all fields from \ref PositionSample and extends it with
 * two useful quantities that are cached so that they don't need to be
 * recomputed: the unit direction and distance from the reference position to
 * the sampled point.
 */
template <typename Point3_> struct DirectionSample : public PositionSample<Point3_> {
    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    using Base                 = PositionSample<Point3_>;
    using Vector3              = vector3_t<Point3_>;
    using Interaction          = mitsuba::Interaction<Point3_>;

    using typename Base::Value;
    using typename Base::Mask;
    using typename Base::Point2;
    using typename Base::Point3;
    using typename Base::Normal3;
    using typename Base::ObjectPtr;
    using typename Base::SurfaceInteraction;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    // Make parent fields visible
    using Base::p;
    using Base::n;
    using Base::uv;
    using Base::time;
    using Base::pdf;
    using Base::delta;
    using Base::object;

    /// Unit direction from the reference point to the target shape
    Vector3 d;

    /// Distance from the reference point to the target shape
    Value dist;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Constructors, methods, etc.
    // =============================================================

    /**
     * \brief Create a direct sampling record, which can be used to \a query
     * the density of a surface position with respect to a given reference
     * position.
     *
     * Direction `s` is set so that it points from the reference surface to
     * the intersected surface, as required when using e.g. the \ref Endpoint
     * interface to compute PDF values.
     *
     * \param it
     *     Surface interaction
     *
     * \param ref
     *     Reference position
     */
    DirectionSample(const SurfaceInteraction &it, const Interaction &ref)
        : Base(it) {
        d    = it.p - ref.p;
        dist = norm(d);
        d   /= dist;
        d[!it.is_valid()] = -it.wi; // for environment emitters
    }

    /// Element-by-element constructor
    DirectionSample(const Point3 &p, const Normal3 &n, const Point2 &uv,
                    const Value &time, const Value &pdf, const Mask &delta,
                    const ObjectPtr &object, const Vector3 &d, const Value &dist)
        : Base(p, n, uv, time, pdf, delta, object), d(d), dist(dist) { }

    /// Construct from a position sample
    DirectionSample(const Base &base) : Base(base) { }

    /**
     * \brief Setup this record so that it can be used to \a query
     * the density of a surface position (where the reference point lies on
     * a \a surface).
     *
     * \param ray
     *     Reference to the ray that generated the intersection \c si.
     *     The ray origin must be located at the reference surface and point
     *     towards \c si.p.
     *
     * \param si
     *     A surface intersection record (usually on an emitter).
     */
    void set_query(const Ray<Point3> &ray, const SurfaceInteraction &si) {
        p = si.p;
        n = si.sh_frame.n;
        uv = si.uv;
        time = si.time;
        object = si.shape->emitter();
        d = ray.d;
        dist = si.t;
    }

    //! @}
    // =============================================================

    ENOKI_DERIVED_STRUCT(DirectionSample, Base,
        ENOKI_BASE_FIELDS(p, n, uv, time, pdf, delta, object),
        ENOKI_DERIVED_FIELDS(d, dist)
    )

    ENOKI_ALIGNED_OPERATOR_NEW()
};

// -----------------------------------------------------------------------------

/**
 * \brief Radiance query record data structure used by \ref SamplingIntegrator
 */
template <typename Point3> struct RadianceSample {
    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    /**
     * \brief Make 'Scene' and 'Sampler' a dependent type so that the compiler
     * does not complain about accessing incomplete types below.
     */
    using Scene              = std::conditional_t<is_array<Point3>::value, mitsuba::Scene,   std::nullptr_t>;
    using Sampler            = std::conditional_t<is_array<Point3>::value, mitsuba::Sampler, std::nullptr_t>;
    using Value              = value_t<Point3>;
    using Mask               = mask_t<Value>;
    using Point2             = point2_t<Point3>;
    using SurfaceInteraction = mitsuba::SurfaceInteraction<Point3>;
    using RayDifferential    = mitsuba::RayDifferential<Point3>;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Pointer to the associated scene
    const Scene *scene;

    /// Sample generator
    Sampler *sampler;

    /// Surface interaction data structure
    SurfaceInteraction si;

    /// Opacity value of the associated pixel
    Value alpha;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Constructor and convenience methods.
    // =============================================================
    /// Construct a radiance query record for the given scene and sampler
    RadianceSample(const Scene *scene, Sampler *sampler)
        : scene(scene), sampler(sampler) { }

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
     *
     * \return \c true if there is a valid intersection.
     */
    SurfaceInteraction& ray_intersect(const RayDifferential &ray, const Mask &active) {
        si = scene->ray_intersect(ray, active);
        alpha = select(si.is_valid(), Value(1.f), Value(0.f));
        return si;
    }

    // Retrieve a 1D sample from the \ref Sampler
    Value next_1d(Mask active = true) { return sampler->template next<Value>(active); }

    // Retrieve a 2D sample from the \ref Sampler
    Point2 next_2d(Mask active = true) { return sampler->template next<Point2>(active); }

    //! @}
    // =============================================================

    ENOKI_STRUCT(RadianceSample, scene, sampler, si, alpha)
    ENOKI_ALIGNED_OPERATOR_NEW()
};

// -----------------------------------------------------------------------------

template <typename Point3>
std::ostream &operator<<(std::ostream &os,
                         const PositionSample<Point3> &ps) {
    os << "PositionSample" << type_suffix<Point3>() << "["  << std::endl
       << "  p = " << string::indent(ps.p, 6) << "," << std::endl
       << "  n = " << string::indent(ps.n, 6) << "," << std::endl
       << "  uv = " << string::indent(ps.uv, 7) << "," << std::endl
       << "  time = " << ps.time << "," << std::endl
       << "  pdf = " << ps.pdf << "," << std::endl
       << "  delta = " << ps.delta << "," << std::endl
       << "  object = " << string::indent(ps.object) << std::endl
       <<  "]";
    return os;
}

template <typename Point3>
std::ostream &operator<<(std::ostream &os,
                         const DirectionSample<Point3> &ds) {
    os << "DirectionSample" << type_suffix<Point3>() << "[" << std::endl
       << "  p = " << string::indent(ds.p, 6) << "," << std::endl
       << "  n = " << string::indent(ds.n, 6) << "," << std::endl
       << "  uv = " << string::indent(ds.uv, 7) << "," << std::endl
       << "  time = " << ds.time << "," << std::endl
       << "  pdf = " << ds.pdf << "," << std::endl
       << "  delta = " << ds.delta << "," << std::endl
       << "  object = " << string::indent(ds.object) << "," << std::endl
       << "  d = " << string::indent(ds.d, 6) << "," << std::endl
       << "  dist = " << ds.dist << std::endl
       << "]";
    return os;
}

template <typename Point3>
std::ostream &operator<<(std::ostream &os,
                         const RadianceSample<Point3> &record) {
    os << "RadianceSample[" << std::endl
       << "  scene = ";
    if (record.scene == nullptr) os << "[ not set ]"; else os << record.scene;
    os << "," << std::endl
       << "  sampler = ";
    if (record.sampler == nullptr) os << "[ not set ]"; else os << record.sampler;
    os << "," << std::endl
       << "  si = " << string::indent(record.si) << std::endl
       << "  alpha = " << record.alpha << std::endl
       << "]" << std::endl;
    return os;
}

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for dynamic vectorization
// -----------------------------------------------------------------------

ENOKI_STRUCT_SUPPORT(mitsuba::PositionSample, p, n, uv, time,
                     pdf, delta, object)

ENOKI_STRUCT_SUPPORT(mitsuba::DirectionSample, p, n, uv, time, pdf,
                     delta, object, d, dist)

ENOKI_STRUCT_SUPPORT(mitsuba::RadianceSample, scene, sampler, si, alpha)

//! @}
// -----------------------------------------------------------------------
