#pragma once

#include <mitsuba/render/records.h>
#include <mitsuba/render/autodiff.h>
#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Base class of all geometric shapes in Mitsuba
 *
 * This class provides core functionality for sampling positions on surfaces,
 * computing ray intersections, and bounding shapes within ray intersection
 * acceleration data structures.
 */
class MTS_EXPORT_RENDER Shape : public DifferentiableObject {
public:
    // Use 32 bit indices to keep track of indices to conserve memory
    using Index  = uint32_t;
    using Size   = uint32_t;

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    /**
     * \brief Sample a point on the surface of this shape
     *
     * The sampling strategy is ideally uniform over the surface, though
     * implementations are allowed to deviate from a perfectly uniform
     * distribution as long as this is reflected in the returned probability
     * density.
     *
     * \param time
     *     The scene time associated with the position sample
     *
     * \param sample
     *     A uniformly distributed 2D point on the domain <tt>[0,1]^2</tt>
     *
     * \return
     *     A \ref PositionSample instance describing the generated sample
     */
    virtual PositionSample3f sample_position(Float time,
                                             const Point2f &sample) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref sample_position()
    PositionSample3f sample_position(Float time,
                                     const Point2f &sample,
                                     bool /* unused */) const {
        return sample_position(time, sample);
    }

    /// Vectorized version of \ref sample_position.
    virtual PositionSample3fP sample_position(FloatP time,
                                              const Point2fP &sample,
                                              MaskP active = true) const;
#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref sample_position.
    virtual PositionSample3fD sample_position(FloatD time,
                                              const Point2fD &sample,
                                              MaskD active = true) const;
#endif

    /**
     * \brief Query the probability density of \ref sample_position() for
     * a particular point on the surface.
     *
     * \param ps
     *     A position record describing the sample in question
     *
     * \return
     *     The probability density per unit area
     */
    virtual Float pdf_position(const PositionSample3f &ps) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref pdf_position()
    Float pdf_position(const PositionSample3f &ps,
                       bool /* unused */) const {
        return pdf_position(ps);
    }

    /// Vectorized version of \ref pdf_position.
    virtual FloatP pdf_position(const PositionSample3fP &ps,
                                MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref pdf_position.
    virtual FloatD pdf_position(const PositionSample3fD &ps,
                                MaskD active = true) const;
#endif

    /**
     * \brief Sample a direction towards this shape with respect to solid
     * angles measured at a reference position within the scene
     *
     * An ideal implementation of this interface would achieve a uniform solid
     * angle density within the surface region that is visible from the
     * reference position <tt>it.p</tt> (though such an ideal implementation
     * is usually neither feasible nor advisable due to poor efficiency).
     *
     * The function returns the sampled position and the inverse probability
     * per unit solid angle associated with the sample.
     *
     * When the Shape subclass does not supply a custom implementation of this
     * function, the \ref Shape class reverts to a fallback approach that
     * piggybacks on \ref sample_position(). This will generally lead to a
     * suboptimal sample placement and higher variance in Monte Carlo
     * estimators using the samples.
     *
     * \param it
     *    A reference position somewhere within the scene.
     *
     * \param sample
     *     A uniformly distributed 2D point on the domain <tt>[0,1]^2</tt>
     *
     * \return
     *     A \ref DirectionSample instance describing the generated sample
     */
    virtual DirectionSample3f sample_direction(const Interaction3f &it,
                                               const Point2f &sample) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref sample_direction()
    DirectionSample3f sample_direction(const Interaction3f &it,
                                       const Point2f &sample,
                                       bool /* unused */) const {
        return sample_direction(it, sample);
    }

    /// Vectorized version of \ref sample_direction.
    virtual DirectionSample3fP sample_direction(const Interaction3fP &it,
                                                const Point2fP &sample,
                                                MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref sample_direction.
    virtual DirectionSample3fD sample_direction(const Interaction3fD &it,
                                                const Point2fD &sample,
                                                MaskD active = true) const;
#endif

    /**
     * \brief Query the probability density of \ref sample_direction()
     *
     * \param it
     *    A reference position somewhere within the scene.
     *
     * \param ps
     *     A position record describing the sample in question
     *
     * \return
     *     The probability density per unit solid angle
     */
    virtual Float pdf_direction(const Interaction3f &it,
                                const DirectionSample3f &ds) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref pdf_direction()
    Float pdf_direction(const Interaction3f &it,
                        const DirectionSample3f &ds,
                        bool /* unused */) const {
        return pdf_direction(it, ds);
    }

    /// Vectorized version of \ref pdf_direction.
    virtual FloatP pdf_direction(const Interaction3fP &it,
                                 const DirectionSample3fP &ds,
                                 MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref pdf_direction.
    virtual FloatD pdf_direction(const Interaction3fD &it,
                                 const DirectionSample3fD &ds,
                                 MaskD active = true) const;
#endif

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    /**
     * \brief Fast ray intersection test
     *
     * Efficiently test whether the shape is intersected by the given ray, and
     * cache preliminary information about the intersection if that is the
     * case.
     *
     * If the intersection is deemed relevant (e.g. the closest to the ray
     * origin), detailed intersection information can later be obtained via the
     * \ref create_surface_interaction() method.
     *
     * \param ray
     *     The ray to be tested for an intersection
     *
     * \param cache
     *    Temporary space (<tt>(MTS_KD_INTERSECTION_CACHE_SIZE-2) *
     *    sizeof(Float[P])</tt> bytes) that must be supplied to cache
     *    information about the intersection.
     */
    virtual std::pair<bool, Float> ray_intersect(const Ray3f &ray, Float *cache) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref ray_intersect()
    std::pair<bool, Float> ray_intersect(const Ray3f &ray, Float *cache,
                                         bool /* unused */) const {
        return ray_intersect(ray, cache);
    }

    /// Vectorized version of \ref ray_intersect.
    virtual std::pair<MaskP, FloatP> ray_intersect(const Ray3fP &ray, FloatP *cache,
                                                   MaskP active) const;
#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref ray_intersect.
    virtual std::pair<MaskD, FloatD> ray_intersect(const Ray3fD &ray, FloatD *cache,
                                                   MaskD active) const;
#endif

    /**
     * \brief Fast ray shadow test
     *
     * Efficiently test whether the shape is intersected by the given ray, and
     * cache preliminary information about the intersection if that is the
     * case.
     *
     * No details about the intersection are returned, hence the function is
     * only useful for visibility queries. For most shapes, the implementation
     * will simply forward the call to \ref ray_intersect(). When the shape
     * actually contains a nested kd-tree, some optimizations are possible.
     *
     * \param ray
     *     The ray to be tested for an intersection
     */
    virtual bool ray_test(const Ray3f &ray) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref ray_test()
    bool ray_test(const Ray3f &ray, bool /* unused */) const {
        return ray_test(ray);
    }

    /// Vectorized version of \ref ray_test.
    virtual MaskP ray_test(const Ray3fP &ray, MaskP active) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref ray_test.
    virtual MaskD ray_test(const Ray3fD &ray, MaskD active) const;
#endif

    /**
     * \brief Given a surface intersection found by \ref ray_intersect(), fill
     * a \ref SurfaceInteraction data structure with detailed information
     * describing the intersection.
     *
     * The implementation should fill in the fields \c p, \c uv, \c n, \c
     * sh_frame.n, \c dp_du, and \c dp_dv. The fields \c t, \c time, \c
     * wavelengths, \c shape, \c prim_index, \c instance, and \c
     * has_uv_partials will already have been initialized by the caller. The
     * field \c wi is initialized by the caller following the call to \ref
     * fill_surface_interaction(), and \c duv_dx, and \c duv_dy are left
     * uninitialized.
     */
    virtual void fill_surface_interaction(const Ray3f &ray,
                                          const Float *cache,
                                          SurfaceInteraction3f &si) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref fill_surface_interaction()
    void fill_surface_interaction(const Ray3f &ray,
                                  const Float *cache,
                                  SurfaceInteraction3f &si,
                                  bool /* unused */) const {
        fill_surface_interaction(ray, cache, si);
    }

    /// Vectorized version of \ref fill_surface_interaction()
    virtual void fill_surface_interaction(const Ray3fP &ray,
                                          const FloatP *cache,
                                          SurfaceInteraction3fP &si,
                                          MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref fill_surface_interaction()
    virtual void fill_surface_interaction(const Ray3fD &ray,
                                          const FloatD *cache,
                                          SurfaceInteraction3fD &si,
                                          MaskD active = true) const;
#endif

    /**
     * \brief Test for an intersection and return detailed information
     *
     * This operation combines the prior \ref ray_intersect() and \ref
     * fill_surface_interaction() operations in case intersection with a single
     * shape is desired.
     *
     * \param ray
     *     The ray to be tested for an intersection
     */
    SurfaceInteraction3f ray_intersect(const Ray3f &ray) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref ray_intersect
    SurfaceInteraction3f ray_intersect(const Ray3f &ray, bool /* unused */) const {
        return ray_intersect(ray);
    }

    /// Vectorized version of \ref ray_intersect()
    SurfaceInteraction3fP ray_intersect(const Ray3fP &ray, MaskP active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Miscellaneous query routines
    // =============================================================

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
    virtual BoundingBox3f bbox(Index index) const;

    /**
     * \brief Return an axis aligned box that bounds a single shape primitive
     * after it has been clipped to another bounding box.
     *
     * This is extremely important to construct high-quality kd-trees. The
     * default implementation just takes the bounding box returned by
     * \ref bbox(Index index) and clips it to \a clip.
     */
    virtual BoundingBox3f bbox(Index index, const BoundingBox3f &clip) const;

    /**
     * \brief Return the shape's surface area.
     *
     * The function assumes that the object is not undergoing
     * some kind of time-dependent scaling.
     *
     * The default implementation throws an exception.
     */
    virtual Float surface_area() const;

    /**
     * \brief Return the derivative of the normal vector with respect to the UV
     * parameterization
     *
     * This can be used to compute Gaussian and principal curvatures, amongst
     * other things.
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
                      bool shading_frame = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref normal_derivative()
    std::pair<Vector3f, Vector3f>
    normal_derivative(const SurfaceInteraction3f &si,
                      bool shading_frame, bool /* unused */) const {
        return normal_derivative(si, shading_frame);
    }

    /// Vectorized version of \ref normal_derivative()
    virtual std::pair<Vector3fP, Vector3fP>
    normal_derivative(const SurfaceInteraction3fP &si,
                      bool shading_frame = true,
                      MaskP active = true) const;

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref normal_derivative()
    virtual std::pair<Vector3fD, Vector3fD>
    normal_derivative(const SurfaceInteraction3fD &si,
                      bool shading_frame = true,
                      MaskD active = true) const;
#endif

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Miscellaneous
    // =============================================================

    /// Return a string identifier
    std::string id() const override;

    /// Is this shape a triangle mesh?
    bool is_mesh() const { return m_mesh; }

    /// Does the surface of this shape mark a medium transition?
    bool is_medium_transition() const { return m_interior_medium.get() != nullptr ||
                                               m_exterior_medium.get() != nullptr; }

    /// Return the medium that lies on the interior of this shape
    const Medium *interior_medium() const { return m_interior_medium.get(); }

    /// Return the medium that lies on the exterior of this shape
    const Medium *exterior_medium() const { return m_exterior_medium.get(); }

    /// Return the shape's BSDF
    const BSDF *bsdf() const { return m_bsdf.get(); }

    /// Is this shape also an area emitter?
    bool is_emitter() const { return (bool) m_emitter; }

    /// Return the area emitter associated with this shape (if any)
    const Emitter *emitter(bool /* unused */ = false) const { return m_emitter.get(); }

    /// Return the area emitter associated with this shape (if any)
    Emitter *emitter(bool /* unused */ = false) { return m_emitter.get(); }

    /// Is this shape also an area sensor?
    bool is_sensor() const { return (bool) m_sensor; }

    /// Return the area sensor associated with this shape (if any)
    const Sensor *sensor() const { return m_sensor.get(); }

    /**
     * \brief Returns the number of sub-primitives that make up this shape
     * \remark The default implementation simply returns \c 1
     */
    virtual Size primitive_count() const;

    /**
     * \brief Return the number of primitives (triangles, hairs, ..)
     * contributed to the scene by this shape
     *
     * Includes instanced geometry. The default implementation simply returns
     * the same value as \ref primitive_count().
     */
    virtual Size effective_primitive_count() const;

#if defined(MTS_USE_EMBREE)
    /// Return the Embree version of this shape
    virtual RTCGeometry embree_geometry(RTCDevice device) const;
#endif

#if defined(MTS_USE_OPTIX)
    /// Return the OptiX version of this shape
    virtual RTgeometrytriangles optix_geometry(RTcontext context);
#endif

    // Return a list of objects that are referenced or owned by the shape
    std::vector<ref<Object>> children() override;

    //! @}
    // =============================================================

    MTS_DECLARE_CLASS()
    ENOKI_CALL_SUPPORT_FRIEND()

protected:
    Shape(const Properties &props);
    inline Shape() { }
    virtual ~Shape();

protected:
    bool m_mesh = false;
    ref<BSDF> m_bsdf;
    ref<Emitter> m_emitter;
    ref<Sensor> m_sensor;
    ref<Medium> m_interior_medium;
    ref<Medium> m_exterior_medium;
    std::string m_id;

private:
    // Internal

    template <typename Interaction,
              typename Value = typename Interaction::Value,
              typename Point2 = Point<Value, 2>,
              typename Point3 = Point<Value, 3>,
              typename DirectionSample = mitsuba::DirectionSample<Point3>>
    DirectionSample sample_direction_fallback(const Interaction &it,
                                              const Point2 &sample,
                                              mask_t<Value> mask) const;

    template <typename Interaction, typename DirectionSample,
              typename Value = typename Interaction::Value>
    Value pdf_direction_fallback(const Interaction &it,
                                 const DirectionSample &ds,
                                 mask_t<Value> mask) const;
};

NAMESPACE_END(mitsuba)

#include "detail/shape.inl"
