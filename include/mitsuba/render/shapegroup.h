#include <mitsuba/core/fwd.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/shape.h>

#if defined(MI_ENABLE_EMBREE)
#  include <embree3/rtcore.h>
#else
#  include <mitsuba/render/kdtree.h>
#endif

#if defined(MI_ENABLE_CUDA)
#  include <mitsuba/render/optix/shapes.h>
#endif

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MI_EXPORT_LIB ShapeGroup : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_id, m_dirty)
    MI_IMPORT_TYPES(ShapeKDTree, ShapePtr)

    using typename Base::ScalarSize;
    using typename Base::ScalarRay3f;

    ShapeGroup(const Properties &props);
    ~ShapeGroup();

#if defined(MI_ENABLE_EMBREE)
    RTCGeometry embree_geometry(RTCDevice device) override;
#else
    std::tuple<ScalarFloat, ScalarPoint2f, ScalarUInt32, ScalarUInt32>
    ray_intersect_preliminary_scalar(const ScalarRay3f &ray) const override;
    bool ray_test_scalar(const ScalarRay3f &ray) const override;
#endif

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     uint32_t recursion_depth,
                                                     Mask active) const override;

    ScalarSize primitive_count() const override;

    ScalarBoundingBox3f bbox() const override{ return m_bbox; }

    Float surface_area() const override { return 0.f; }

    MI_INLINE ScalarSize effective_primitive_count() const override { return 0; }

    /// Return whether this shapegroup contains triangle mesh shapes
    bool has_meshes() const { return m_has_meshes; }

    /// Return whether this shapegroup contains other type of shapes
    bool has_others() const { return m_has_others; }

    void traverse(TraversalCallback *callback) override;
    void parameters_changed(const std::vector<std::string> &/*keys*/ = {}) override;
    bool parameters_grad_enabled() const override;

    std::string to_string() const override;

#if defined(MI_ENABLE_CUDA)
    void optix_prepare_ias(const OptixDeviceContext& context,
                           std::vector<OptixInstance>& instances,
                           uint32_t instance_id,
                           const ScalarTransform4f& transf) override;

    void optix_fill_hitgroup_records(std::vector<HitGroupSbtRecord> &hitgroup_records,
                                     const OptixProgramGroup *program_groups) override;


    void optix_prepare_geometry() override;

    /// Build OptiX geometry acceleration structures
    void optix_build_gas(const OptixDeviceContext& context);
#endif

    MI_DECLARE_CLASS()
private:
    ScalarBoundingBox3f m_bbox;
    std::vector<ref<Base>> m_shapes;

#if defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_CUDA)
    DynamicBuffer<UInt32> m_shapes_registry_ids;
#endif

#if defined(MI_ENABLE_EMBREE)
    RTCScene m_embree_scene = nullptr;
    std::vector<int> m_embree_geometries;
#else
    ref<ShapeKDTree> m_kdtree;
#endif

#if defined(MI_ENABLE_CUDA)
    OptixAccelData m_accel;
    /// OptiX hitgroup sbt offset
    uint32_t m_sbt_offset;
#endif

    bool m_has_meshes, m_has_others;
};

MI_EXTERN_CLASS(ShapeGroup)
NAMESPACE_END(mitsuba)
