#pragma once

// TODO: move as many includes as possible to the .cpp
#include <mitsuba/core/fwd.h>
#include <mitsuba/core/frame.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/common.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

// TODO: port the Medium struct.
// struct Medium {};
// TODO: port Sampler.
struct Sampler {};
// TODO: which Spectrum should we use?
using Spectrum = DiscreteSpectrum;


// TODO: vectorized implementations for most methods of Intersection.

/** \brief Container for all information related to a surface intersection.
 *
 * Supports Structure of Array-style vectorization.
 *
 * \ingroup librender
 * \ingroup libpython
 */
template <typename Point3>
struct MTS_EXPORT_RENDER Intersection {
    using Point2 = point2_t<Point3>;
    using Vector3 = vector3_t<Point3>;
    using Normal3 = normal3_t<Point3>;
    using Type = type_t<Point3>;
    using Mask = mask_t<Type>;
    using Index = uint32_array_t<Type>;

    using Frame3 = Frame<Vector3>;
    using RayDifferential3 = RayDifferential<Point3>;
    using Spectrum = like_t<Type, Spectrum>;
    using BSDFPointer = like_t<Type, const BSDF *>;
    using MediumPointer = like_t<Type, const Medium *>;
    using ShapePointer = like_t<Type, const Shape *>;

public:
    inline Intersection() :
        shape(nullptr), t(math::MaxFloat) { }

    /// Convert a local shading-space vector into world space
    inline Vector3 to_world(const Vector3 &v) const {
        return sh_frame.to_world(v);
    }

    /// Convert a world-space vector into local shading coordinates
    inline Vector3 to_local(const Vector3 &v) const {
        return sh_frame.to_local(v);
    }

    /// Is the current intersection valid?
    inline Mask is_valid() const {
        return t != math::MaxFloat;
    }

    /// Is the intersected shape also a emitter?
    inline Mask is_emitter() const {
        return shape->is_emitter();
    }

    /// Is the intersected shape also a sensor?
    inline Mask is_sensor() const {
        // return call_helper<Array<const Shape*, PacketSize>>(shape).is_sensor();
        // std::vector<int> i = shape.operator->();
        return shape->is_sensor();
    }

    /// Does the intersected shape have a subsurface integrator?
    inline Mask has_subsurface() const {
        return shape->has_subsurface();
    }

    /// Does the surface mark a transition between two media?
    inline Mask is_medium_transition() const {
        return shape->is_medium_transition();
    }

    /**
     * \brief Determine the target medium
     *
     * When \c is_medium_transition() = \c true, determine the medium that
     * contains the ray (\c this->p, \c d)
     */
    inline const MediumPointer target_medium(const Vector3 &d) const {
        return select(dot(d, geo_frame.n) > 0,
            shape->exterior_medium(),
            shape->interior_medium());
    }

    /**
     * \brief Determine the target medium based on the cosine
     * of the angle between the geometric normal and a direction
     *
     * Returns the exterior medium when \c cos_theta > 0 and
     * the interior medium when \c cos_theta <= 0.
     */
    inline const MediumPointer target_medium(Type cos_theta) const {
        return select(cos_theta > 0,
            shape->exterior_medium(),
            shape->interior_medium());
    }

    /**
     * \brief Returns the BSDF of the intersected shape.
     *
     * The parameter ray must match the one used to create the intersection
     * record. This function computes texture coordinate partials if this is
     * required by the BSDF (e.g. for texture filtering).
     *
     * \remark This function should only be called if there is a valid
     * intersection!
     */
    inline const BSDFPointer bsdf(const RayDifferential3 &ray) {
        const BSDFPointer bsdf = shape->bsdf();
        if (!has_uv_partials) {
            // Compute partials for the valid entries
            // const Mask valid = (bsdf != nullptr) && bsdf->uses_ray_differentials();
            const Mask valid = enoki::neq(bsdf, nullptr) && bsdf->uses_ray_differentials();
            compute_partials(ray, valid);
        }
        return bsdf;
    }

    /// Returns the BSDF of the intersected shape
    inline const BSDFPointer bsdf() const {
        return shape->bsdf();
    }

    /**
     * \brief Returns radiance emitted into direction d.
     *
     * \remark This function should only be called if the
     * intersected shape is actually an emitter.
     */
    inline Spectrum Le(const Vector3 &/*d*/) const {
        Log(EError, "Not implemented yet");
        // return shape->emitter()->eval(*this, d);
        return Spectrum();
    }

    /**
     * \brief Returns radiance from a subsurface integrator
     * emitted into direction d.
     *
     * \remark Should only be called if the intersected
     * shape is actually a subsurface integrator.
     */
    // TODO: should we take vectorized samplers, or a single one that we use
    // in a vectorized way?
    inline Spectrum Lo_sub(const ShapePointer /*scene*/, Sampler */*sampler*/,
                           const Vector3 &/*d*/, int /*depth = 0*/) const {
        Log(EError, "Not implemented yet");
        // return shape->subsurface()->Lo(scene, sampler, *this, d, depth);
        return Spectrum();
    }

    /// Computes texture coordinate partials
    void compute_partials(const RayDifferential3 &ray, const Mask& mask = Mask(true));

    /// Move the intersection forward or backward through time
    inline void adjust_time(Type /*time*/) {
        Log(EError, "Not implemented yet");
        // if (instance)
        //     instance->adjust_time(*this, time);
        // else if (shape)
        //     shape->adjust_time(*this, time);
        // else
        //     this->time = time;
    }

    /// Calls the suitable implementation of \ref Shape::normal_derivative()
    inline void normal_derivative(Vector3 &/*dndu*/, Vector3 &/*dndv*/,
                                  Mask /*shading_frame = Mask(true)*/) const {
        Log(EError, "Not implemented yet");
        // if (instance)
        //     instance->normal_derivative(*this, dndu, dndv, shading_frame);
        // else if (shape)
        //     shape->normal_derivative(*this, dndu, dndv, shading_frame);
    }


    /**
     * \brief Create a fully specified sampling record.
     *
     * Intended for slice/packet access in dynamic arrays.
     */
    Intersection(const ShapePointer &shape, const Type &t, const Point3 &p,
                 const Frame3 &geo_frame, const Frame3 &sh_frame,
                 const Point2f &uv, const Vector3 &dpdu, const Vector3 &dpdv,
                 const Type &dudx, const Type &dudy,
                 const Type &dvdx, const Type &dvdy,
                 const Type &time, const Spectrum &color,
                 const Vector3 &wi, const bool &has_uv_partials,
                 const Index &prim_index, const ShapePointer &instance)
        : shape(shape), t(t), p(p),
          geo_frame(geo_frame), sh_frame(sh_frame),
          uv(uv), dpdu(dpdu), dpdv(dpdv), dudx(dudx),
          dudy(dudy), dvdx(dvdx), dvdy(dvdy),
          time(time), color(color), wi(wi),
          has_uv_partials(has_uv_partials), prim_index(prim_index),
          instance(instance) { }

    /// Conversion from other data types
    template <typename Other>
    Intersection(const Intersection<Other> &o)
        : shape(o.shape), t(o.t), p(o.p),
          geo_frame(o.geo_frame), sh_frame(o.sh_frame),
          uv(o.uv), dpdu(o.dpdu), dpdv(o.dpdv), dudx(o.dudx),
          dudy(o.dudy), dvdx(o.dvdx), dvdy(o.dvdy),
          time(o.time), color(o.color), wi(o.wi),
          has_uv_partials(o.has_uv_partials), prim_index(o.prim_index),
          instance(o.instance) { }

    /// Conversion from other data types
    template <typename T>
    Intersection &operator=(const Intersection<T> &o) {
        shape = o.shape; t = o.t; p = o.p;
        geo_frame = o.geo_frame; sh_frame = o.sh_frame;
        uv = o.uv; dpdu = o.dpdu; dpdv = o.dpdv; dudx = o.dudx;
        dudy = o.dudy; dvdx = o.dvdx; dvdy = o.dvdy;
        time = o.time; color = o.color; wi = o.wi;
        has_uv_partials = o.has_uv_partials; prim_index = o.prim_index;
        instance = o.instance;
        return *this;
    }

public:
    /// Pointer to the associated shape
    ShapePointer shape;

    /// Distance traveled along the ray
    Type t;

    /* Intersection point in 3D coordinates */
    Point3 p;

    /// Geometry frame
    Frame3 geo_frame;

    /// Shading frame
    Frame3 sh_frame;

    /// UV surface coordinates
    Point2f uv;

    /// Position partials wrt. the UV parameterization
    Vector3 dpdu, dpdv;

    /// UV partials wrt. changes in screen-space
    Type dudx, dudy, dvdx, dvdy;

    /// Time value associated with the intersection
    Type time;

    /// Interpolated vertex color
    Spectrum color;

    /// Incident direction in the local shading frame
    Vector3 wi;

    /**
     * Have texture coordinate partials been computed?
     * They are always computed for all slots at once, so we can make this a
     * single boolean regardless of the underlying Point3 type.
     */
    // TODO: can we make an equivalent of the bit field layout that was used here?
    bool has_uv_partials;  // : 1;

    /// Primitive index, e.g. the triangle ID (if applicable)
    // TODO: can we make an equivalent of the bit field layout that was used here?
    Index prim_index;  // : 31;

    /// Stores a pointer to the parent instance, if applicable
    ShapePointer instance;
};

// -----------------------------------------------------------------------------
/// Common type aliases (non-vectorized, packet, dynamic).

using Intersection3f  = Intersection<Point3f>;
using Intersection3fP = Intersection<Point3fP>;
using Intersection3fX = Intersection<Point3fX>;

// -----------------------------------------------------------------------------

template <typename Point3>
extern MTS_EXPORT_RENDER std::ostream &operator<<(std::ostream &os,
                                                  const Intersection<Point3> &it);

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki accessors for static & dynamic vectorization
// -----------------------------------------------------------------------

// Support for static & dynamic vectorization
ENOKI_DYNAMIC_SUPPORT(mitsuba::Intersection,
                      shape, t, p, geo_frame, sh_frame, uv,
                      dpdu, dpdv, dudx, dudy, dvdx, dvdy,
                      time, color, wi, has_uv_partials,
                      prim_index, instance)

//! @}
// -----------------------------------------------------------------------
