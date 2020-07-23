#pragma once

#include <mitsuba/render/records.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/bbox.h>
#include <enoki/stl.h>

#if defined(MTS_ENABLE_OPTIX)
#  include <mitsuba/render/optix/common.h>
#endif

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Base class of all geometric shapes in Mitsuba
 *
 * This class provides core functionality for sampling positions on surfaces,
 * computing ray intersections, and bounding shapes within ray intersection
 * acceleration data structures.
 */
template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER Shape : public Object {
public:
    MTS_IMPORT_TYPES(BSDF, Medium, Emitter, Sensor, MeshAttribute);

    // Use 32 bit indices to keep track of indices to conserve memory
    using ScalarIndex = uint32_t;
    using ScalarSize  = uint32_t;

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
    virtual PositionSample3f sample_position(Float time, const Point2f &sample,
                                             Mask active = true) const;

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
    virtual Float pdf_position(const PositionSample3f &ps, Mask active = true) const;

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
    virtual DirectionSample3f sample_direction(const Interaction3f &it, const Point2f &sample,
                                               Mask active = true) const;

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
    virtual Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                                Mask active = true) const;

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
    virtual PreliminaryIntersection3f ray_intersect_preliminary(const Ray3f &ray,
                                                                Mask active = true) const;

    /**
     * \brief Fast ray shadow test
     *
     * Efficiently test whether the shape is intersected by the given ray, and
     * cache preliminary information about the intersection if that is the
     * case.
     *
     * No details about the intersection are returned, hence the function is
     * only useful for visibility queries. For most shapes, the implementation
     * will simply forward the call to \ref ray_intersect_preliminary(). When
     * the shape actually contains a nested kd-tree, some optimizations are possible.
     *
     * \param ray
     *     The ray to be tested for an intersection
     */
    virtual Mask ray_test(const Ray3f &ray, Mask active = true) const;

    /**
     * \brief Compute and return detailed information related to a surface interaction
     *
     * The implementation should at most compute the fields \c p, \c uv, \c n,
     * \c sh_frame.n, \c dp_du, \c dp_dv, \c dn_du and \c dn_dv. The \c flags parameter
     * specifies which of those fields should be computed.
     *
     * The fields \c t, \c time, \c wavelengths, \c shape, \c prim_index, \c instance,
     * will already have been initialized by the caller. The field \c wi is initialized
     * by the caller following the call to \ref compute_surface_interaction(), and
     * \c duv_dx, and \c duv_dy are left uninitialized.
     *
     * \param ray
     *      Ray associated with the ray intersection
     * \param flags
     *      Data structure carrying information about the ray intersection
     * \param flags
     *      Flags specifying which information should be computed
     * \return
     *      A data structure containing the detailed information
     */
    virtual SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                             PreliminaryIntersection3f pi,
                                                             HitComputeFlags flags = HitComputeFlags::All,
                                                             Mask active = true) const;

    /**
     * \brief Test for an intersection and return detailed information
     *
     * This operation combines the prior \ref ray_intersect_preliminary() and
     * \ref compute_surface_interaction() operations.
     *
     * \param ray
     *     The ray to be tested for an intersection
     *
     * \param flags
     *     Describe how the detailed information should be computed
     */
    SurfaceInteraction3f ray_intersect(const Ray3f &ray,
                                       HitComputeFlags flags = HitComputeFlags::All,
                                       Mask active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Miscellaneous query routines
    // =============================================================

    /**
     * \brief Return an axis aligned box that bounds all shape primitives
     * (including any transformations that may have been applied to them)
     */
    virtual ScalarBoundingBox3f bbox() const = 0;

    /**
     * \brief Return an axis aligned box that bounds a single shape primitive
     * (including any transformations that may have been applied to it)
     *
     * \remark
     *     The default implementation simply calls \ref bbox()
     */
    virtual ScalarBoundingBox3f bbox(ScalarIndex index) const;

    /**
     * \brief Return an axis aligned box that bounds a single shape primitive
     * after it has been clipped to another bounding box.
     *
     * This is extremely important to construct high-quality kd-trees. The
     * default implementation just takes the bounding box returned by
     * \ref bbox(ScalarIndex index) and clips it to \a clip.
     */
    virtual ScalarBoundingBox3f bbox(ScalarIndex index,
                                     const ScalarBoundingBox3f &clip) const;

    /**
     * \brief Return the shape's surface area.
     *
     * The function assumes that the object is not undergoing
     * some kind of time-dependent scaling.
     *
     * The default implementation throws an exception.
     */
    virtual ScalarFloat surface_area() const;

    /**
     * \brief Evaluate a specific shape attribute at the given surface interaction.
     *
     * Shape attributes are user-provided fields that provide extra
     * information at an intersection. An example of this would be a per-vertex
     * or per-face color on a triangle mesh.
     *
     * \param name
     *     Name of the attribute to evaluate
     *
     * \param si
     *     Surface interaction associated with the query
     *
     * \return
     *     An unpolarized spectral power distribution or reflectance value
     *
     * The default implementation throws an exception.
     */
    virtual UnpolarizedSpectrum eval_attribute(const std::string &name,
                                               const SurfaceInteraction3f &si,
                                               Mask active = true) const;

    /**
     * \brief Monochromatic evaluation of a shape attribute at the given surface interaction
     *
     * This function differs from \ref eval_attribute() in that it provided raw access to
     * scalar intensity/reflectance values without any color processing (e.g.
     * spectral upsampling).
     *
     * \param name
     *     Name of the attribute to evaluate
     *
     * \param si
     *     Surface interaction associated with the query
     *
     * \return
     *     An scalar intensity or reflectance value
     *
     * The default implementation throws an exception.
     */
    virtual Float eval_attribute_1(const std::string &name,
                                   const SurfaceInteraction3f &si,
                                   Mask active = true) const;

    /**
     * \brief Trichromatic evaluation of a shape attribute at the given surface interaction
     *
     * This function differs from \ref eval_attribute() in that it provided raw access to
     * RGB intensity/reflectance values without any additional color processing
     * (e.g. RGB-to-spectral upsampling).
     *
     * \param name
     *     Name of the attribute to evaluate
     *
     * \param si
     *     Surface interaction associated with the query
     *
     * \return
     *     An trichromatic intensity or reflectance value
     *
     * The default implementation throws an exception.
     */
    virtual Color3f eval_attribute_3(const std::string &name,
                                     const SurfaceInteraction3f &si,
                                     Mask active = true) const;

    /**
     * \brief Parameterize the mesh using UV values
     *
     * This function maps a 2D UV value to a surface interaction data
     * structure. Its behavior is only well-defined in regions where this
     * mapping is bijective. Only the mesh data structure currently implements
     * this interface via ray tracing, others are to follow later.
     * The default implementation throws.
     */
    virtual SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                                       Mask active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Miscellaneous
    // =============================================================

    /// Return a string identifier
    std::string id() const override;

    /// Is this shape a triangle mesh?
    bool is_mesh() const { return class_()->derives_from(Mesh<Float, Spectrum>::m_class); }

    /// Is this shape a shapegroup?
    bool is_shapegroup() const { return class_()->name() == "ShapeGroupPlugin"; };

    /// Is this shape an instance?
    bool is_instance() const { return class_()->name() == "Instance"; };

    /// Does the surface of this shape mark a medium transition?
    bool is_medium_transition() const { return m_interior_medium.get() != nullptr ||
                                               m_exterior_medium.get() != nullptr; }

    /// Return the medium that lies on the interior of this shape
    const Medium *interior_medium() const { return m_interior_medium.get(); }

    /// Return the medium that lies on the exterior of this shape
    const Medium *exterior_medium() const { return m_exterior_medium.get(); }

    /// Return the shape's BSDF
    const BSDF *bsdf() const { return m_bsdf.get(); }

    /// Return the shape's BSDF
    BSDF *bsdf() { return m_bsdf.get(); }

    /// Is this shape also an area emitter?
    bool is_emitter() const { return (bool) m_emitter; }

    /// Return the area emitter associated with this shape (if any)
    const Emitter *emitter(Mask /* unused */ = false) const { return m_emitter.get(); }

    /// Return the area emitter associated with this shape (if any)
    Emitter *emitter(Mask /* unused */ = false) { return m_emitter.get(); }

    /// Is this shape also an area sensor?
    bool is_sensor() const { return (bool) m_sensor; }

    /// Return the area sensor associated with this shape (if any)
    const Sensor *sensor() const { return m_sensor.get(); }
    /// Return the area sensor associated with this shape (if any)
    Sensor *sensor() { return m_sensor.get(); }

    /**
     * \brief Returns the number of sub-primitives that make up this shape
     *
     * \remark
     *     The default implementation simply returns \c 1
     */
    virtual ScalarSize primitive_count() const;

    /**
     * \brief Return the number of primitives (triangles, hairs, ..)
     * contributed to the scene by this shape
     *
     * Includes instanced geometry. The default implementation simply returns
     * the same value as \ref primitive_count().
     */
    virtual ScalarSize effective_primitive_count() const;


#if defined(MTS_ENABLE_EMBREE)
    /// Return the Embree version of this shape
    virtual RTCGeometry embree_geometry(RTCDevice device);
#endif

#if defined(MTS_ENABLE_OPTIX)
    /**
     * \brief Populates the GPU data buffer, used in the Optix Hitgroup sbt records.
     *
     * \remark
     *      Actual implementations of this method should allocate the field \ref
     *      m_optix_data_ptr on the GPU and populate it with the Optix representation
     *      of the class.
     *
     * The default implementation throws an exception.
     */
    virtual void optix_prepare_geometry();

    /**
     * \brief Fills the OptixBuildInput associated with this shape.
     *
     * \param build_input
     *     A reference to the build input to be filled. The field
     *     build_input.type has to be set, along with the associated members. For
     *     now, Mistuba only supports the types \ref OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES
     *     and \ref OPTIX_BUILD_INPUT_TYPE_TRIANGLES.
     *
     * The default implementation assumes that an implicit Shape (custom primitive
     * build type) is begin constructed, with its GPU data stored at \ref m_optix_data_ptr.
     */
    virtual void optix_build_input(OptixBuildInput& build_input) const;

    /**
     * \brief Prepares and fills the \ref OptixInstance(s) associated with this
     * shape. This process includes generating the Optix instance acceleration
     * structure (IAS) represented by this shape, and pushing OptixInstance
     * structs to the provided instances vector.
     *
     * \remark
     *     This method is currently only implemented for the \ref Instance
     *     and \ref ShapeGroup plugin.
     *
     * \param context
     *     The Optix context that was used to construct the rest of the scene's
     *     Optix representation.
     *
     * \param instances
     *     The array to which new OptixInstance should be appended.
     *
     * \param instance_id
     *     The instance id, used internally inside Optix to detect when a Shape
     *     is part of an Instance.
     *
     * \param transf
     *     The current to_world transformation (should allow for recursive instancing).
     *
     * The default implementation throws an exception.
     */
    virtual void optix_prepare_ias(const OptixDeviceContext& /*context*/,
                                   std::vector<OptixInstance>& /*instances*/,
                                   uint32_t /*instance_id*/,
                                   const ScalarTransform4f& /*transf*/);

    /**
     * \brief Creates and appends the HitGroupSbtRecord(s) associated with this
     * shape to the provided array.
     *
     * \remark
     *     This method can append multiple hitgroup records to the array
     *     (see the \ref Shapegroup plugin for an example).
     *
     * \param hitgroup_records
     *     The array of hitgroup records where the new HitGroupRecords should be
     *     appended.
     *
     * \param program_groups
     *     The array of available program groups (used to pack the Optix header
     *     at the beginning of the record).
     *
     * The default implementation creates a new HitGroupSbtRecord and fills its
     * \ref data field with \ref m_optix_data_ptr. It then calls \ref
     * optixSbtRecordPackHeader with one of the OptixProgramGroup of the \ref
     * program_groups array (the actual program group index is infered by the
     * type of the Shape, see \ref get_shape_descr_idx()).
     */
    virtual void optix_fill_hitgroup_records(std::vector<HitGroupSbtRecord> &hitgroup_records,
                                             const OptixProgramGroup *program_groups);
#endif

    void traverse(TraversalCallback *callback) override;
    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override;

    /// Return whether shape's parameters require gradients (default implementation return false)
    virtual bool parameters_grad_enabled() const;

    //! @}
    // =============================================================

    ENOKI_CALL_SUPPORT_FRIEND()
    ENOKI_PINNED_OPERATOR_NEW(Float)

    MTS_DECLARE_CLASS()
protected:
    Shape(const Properties &props);
    inline Shape() { }
    virtual ~Shape();

    /// Explicitly register this shape as the parent of the provided sub-objects (emitters, etc.)
    void set_children();
    std::string get_children_string() const;
protected:
    ref<BSDF> m_bsdf;
    ref<Emitter> m_emitter;
    ref<Sensor> m_sensor;
    ref<Medium> m_interior_medium;
    ref<Medium> m_exterior_medium;
    std::string m_id;

    ScalarTransform4f m_to_world;
    ScalarTransform4f m_to_object;

#if defined(MTS_ENABLE_OPTIX)
    /// OptiX hitgroup data buffer
    void* m_optix_data_ptr = nullptr;
#endif
};

MTS_EXTERN_CLASS_RENDER(Shape)
NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for vectorized function calls
// -----------------------------------------------------------------------

ENOKI_CALL_SUPPORT_TEMPLATE_BEGIN(mitsuba::Shape)
    ENOKI_CALL_SUPPORT_METHOD(compute_surface_interaction)
    ENOKI_CALL_SUPPORT_METHOD(eval_attribute)
    ENOKI_CALL_SUPPORT_METHOD(eval_attribute_1)
    ENOKI_CALL_SUPPORT_METHOD(eval_attribute_3)
    ENOKI_CALL_SUPPORT_GETTER_TYPE(emitter, m_emitter, const typename Class::Emitter *)
    ENOKI_CALL_SUPPORT_GETTER_TYPE(sensor, m_sensor, const typename Class::Sensor *)
    ENOKI_CALL_SUPPORT_GETTER_TYPE(bsdf, m_bsdf, const typename Class::BSDF *)
    ENOKI_CALL_SUPPORT_GETTER_TYPE(interior_medium, m_interior_medium,
                                   const typename Class::Medium *)
    ENOKI_CALL_SUPPORT_GETTER_TYPE(exterior_medium, m_exterior_medium,
                                   const typename Class::Medium *)
    auto is_emitter() const { return neq(emitter(), nullptr); }
    auto is_sensor() const { return neq(sensor(), nullptr); }
    auto is_medium_transition() const { return neq(interior_medium(), nullptr) ||
                                               neq(exterior_medium(), nullptr); }
ENOKI_CALL_SUPPORT_TEMPLATE_END(mitsuba::Shape)

//! @}
// -----------------------------------------------------------------------
