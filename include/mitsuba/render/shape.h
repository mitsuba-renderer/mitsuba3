#pragma once

#include <mitsuba/render/records.h>
#include <mitsuba/core/bbox.h>

#if defined(MTS_USE_EMBREE)
    #include <embree3/rtcore.h>
#endif

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Base class of all geometric shapes in Mitsuba
 *
 * This class provides core functionality for sampling positions on surfaces,
 * computing ray intersections, and bounding shapes within ray intersection
 * acceleration data structures.
 */
class MTS_EXPORT_RENDER Shape : public Object {
public:
    // Use 32 bit indices to keep track of indices to conserve memory
    using Index  = uint32_t;
    using IndexP = UInt32P;
    using Size   = uint32_t;
    using SizeP  = UInt32P;

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

    /// Vectorized version of \ref sample_position.
    virtual PositionSample3fP sample_position(FloatP time,
                                              const Point2fP &sample,
                                              MaskP active = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref sample_position()
    PositionSample3f sample_position(Float time,
                                     const Point2f &sample,
                                     bool /* unused */) const {
        return sample_position(time, sample);
    }

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

    /// Vectorized version of \ref pdf_position.
    virtual FloatP pdf_position(const PositionSample3fP &ps,
                                MaskP active = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref pdf_position()
    Float pdf_position(const PositionSample3f &ps,
                       bool /* unused */) const {
        return pdf_position(ps);
    }

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

    /// Vectorized version of \ref sample_direction.
    virtual DirectionSample3fP sample_direction(const Interaction3fP &it,
                                                const Point2fP &sample,
                                                MaskP active = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref sample_direction()
    DirectionSample3f sample_direction(const Interaction3f &it,
                                       const Point2f &sample,
                                       bool /* unused */) const {
        return sample_direction(it, sample);
    }

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

    /// Vectorized version of \ref pdf_direction.
    virtual FloatP pdf_direction(const Interaction3fP &it,
                                 const DirectionSample3fP &ds,
                                 MaskP active = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref pdf_direction()
    Float pdf_direction(const Interaction3f &it,
                        const DirectionSample3f &ds,
                        bool /* unused */) const {
        return pdf_direction(it, ds);
    }

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

    /// Vectorized variant of \ref ray_intersect.
    virtual std::pair<MaskP, FloatP> ray_intersect(const Ray3fP &ray, FloatP *cache,
                                                   MaskP active) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref ray_intersect()
    std::pair<bool, Float> ray_intersect(const Ray3f &ray, Float *cache,
                                         bool /* unused */) const {
        return ray_intersect(ray, cache);
    }

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

    /// Vectorized variant of \ref ray_test.
    virtual MaskP ray_test(const Ray3fP &ray, MaskP active) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref ray_test()
    bool ray_test(const Ray3f &ray, bool /* unused */) const {
        return ray_test(ray);
    }

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

    /// Vectorized version of \ref fill_surface_interaction()
    virtual void fill_surface_interaction(const Ray3fP &ray,
                                          const FloatP *cache,
                                          SurfaceInteraction3fP &si,
                                          MaskP active = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref fill_surface_interaction()
    void fill_surface_interaction(const Ray3f &ray,
                                  const Float *cache,
                                  SurfaceInteraction3f &si,
                                  bool /* unused */) const {
        fill_surface_interaction(ray, cache, si);
    }

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

    /// Vectorized version of \ref ray_intersect()
    SurfaceInteraction3fP ray_intersect(const Ray3fP &ray, MaskP active = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref ray_intersect
    SurfaceInteraction3f ray_intersect(const Ray3f &ray, bool /* unused */) const {
        return ray_intersect(ray);
    }

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

    /// Vectorized version of \ref normal_derivative()
    virtual std::pair<Vector3fP, Vector3fP>
    normal_derivative(const SurfaceInteraction3fP &si,
                      bool shading_frame = true,
                      MaskP active = true) const;

    /// Compatibility wrapper, which strips the mask argument and invokes \ref normal_derivative()
    std::pair<Vector3f, Vector3f>
    normal_derivative(const SurfaceInteraction3f &si,
                      bool shading_frame, bool /* unused */) const {
        return normal_derivative(si, shading_frame);
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Miscellaneous
    // =============================================================

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
    const Emitter *emitter() const { return m_emitter.get(); }

    /// Return the area emitter associated with this shape (if any)
    Emitter *emitter() { return m_emitter.get(); }

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

private:
    // Internal

    template <typename Interaction,
              typename Value = typename Interaction::Value,
              typename Point2 = Point<Value, 2>,
              typename Point3 = Point<Value, 3>,
              typename DirectionSample = mitsuba::DirectionSample<Point3>,
              typename Mask = mask_t<Value>>
    DirectionSample sample_direction_fallback(const Interaction &it,
                                           const Point2 &sample,
                                           Mask mask) const;

    template <typename Interaction, typename DirectionSample,
              typename Value = typename Interaction::Value,
              typename Mask = mask_t<Value>>
    Value pdf_direction_fallback(const Interaction &it,
                                 const DirectionSample &ds,
                                 Mask mask) const;
};

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for packets of Shape pointers
// -----------------------------------------------------------------------

// Enable usage of array pointers for our types
ENOKI_CALL_SUPPORT_BEGIN(mitsuba::Shape)
    ENOKI_CALL_SUPPORT_METHOD(normal_derivative)
    ENOKI_CALL_SUPPORT_METHOD(fill_surface_interaction)
    ENOKI_CALL_SUPPORT_GETTER_TYPE(emitter, m_emitter, const mitsuba::Emitter *)
    ENOKI_CALL_SUPPORT_GETTER_TYPE(sensor, m_sensor, const mitsuba::Sensor *)
    ENOKI_CALL_SUPPORT_GETTER_TYPE(bsdf, m_bsdf, const mitsuba::BSDF *)
    ENOKI_CALL_SUPPORT_GETTER_TYPE(interior_medium, m_interior_medium, const mitsuba::Medium *)
    ENOKI_CALL_SUPPORT_GETTER_TYPE(exterior_medium, m_exterior_medium, const mitsuba::Medium *)
    auto is_emitter() const { return neq(emitter(), nullptr); }
    auto is_sensor() const { return neq(sensor(), nullptr); }
    auto is_medium_transition() const { return neq(interior_medium(), nullptr) ||
                                               neq(exterior_medium(), nullptr); }
ENOKI_CALL_SUPPORT_END(mitsuba::Shape)

//! @}
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
//! @{ \name Macro for template implementation of shape's functions
// -----------------------------------------------------------------------

/*
 * \brief This macro should be used in the definition of Shape
 * plugins to instantiate concrete versions of the functions.
 */
#define MTS_IMPLEMENT_SHAPE()                                                            \
    PositionSample3f sample_position(Float time, const Point2f &sample) const override { \
        return sample_position_impl(time, sample, true);                                 \
    }                                                                                    \
    PositionSample3fP sample_position(FloatP time, const Point2fP &sample,               \
                                      MaskP active) const override {                     \
        return sample_position_impl(time, sample, active);                               \
    }                                                                                    \
    Float pdf_position(const PositionSample3f &ps) const override {                      \
        return pdf_position_impl(ps, true);                                              \
    }                                                                                    \
    FloatP pdf_position(const PositionSample3fP &ps,                                     \
                        MaskP active) const override {                                   \
        return pdf_position_impl(ps, active);                                            \
    }                                                                                    \
                                                                                         \
    DirectionSample3f sample_direction(const Interaction3f &it,                          \
                                       const Point2f &sample) const override {           \
        return sample_direction_impl(it, sample, true);                                  \
    }                                                                                    \
    DirectionSample3fP sample_direction(const Interaction3fP &it,                        \
                                        const Point2fP &sample,                          \
                                        MaskP active) const override {                   \
        return sample_direction_impl(it, sample, active);                                \
    }                                                                                    \
    Float pdf_direction(const Interaction3f &it,                                         \
                        const DirectionSample3f &ds) const override {                    \
        return pdf_direction_impl(it, ds, true);                                         \
    }                                                                                    \
    FloatP pdf_direction(const Interaction3fP &it, const DirectionSample3fP &ds,         \
                         MaskP active) const override {                                  \
        return pdf_direction_impl(it, ds, active);                                       \
    }                                                                                    \
                                                                                         \
    std::pair<bool, Float> ray_intersect(const Ray3f &ray,                               \
                                         Float * cache) const override {                 \
        return ray_intersect_impl(ray, cache, true);                                     \
    }                                                                                    \
    std::pair<MaskP, FloatP> ray_intersect(const Ray3fP &ray, FloatP * cache,            \
                                           MaskP active) const override {                \
        return ray_intersect_impl(ray, cache, active);                                   \
    }                                                                                    \
    bool ray_test(const Ray3f &ray) const override {                                     \
        return ray_test_impl(ray, true);                                                 \
    }                                                                                    \
    MaskP ray_test(const Ray3fP &ray, MaskP active) const override {                     \
        return ray_test_impl(ray, active);                                               \
    }                                                                                    \
                                                                                         \
    void fill_surface_interaction(const Ray3f &ray, const Float *cache,                  \
                                  SurfaceInteraction3f &si) const override {             \
        fill_surface_interaction_impl(ray, cache, si, true);                             \
    }                                                                                    \
    void fill_surface_interaction(const Ray3fP &ray, const FloatP *cache,                \
                                  SurfaceInteraction3fP &si,                             \
                                  MaskP active) const override {                         \
        fill_surface_interaction_impl(ray, cache, si, active);                           \
    }                                                                                    \
                                                                                         \
    std::pair<Vector3f, Vector3f> normal_derivative(const SurfaceInteraction3f &si,      \
                                                    bool shading_frame) const override { \
        return normal_derivative_impl(si, shading_frame, true);                          \
    }                                                                                    \
    std::pair<Vector3fP, Vector3fP> normal_derivative(const SurfaceInteraction3fP &si,   \
                                                      bool shading_frame,                \
                                                      MaskP active) const override {     \
        return normal_derivative_impl(si, shading_frame, active);                        \
    }

//! @}
// -----------------------------------------------------------------------
