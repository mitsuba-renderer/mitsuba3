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
template <typename Float_, typename Spectrum_>
struct PositionSample {
    // =============================================================
    //! @{ \name Type declarations
    // =============================================================

    using Float    = Float_;
    using Spectrum = Spectrum_;
    MTS_IMPORT_RENDER_BASIC_TYPES()
    using ObjectPtr            = typename RenderAliases::ObjectPtr;
    using SurfaceInteraction3f = typename RenderAliases::SurfaceInteraction3f;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Sampled position
    Point3f p;

    /// Sampled surface normal (if applicable)
    Normal3f n;

    /**
     * \brief Optional: 2D sample position associated with the record
     *
     * In some uses of this record, a sampled position may be associated with
     * an important 2D quantity, such as the texture coordinates on a triangle
     * mesh or a position on the aperture of a sensor. When applicable, such
     * positions are stored in the \c uv attribute.
     */
    Point2f uv;

    /// Associated time value
    Float time;

    /// Probability density at the sample
    Float pdf;

    /** Set if the sample was drawn from a degenerate (Dirac delta) distribution
     *
     * Note: we use an array of booleans instead of a mask, so that slicing
     * a dynamic array of PositionSample remains possible even on architectures
     * where scalar_t<Mask> != bool (e.g. Knights Landing).
     */
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
    PositionSample(const SurfaceInteraction3f &si)
        : p(si.p), n(si.sh_frame.n), uv(si.uv), time(si.time), pdf(0.f),
          delta(false), object(reinterpret_array<ObjectPtr>(si.shape)) { }

    //! @}
    // =============================================================

    ENOKI_STRUCT(PositionSample, p, n, uv, time, pdf, delta, object)
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
template <typename Float_, typename Spectrum_>
struct DirectionSample : public PositionSample<Float_, Spectrum_> {
    // =============================================================
    //! @{ \name Type declarations
    // =============================================================
    using Float    = Float_;
    using Spectrum = Spectrum_;
    MTS_IMPORT_BASE(PositionSample, p, n, uv, time, pdf, delta, object)
    MTS_IMPORT_RENDER_BASIC_TYPES()
    using Interaction3f        = typename RenderAliases::Interaction3f;
    using SurfaceInteraction3f = typename RenderAliases::SurfaceInteraction3f;
    using ObjectPtr            = typename RenderAliases::ObjectPtr;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Fields
    // =============================================================

    /// Unit direction from the reference point to the target shape
    Vector3f d;

    /// Distance from the reference point to the target shape
    Float dist;

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
    DirectionSample(const SurfaceInteraction3f &it, const Interaction3f &ref)
        : Base(it) {
        d    = it.p - ref.p;
        dist = norm(d);
        d   /= dist;
        d[!it.is_valid()] = -it.wi; // for environment emitters
    }

    /// Element-by-element constructor
    DirectionSample(const Point3f &p, const Normal3f &n, const Point2f &uv,
                    const Float &time, const Float &pdf, const Mask &delta,
                    const ObjectPtr &object, const Vector3f &d, const Float &dist)
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
     *
     * \note Defined in scene.h
     */
    void set_query(const Ray3f &ray, const SurfaceInteraction3f &si);

    //! @}
    // =============================================================

    ENOKI_DERIVED_STRUCT(DirectionSample, Base,
        ENOKI_BASE_FIELDS(p, n, uv, time, pdf, delta, object),
        ENOKI_DERIVED_FIELDS(d, dist)
    )
};

// -----------------------------------------------------------------------------

template <typename Float, typename Spectrum>
std::ostream &operator<<(std::ostream &os,
                         const PositionSample<Float, Spectrum> &ps) {
    os << "PositionSample" << type_suffix<Point<Float, 3>>() << "["  << std::endl
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

template <typename Float, typename Spectrum>
std::ostream &operator<<(std::ostream &os,
                         const DirectionSample<Float, Spectrum> &ds) {
    os << "DirectionSample" << type_suffix<Point<Float, 3>>() << "[" << std::endl
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

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for dynamic vectorization
// -----------------------------------------------------------------------

ENOKI_STRUCT_SUPPORT(mitsuba::PositionSample, p, n, uv, time,
                     pdf, delta, object)

ENOKI_STRUCT_SUPPORT(mitsuba::DirectionSample, p, n, uv, time, pdf,
                     delta, object, d, dist)

//! @}
// -----------------------------------------------------------------------
