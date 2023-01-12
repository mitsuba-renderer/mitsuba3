#pragma once

#include <drjit/vcall.h>
#include <mitsuba/render/records.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/bbox.h>
#include <mitsuba/core/field.h>
#include <drjit/packet.h>

#if defined(MI_ENABLE_CUDA)
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
class MI_EXPORT_LIB Shape : public Object {
public:
    MI_IMPORT_TYPES(BSDF, Medium, Emitter, Sensor, MeshAttribute);

    // Use 32 bit indices to keep track of indices to conserve memory
    using ScalarIndex = uint32_t;
    using ScalarSize  = uint32_t;
    using ScalarRay3f = Ray<ScalarPoint3f, Spectrum>;

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
     * \brief Fast ray intersection
     *
     * Efficiently test whether the shape is intersected by the given ray, and
     * return preliminary information about the intersection if that is the
     * case.
     *
     * If the intersection is deemed relevant (e.g. the closest to the ray
     * origin), detailed intersection information can later be obtained via the
     * \ref create_surface_interaction() method.
     *
     * \param ray
     *     The ray to be tested for an intersection
     */
    virtual PreliminaryIntersection3f ray_intersect_preliminary(const Ray3f &ray,
                                                                Mask active = true) const;

    /**
     * \brief Fast ray shadow test
     *
     * Efficiently test whether the shape is intersected by the given ray.
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
     * \param pi
     *      Data structure carrying information about the ray intersection
     * \param ray_flags
     *      Flags specifying which information should be computed
     * \param recursion_depth
     *      Integer specifying the recursion depth for nested virtual function
     *      call to this method (e.g. used for instancing).
     * \return
     *      A data structure containing the detailed information
     */
    virtual SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                             const PreliminaryIntersection3f &pi,
                                                             uint32_t ray_flags = +RayFlags::All,
                                                             uint32_t recursion_depth = 0,
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
                                       uint32_t ray_flags = +RayFlags::All,
                                       Mask active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Packet versions of ray test/intersection routines
    // =============================================================
    /**
     * \brief Scalar test for an intersection and return detailed information
     *
     * This operation is used by the KDTree acceleration structure.
     *
     * \param ray
     *     The ray to be tested for an intersection
     *
     * \return
     *     A tuple containing the following field: \c t, \c uv, \c shape_index,
     *     \c prim_index. The \c shape_index should be only used by the
     *     \ref ShapeGroup class and be set to \c (uint32_t)-1 otherwise.
     */
    virtual std::tuple<ScalarFloat, ScalarPoint2f, ScalarUInt32, ScalarUInt32>
    ray_intersect_preliminary_scalar(const ScalarRay3f &ray) const;
    virtual bool ray_test_scalar(const ScalarRay3f &ray) const;

    /// Macro to declare packet versions of the scalar routine above
    #define MI_DECLARE_RAY_INTERSECT_PACKET(N)                            \
        using FloatP##N   = dr::Packet<dr::scalar_t<Float>, N>;            \
        using UInt32P##N  = dr::uint32_array_t<FloatP##N>;                 \
        using MaskP##N    = dr::mask_t<FloatP##N>;                         \
        using Point2fP##N = Point<FloatP##N, 2>;                           \
        using Point3fP##N = Point<FloatP##N, 3>;                           \
        using Ray3fP##N   = Ray<Point3fP##N, Spectrum>;                    \
        virtual std::tuple<FloatP##N, Point2fP##N, UInt32P##N, UInt32P##N> \
        ray_intersect_preliminary_packet(const Ray3fP##N &ray,             \
                                         MaskP##N active = true) const;    \
        virtual MaskP##N ray_test_packet(const Ray3fP##N &ray,             \
                                         MaskP##N active = true) const;

    MI_DECLARE_RAY_INTERSECT_PACKET(4)
    MI_DECLARE_RAY_INTERSECT_PACKET(8)
    MI_DECLARE_RAY_INTERSECT_PACKET(16)

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
    virtual Float surface_area() const;

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
     * mapping is bijective.
     * The default implementation throws.
     */
    virtual SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                                       uint32_t ray_flags = +RayFlags::All,
                                                       Mask active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Miscellaneous
    // =============================================================

    /// Return a string identifier
    std::string id() const override { return m_id; }

    /// Set a string identifier
    void set_id(const std::string& id) override { m_id = id; };

    /// Is this shape a triangle mesh?
    bool is_mesh() const;

    /// Is this shape a shapegroup?
    bool is_shapegroup() const { return class_()->name() == "ShapeGroupPlugin"; };

    /// Is this shape an instance?
    bool is_instance() const { return class_()->name() == "Instance"; };

    /// Does the surface of this shape mark a medium transition?
    bool is_medium_transition() const { return m_interior_medium.get() != nullptr ||
                                               m_exterior_medium.get() != nullptr; }

    /// Return the medium that lies on the interior of this shape
    const Medium *interior_medium(Mask /*unused*/ = true) const { return m_interior_medium.get(); }

    /// Return the medium that lies on the exterior of this shape
    const Medium *exterior_medium(Mask /*unused*/ = true) const { return m_exterior_medium.get(); }

    /// Return the shape's BSDF
    const BSDF *bsdf(Mask /*unused*/ = true) const { return m_bsdf.get(); }

    /// Return the shape's BSDF
    BSDF *bsdf(Mask /*unused*/ = true) { return m_bsdf.get(); }

    /// Is this shape also an area emitter?
    bool is_emitter() const { return (bool) m_emitter; }

    /// Return the area emitter associated with this shape (if any)
    const Emitter *emitter(Mask /*unused*/ = true) const { return m_emitter.get(); }

    /// Return the area emitter associated with this shape (if any)
    Emitter *emitter(Mask /*unused*/ = true) { return m_emitter.get(); }

    /// Is this shape also an area sensor?
    bool is_sensor() const { return (bool) m_sensor; }

    /// Return the area sensor associated with this shape (if any)
    const Sensor *sensor(Mask /*unused*/ = true) const { return m_sensor.get(); }
    /// Return the area sensor associated with this shape (if any)
    Sensor *sensor(Mask /*unused*/ = true) { return m_sensor.get(); }

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


#if defined(MI_ENABLE_EMBREE)
    /// Return the Embree version of this shape
    virtual RTCGeometry embree_geometry(RTCDevice device);
#endif

#if defined(MI_ENABLE_CUDA)
    /**
     * \brief Populates the GPU data buffer, used in the OptiX Hitgroup sbt records.
     *
     * \remark
     *      Actual implementations of this method should allocate the field \ref
     *      m_optix_data_ptr on the GPU and populate it with the OptiX representation
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
     *     now, Mitsuba only supports the types \ref OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES
     *     and \ref OPTIX_BUILD_INPUT_TYPE_TRIANGLES.
     *
     * The default implementation assumes that an implicit Shape (custom primitive
     * build type) is begin constructed, with its GPU data stored at \ref m_optix_data_ptr.
     */
    virtual void optix_build_input(OptixBuildInput& build_input) const;

    /**
     * \brief Prepares and fills the \ref OptixInstance(s) associated with this
     * shape. This process includes generating the OptiX instance acceleration
     * structure (IAS) represented by this shape, and pushing OptixInstance
     * structs to the provided instances vector.
     *
     * \remark
     *     This method is currently only implemented for the \ref Instance
     *     and \ref ShapeGroup plugin.
     *
     * \param context
     *     The OptiX context that was used to construct the rest of the scene's
     *     OptiX representation.
     *
     * \param instances
     *     The array to which new OptixInstance should be appended.
     *
     * \param instance_id
     *     The instance id, used internally inside OptiX to detect when a Shape
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
     *     The array of available program groups (used to pack the OptiX header
     *     at the beginning of the record).
     *
     * The default implementation creates a new HitGroupSbtRecord and fills its
     * \ref data field with \ref m_optix_data_ptr. It then calls \ref
     * optixSbtRecordPackHeader with one of the OptixProgramGroup of the \ref
     * program_groups array (the actual program group index is inferred by the
     * type of the Shape, see \ref get_shape_descr_idx()).
     */
    virtual void optix_fill_hitgroup_records(std::vector<HitGroupSbtRecord> &hitgroup_records,
                                             const OptixProgramGroup *program_groups);
#endif

    void traverse(TraversalCallback *callback) override;
    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override;

    /// Return whether the shape's geometry has changed
    bool dirty() const { return m_dirty; }

    /// Mark that the shape's geometry has changed
    void mark_dirty() { m_dirty = true; }

    // Mark that shape as an instance
    void mark_as_instance() { m_is_instance = true; }

    /// The \c Scene and \c ShapeGroup class needs access to \c Shape::m_dirty
    friend class Scene<Float, Spectrum>;
    friend class ShapeGroup<Float, Spectrum>;

    /// Return whether any shape's parameters require gradients (default return false)
    virtual bool parameters_grad_enabled() const;

    //! @}
    // =============================================================

    DRJIT_VCALL_REGISTER(Float, mitsuba::Shape)

    MI_DECLARE_CLASS()
protected:
    Shape(const Properties &props);
    inline Shape() { }
    virtual ~Shape();

    virtual void initialize();
    std::string get_children_string() const;
protected:
    ref<BSDF> m_bsdf;
    ref<Emitter> m_emitter;
    ref<Sensor> m_sensor;
    ref<Medium> m_interior_medium;
    ref<Medium> m_exterior_medium;
    std::string m_id;

    field<Transform4f, ScalarTransform4f> m_to_world;
    field<Transform4f, ScalarTransform4f> m_to_object;

    /// True if the shape is used in a \c ShapeGroup
    bool m_is_instance = false;

#if defined(MI_ENABLE_CUDA)
    /// OptiX hitgroup data buffer
    void* m_optix_data_ptr = nullptr;
#endif

protected:
    /// True if the shape's geometry has changed
    bool m_dirty = true;
};

MI_EXTERN_CLASS(Shape)
NAMESPACE_END(mitsuba)

#define MI_IMPLEMENT_RAY_INTERSECT_PACKET(N)                                   \
    using typename Base::FloatP##N;                                            \
    using typename Base::UInt32P##N;                                           \
    using typename Base::MaskP##N;                                             \
    using typename Base::Point2fP##N;                                          \
    using typename Base::Point3fP##N;                                          \
    using typename Base::Ray3fP##N;                                            \
    std::tuple<FloatP##N, Point2fP##N, UInt32P##N, UInt32P##N>                 \
    ray_intersect_preliminary_packet(                                          \
        const Ray3fP##N &ray, MaskP##N active) const override {                \
        (void) ray; (void) active;                                             \
        if constexpr (!dr::is_cuda_v<Float>)                                   \
            return ray_intersect_preliminary_impl<FloatP##N>(ray, active);     \
        else                                                                   \
            Throw("ray_intersect_preliminary_packet() CUDA not supported");    \
    }                                                                          \
    MaskP##N ray_test_packet(const Ray3fP##N &ray, MaskP##N active)            \
        const override {                                                       \
        (void) ray; (void) active;                                             \
        if constexpr (!dr::is_cuda_v<Float>)                                   \
            return ray_test_impl<FloatP##N>(ray, active);                      \
        else                                                                   \
            Throw("ray_intersect_preliminary_packet() CUDA not supported");    \
    }

// Macro to define ray intersection methods given an *_impl() templated implementation
#define MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()                                \
    PreliminaryIntersection3f ray_intersect_preliminary(                       \
        const Ray3f &ray, Mask active) const override {                        \
        MI_MASK_ARGUMENT(active);                                              \
        PreliminaryIntersection3f pi = dr::zeros<PreliminaryIntersection3f>(); \
        std::tie(pi.t, pi.prim_uv, pi.shape_index, pi.prim_index) =            \
            ray_intersect_preliminary_impl<Float>(ray, active);                \
        pi.shape = this;                                                       \
        return pi;                                                             \
    }                                                                          \
    Mask ray_test(const Ray3f &ray, Mask active) const override {              \
        MI_MASK_ARGUMENT(active);                                              \
        return ray_test_impl<Float>(ray, active);                              \
    }                                                                          \
    using typename Base::ScalarRay3f;                                          \
    std::tuple<ScalarFloat, ScalarPoint2f, ScalarUInt32, ScalarUInt32>         \
    ray_intersect_preliminary_scalar(const ScalarRay3f &ray) const override {  \
        return ray_intersect_preliminary_impl<ScalarFloat>(ray, true);         \
    }                                                                          \
    ScalarMask ray_test_scalar(const ScalarRay3f &ray) const override {        \
        return ray_test_impl<ScalarFloat>(ray, true);                          \
    }                                                                          \
    MI_IMPLEMENT_RAY_INTERSECT_PACKET(4)                                       \
    MI_IMPLEMENT_RAY_INTERSECT_PACKET(8)                                       \
    MI_IMPLEMENT_RAY_INTERSECT_PACKET(16)

// -----------------------------------------------------------------------
//! @{ \name Dr.Jit support for vectorized function calls
// -----------------------------------------------------------------------

DRJIT_VCALL_TEMPLATE_BEGIN(mitsuba::Shape)
    DRJIT_VCALL_METHOD(compute_surface_interaction)
    DRJIT_VCALL_METHOD(eval_attribute)
    DRJIT_VCALL_METHOD(eval_attribute_1)
    DRJIT_VCALL_METHOD(eval_attribute_3)
    DRJIT_VCALL_METHOD(ray_intersect_preliminary)
    DRJIT_VCALL_METHOD(ray_intersect)
    DRJIT_VCALL_METHOD(ray_test)
    DRJIT_VCALL_METHOD(sample_position)
    DRJIT_VCALL_METHOD(pdf_position)
    DRJIT_VCALL_METHOD(sample_direction)
    DRJIT_VCALL_METHOD(pdf_direction)
    DRJIT_VCALL_METHOD(eval_parameterization)
    DRJIT_VCALL_METHOD(surface_area)
    DRJIT_VCALL_GETTER(emitter, const typename Class::Emitter *)
    DRJIT_VCALL_GETTER(sensor, const typename Class::Sensor *)
    DRJIT_VCALL_GETTER(bsdf, const typename Class::BSDF *)
    DRJIT_VCALL_GETTER(interior_medium, const typename Class::Medium *)
    DRJIT_VCALL_GETTER(exterior_medium, const typename Class::Medium *)
    auto is_emitter() const { return neq(emitter(), nullptr); }
    auto is_sensor() const { return neq(sensor(), nullptr); }
    auto is_medium_transition() const { return neq(interior_medium(), nullptr) ||
                                               neq(exterior_medium(), nullptr); }
DRJIT_VCALL_TEMPLATE_END(mitsuba::Shape)

//! @}
// -----------------------------------------------------------------------
