#pragma once

#include <mitsuba/render/fwd.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \warning Implementing classes must call `m_emitter->set_shape(this)` once they
 * are entirely initialized. When calling it from the parent class (Shape), we
 * cannot ensure that all information required to compute `surface_area()` has
 * been initialized.
 */
class MTS_EXPORT_RENDER Shape : public Object {
public:
    /// Use 32 bit indices to keep track of indices to conserve memory
    using Index = uint32_t;

    /// Use 32 bit indices to keep track of sizes to conserve memory
    using Size  = uint32_t;

    /**
     * \brief Return an axis aligned box that bounds all shape primitives
     * (including any transformations that may have been applied to them)
     */
    virtual BoundingBox3f bbox() const = 0;

    /**
     * \brief Return an axis aligned box that bounds a single shape primitive
     * (including any transformations that may have been applied to it)
     *
     * \remark The default implementation simply calls \ref bbox()
     */
    virtual BoundingBox3f bbox(Index Size) const;

    /**
     * \brief Return an axis aligned box that bounds a single shape primitive
     * after it has been clipped to another bounding box.
     *
     * This is extremely important to construct decent kd-trees. The default
     * implementation just takes the bounding box returned by \ref bbox(Index
     * index) and clips it to \a clip.
     */
    virtual BoundingBox3f bbox(Index index,
                               const BoundingBox3f &clip) const;

    /**
     * \brief Return the shape's surface area.
     *
     * Assumes that the object is not undergoing some kind of
     * time-dependent scaling.
     *
     * The default implementation throws an exception.
     */
    virtual Float surface_area() const {
        NotImplementedError("surface_area");
    }

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    /**
     * \brief Move an intersection record to a different time value
     *
     * Conceptually, the point remains firmly attached to the underlying
     * object, which is moved forward or backward in time. The method updates
     * all relevant SurfaceInteraction fields.
     */
    virtual void adjust_time(SurfaceInteraction3f &si,
                             const Float &time) const;
    void adjust_time(SurfaceInteraction3f &si, const Float &time,
                     bool /*active*/) const {
        return adjust_time(si, time);
    }

    /// Statically vectorized version of \ref adjust_time()
    virtual void adjust_time(SurfaceInteraction3fP &si, const FloatP &time,
                             const mask_t<FloatP> &active = true) const;

    /**
     * \brief Return the derivative of the normal vector with
     * respect to the UV parameterization
     *
     * This can be used to compute Gaussian and principal curvatures,
     * amongst other things.
     *
     * \param si
     *     Surface interaction associated with the query
     *
     * \param shading_frame
     *     Specifies whether to compute the derivative of the
     *     geometric normal \a or the shading normal of the surface
     *
     * \return
     *     The partial derivatives of the normal vector with
     *     respect to \c u and \c v.
     */
    virtual std::pair<Vector3f, Vector3f>
    normal_derivative(const SurfaceInteraction3f &si,
                      bool shading_frame = true) const = 0;
    std::pair<Vector3f, Vector3f>
    normal_derivative(const SurfaceInteraction3f &si,
                      bool shading_frame, bool /*active*/) const {
        return normal_derivative(si, shading_frame);
    }

    /// Vectorized version of \ref normal_derivative()
    virtual std::pair<Vector3fP, Vector3fP>
    normal_derivative(const SurfaceInteraction3fP &si,
                      bool shading_frame           = true,
                      const mask_t<FloatP> &active = true) const = 0;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    /**
     * \brief Sample a point on the surface of this shape instance
     * (with respect to the area measure)
     *
     * The returned sample density will be uniform over the surface.
     *
     * \param p_rec
     *     A position record, which will be used to return the sampled
     *     position, as well as auxilary information about the sample.
     *
     * \param sample
     *     A uniformly distributed 2D vector
     */
    virtual void sample_position(PositionSample3f &/*p_rec*/,
                                 const Point2f &/*sample*/) const {
        NotImplementedError("sample_position");
    }
    void sample_position(PositionSample3f &p_rec, const Point2f &sample,
                         bool /*unused*/) const {
        return sample_position(p_rec, sample);
    }
    /// Vectorized version of \ref sample_position.
    virtual void sample_position(PositionSample3fP &/*p_rec*/,
                                 const Point2fP &/*sample*/,
                                 const mask_t<FloatP> &/*active*/ = true) const {
        NotImplementedError("sample_position");
    }
    /**
     * \brief Query the probability density of \ref sample_position() for
     * a particular point on the surface.
     *
     * This method will generally return the inverse of the surface area.
     *
     * \param p_rec
     *     A position record, which will be used to return the sampled
     *     position, as well as auxilary information about the sample.
     */
    virtual Float pdf_position(const PositionSample3f &/*p_rec*/) const {
        NotImplementedError("pdf_position");
        return 0.0f;
    }
    Float pdf_position(const PositionSample3f &p_rec, bool /*unused*/) const {
        return pdf_position(p_rec);
    }
    /// Vectorized version of \ref pdf_position.
    virtual FloatP pdf_position(const PositionSample3fP &/*p_rec*/,
                                const mask_t<FloatP> &/*active*/ = true) const {
        NotImplementedError("pdf_position");
        return FloatP(0.0f);
    }

    /**
     * \brief Sample a point on the surface of this shape instance
     * (with respect to the solid angle measure)
     *
     * The sample density should ideally be uniform in direction as seen from
     * the reference point \c d_rec.p.
     *
     * This general approach for sampling positions is named "direct" sampling
     * throughout Mitsuba motivated by direct illumination rendering techniques,
     * which represent the most important application.
     *
     * When no implementation of this function is supplied, the \ref Shape
     * class will revert to the default approach, which piggybacks on
     * \ref sampleArea(). This usually results in a a suboptimal sample
     * placement, which can manifest itself in the form of high variance
     *
     * \param d_rec
     *    A direct sampling record that specifies the reference point and a
     *    time value. After the function terminates, it will be populated
     *    with the position sample and related information
     *
     * \param sample
     *     A uniformly distributed 2D vector
     */
    virtual void sample_direct(DirectSample3f &d_rec,
                               const Point2f &sample) const;
    void sample_direct(DirectSample3f &d_rec, const Point2f &sample,
                       bool /*unused*/) const {
        return sample_direct(d_rec, sample);
    }
    /// Vectorized version of \ref sample_direct.
    virtual void sample_direct(DirectSample3fP &d_rec,
                               const Point2fP &sample,
                               const mask_t<FloatP> &active = true) const;

    /**
     * \brief Query the probability density of \ref sampleDirect() for
     * a particular point on the surface.
     *
     * \param d_rec
     *    A direct sampling record, which specifies the query
     *    location. Note that this record need not be completely
     *    filled out. The important fields are \c p, \c n, \c ref,
     *    \c dist, \c d, \c measure, and \c uv.
     *
     * \param p
     *     An arbitrary point used to define the solid angle measure
     */
    virtual Float pdf_direct(const DirectSample3f &d_rec) const;
    Float pdf_direct(const DirectSample3f &d_rec, bool /*unused*/) const {
        return pdf_direct(d_rec);
    }
    /// Vectorized version of \ref pdf_direct.
    virtual FloatP pdf_direct(const DirectSample3fP &d_rec,
                              const mask_t<FloatP> &active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    /**
     * \brief Fast ray intersection test (scalar)
     *
     * Check whether the shape is intersected by the given ray. Some
     * temporary space (\ref MTS_KD_INTERSECTION_CACHE_SIZE-4 bytes) is,
     * supplied which can be used to cache information about the
     * intersection. The function \ref fill_surface_interaction()
     * can later use this information to fill in a detailed
     * intersection record.
     */
    virtual std::pair<bool, Float> ray_intersect(
            const Ray3f &/*ray*/, Float /*mint*/, Float /*maxt*/,
            void * /*cache*/) const {
        NotImplementedError("ray_intersect(full, scalar)");
        return { false, 0.f };
    }
    auto ray_intersect(const Ray3f &ray, Float mint, Float maxt, void *cache,
                       bool /*unused*/) const {
        return ray_intersect(ray, mint, maxt, cache);
    }
    /// Vectorized variant of \ref ray_intersect.
    virtual std::pair<mask_t<FloatP>, FloatP> ray_intersect(
            const Ray3fP &/*ray*/, FloatP /*mint*/, FloatP /*maxt*/,
            void * /*cache*/, const mask_t<FloatP> &/*active*/) const {
        NotImplementedError("ray_intersect(full, vector)");
        return { false, 0.f };
    }

    /**
     * \brief Fast ray intersection test for visibility queries (scalar)
     *
     * Check whether the shape is intersected by the given ray.
     * No details about the intersection are returned, hence the
     * function is only useful for visibility queries. For most
     * shapes, this will simply call forward the call to \ref
     * ray_intersect. When the shape actually contains a nested
     * kd-tree, some optimizations are possible.
     */
    virtual bool ray_intersect(const Ray3f &/*ray*/, Float /*mint*/,
                               Float /*maxt*/) const {
        NotImplementedError("ray_intersect(shadow, scalar)");
        return false;
    }
    auto ray_intersect(const Ray3f &ray, Float mint, Float maxt,
                       bool /*unused*/) const {
        return ray_intersect(ray, mint, maxt);
    }
    /// Vectorized variant of \ref ray_intersect.
    virtual mask_t<FloatP> ray_intersect(
            const Ray3fP &/*ray*/, FloatP /*mint*/, FloatP /*maxt*/,
            const mask_t<FloatP> &/*active*/) const {
        NotImplementedError("ray_intersect(shadow, scalar)");
        return false;
    }

    /**
     * \brief Given that an intersection has been found, fill a
     * detailed intersection record.
     *
     * \warning In implementations of this interface, all fields of \c its may
     * be overwritten, independently of the \c active mask. The mask is only
     * used to avoid unsafe operations such as nullptr dereference.
     * This allows for efficient unmasked operations.
     */
    virtual void fill_surface_interaction(
            const Ray3f &/*ray*/, const void * /*cache*/,
            SurfaceInteraction3f &/*its*/) const {
        NotImplementedError("fill_surface_interaction(scalar)");
    }
    void fill_surface_interaction(
            const Ray3f &ray, const void *cache,
            SurfaceInteraction3f &its, bool /*unused*/) const {
        return fill_surface_interaction(ray, cache, its);
    }
    /// Vectorized variant of \ref fill_surface_interaction.
    virtual void fill_surface_interaction(
            const Ray3fP &/*ray*/, const void * /*cache*/,
            SurfaceInteraction3fP &/*its*/, const mask_t<FloatP> &/*active*/) const {
        NotImplementedError("fill_surface_interaction(vector)");
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Miscellaneous
    // =============================================================

    /// Does the surface of this shape mark a medium transition?
    bool is_medium_transition(bool /*unused*/ = true) const {
        // TODO: handle media.
        return false;
        // NotImplementedError(is_medium_transition);
        // return m_interior_medium.get() || m_exterior_medium.get();
    }
    /// Return the medium that lies on the interior of this
    /// shape (\c nullptr == vacuum)
    Medium *interior_medium(bool /*unused*/ = true) {
        // TODO: handle media.
        return nullptr;
        // NotImplementedError(interior_medium);
        // return m_interior_medium;
    }
    /// Return the medium that lies on the interior of this
    /// shape (\c nullptr == vacuum, const version)
    const Medium *interior_medium(bool /*unused*/ = true) const {
        // TODO: handle media.
        return nullptr;
        // NotImplementedError(interior_medium);
        // return m_interior_medium.get();
    }
    /// Return the medium that lies on the exterior of this
    /// shape (\c nullptr == vacuum)
    Medium *exterior_medium(bool /*unused*/ = true) {
        // TODO: handle media.
        return nullptr;
        // NotImplementedError(exterior_medium);
        // return m_exterior_medium;
    }
    /// Return the medium that lies on the exterior of this
    /// shape (\c nullptr == vacuum, const version)
    const Medium *exterior_medium(bool /*unused*/ = true) const {
        // TODO: handle media.
        return nullptr;
        // NotImplementedError(exterior_medium);
        // return m_exterior_medium.get();
    }

    /// Return the shape's BSDF (const version)
    const BSDF *bsdf(bool /*unused*/ = true) const { return m_bsdf.get(); }

    /// Return the shape's BSDF
    BSDF *bsdf(bool /*unused*/ = true) { return m_bsdf.get(); }

    /// Set the BSDF of this shape
    void set_bsdf(BSDF *bsdf) { m_bsdf = bsdf; }

    /// Is this shape also an area emitter?
    bool is_emitter(bool /*unused*/ = true) const { return (bool)m_emitter; }

    /// Return the area emitter associated with this shape (if any, const version)
    const Emitter *emitter(bool /*unused*/ = true) const { return m_emitter.get(); }

    /// Return the area emitter associated with this shape (if any)
    Emitter *emitter(bool /*unused*/ = true) { return m_emitter.get(); }

    /// Is this shape also an area sensor?
    bool is_sensor(bool /*unused*/ = true) const { return (bool)m_sensor; }

    /// Return the area sensor associated with this shape (if any, const version)
    const Sensor *sensor(bool /*unused*/ = true) const { return m_sensor.get(); }

    /// Return the area sensor associated with this shape (if any)
    Sensor *sensor(bool /*unused*/ = true) { return m_sensor.get(); }

    /**
     * \brief Returns the number of sub-primitives that make up this shape
     * \remark The default implementation simply returns \c 1
     */
    virtual Size primitive_count() const;

    /**
     * \brief Return the number of primitives (triangles, hairs, ..)
     * contributed to the scene by this shape
     *
     * Includes instanced geometry.
     * The default implementation simply returns `primitive_count()`.
     */
    virtual size_t effective_primitive_count() const {
        return primitive_count();
    };

    //! @}
    // =============================================================

    MTS_DECLARE_CLASS()

protected:
    /// Constructs a new Shape.
    Shape() { };
    /// Constructs a new Shape, potentially containing children objects such
    /// as an Emitter.
    Shape(const Properties &props);

    virtual ~Shape();

private:
    ref<BSDF> m_bsdf;
    ref<Emitter> m_emitter;
    ref<Sensor> m_sensor;


private:  // Implementation-specific
    template <typename DirectSample, typename Point2 = typename DirectSample::Point2,
              typename Mask = mask_t<typename DirectSample::Value>>
    void sample_direct_impl(DirectSample &d_rec, const Point2 &sample,
                            const Mask &active) const;
    template <typename DirectSample, typename Value = typename DirectSample::Value,
              typename Mask = mask_t<Value>>
    Value pdf_direct_impl(const DirectSample &d_rec, const Mask &active) const;
};

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for packets of Shape pointers
// -----------------------------------------------------------------------

// Enable usage of array pointers for our types
ENOKI_CALL_SUPPORT_BEGIN(mitsuba::ShapeP)
ENOKI_CALL_SUPPORT_SCALAR(is_emitter)
ENOKI_CALL_SUPPORT_SCALAR(emitter)
ENOKI_CALL_SUPPORT_SCALAR(is_sensor)
ENOKI_CALL_SUPPORT_SCALAR(sensor)
ENOKI_CALL_SUPPORT_SCALAR(bsdf)
ENOKI_CALL_SUPPORT_SCALAR(is_medium_transition)
ENOKI_CALL_SUPPORT_SCALAR(exterior_medium)
ENOKI_CALL_SUPPORT_SCALAR(interior_medium)
ENOKI_CALL_SUPPORT(normal_derivative)
ENOKI_CALL_SUPPORT(adjust_time)
ENOKI_CALL_SUPPORT_END(mitsuba::ShapeP)

//! @}
// -----------------------------------------------------------------------

