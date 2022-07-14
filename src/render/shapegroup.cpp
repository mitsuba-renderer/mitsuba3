#include <mitsuba/core/properties.h>
#include <mitsuba/render/shapegroup.h>
#include <mitsuba/render/optix_api.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT ShapeGroup<Float, Spectrum>::ShapeGroup(const Properties &props) {
    m_id = props.id();

#if !defined(MI_ENABLE_EMBREE)
    if constexpr (!dr::is_cuda_v<Float>)
        m_kdtree = new ShapeKDTree(props);
#endif
    m_has_meshes = false;
    m_has_others = false;

    // Add children to the underlying datastructure
    for (auto &kv : props.objects()) {
        const Class *c_class = kv.second->class_();
        if (c_class->name() == "Instance") {
            Throw("Nested instancing is not permitted");
        } else if (c_class->derives_from(MI_CLASS(Base))) {
            Base *shape = static_cast<Base *>(kv.second.get());
            ShapeGroup *shapegroup = dynamic_cast<ShapeGroup *>(kv.second.get());
            if (shapegroup)
                Throw("Nested ShapeGroup is not permitted");
            if (shape->is_emitter())
                Throw("Instancing of emitters is not supported");
            if (shape->is_sensor())
                Throw("Instancing of sensors is not supported");
            else {
                m_shapes.push_back(shape);
                shape->mark_as_instance();

#if defined(MI_ENABLE_EMBREE) || defined(MI_ENABLE_CUDA)
                m_bbox.expand(shape->bbox());
#endif

#if !defined(MI_ENABLE_EMBREE)
                if constexpr (!dr::is_cuda_v<Float>)
                    m_kdtree->add_shape(shape);
#endif
                m_has_meshes |= shape->is_mesh();
                m_has_others |= !shape->is_mesh();
            }
        } else {
            Throw("Tried to add an unsupported object of type \"%s\"", kv.second);
        }
    }
#if !defined(MI_ENABLE_EMBREE)
    if constexpr (!dr::is_cuda_v<Float>) {
        if (!m_kdtree->ready())
            m_kdtree->build();

        m_bbox = m_kdtree->bbox();
    }
#endif

#if defined(MI_ENABLE_LLVM)
    if constexpr (dr::is_llvm_v<Float>) {
        // Get shapes registry ids
        std::unique_ptr<uint32_t[]> data(new uint32_t[m_shapes.size()]);
        for (size_t i = 0; i < m_shapes.size(); i++)
            data[i] = jit_registry_get_id(dr::backend_v<Float>, m_shapes[i]);
        m_shapes_registry_ids =
            dr::load<DynamicBuffer<UInt32>>(data.get(), m_shapes.size());
    }
#endif
}

MI_VARIANT ShapeGroup<Float, Spectrum>::~ShapeGroup() {
#if defined(MI_ENABLE_EMBREE)
    if constexpr (!dr::is_cuda_v<Float>) {
        // Ensure all raytracing kernels are terminated before releasing the scene
        if constexpr (dr::is_llvm_v<Float>)
            dr::sync_thread();

        rtcReleaseScene(m_embree_scene);
    }
#endif
}

#if defined(MI_ENABLE_EMBREE)
MI_VARIANT RTCGeometry ShapeGroup<Float, Spectrum>::embree_geometry(RTCDevice device) {
    DRJIT_MARK_USED(device);
    if constexpr (!dr::is_cuda_v<Float>) {
        // Construct the BVH only once
        if (m_embree_scene == nullptr) {
            m_embree_scene = rtcNewScene(device);
            for (auto shape : m_shapes) {
                RTCGeometry geom = shape->embree_geometry(device);
                rtcAttachGeometry(m_embree_scene, geom);
                rtcReleaseGeometry(geom);
            }

            // Ensure shape data pointers are finished evaluating before building
            if constexpr (dr::is_llvm_v<Float>)
                dr::sync_thread();

            rtcCommitScene(m_embree_scene);
        }

        RTCGeometry instance = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_INSTANCE);
        rtcSetGeometryInstancedScene(instance, m_embree_scene);
        return instance;
    } else {
        Throw("embree_geometry() should only be called in CPU mode.");
    }
}
#else
MI_VARIANT
std::tuple<typename ShapeGroup<Float, Spectrum>::ScalarFloat,
           typename ShapeGroup<Float, Spectrum>::ScalarPoint2f,
           typename ShapeGroup<Float, Spectrum>::ScalarUInt32,
           typename ShapeGroup<Float, Spectrum>::ScalarUInt32>
ShapeGroup<Float, Spectrum>::ray_intersect_preliminary_scalar(const ScalarRay3f &ray) const {
    auto pi = m_kdtree->template ray_intersect_scalar<false>(ray);
    return { pi.t, pi.prim_uv, pi.shape_index, pi.prim_index };
}

MI_VARIANT
bool ShapeGroup<Float, Spectrum>::ray_test_scalar(const ScalarRay3f &ray) const {
    return m_kdtree->template ray_intersect_scalar<true>(ray).is_valid();
}
#endif

MI_VARIANT typename ShapeGroup<Float, Spectrum>::SurfaceInteraction3f
ShapeGroup<Float, Spectrum>::compute_surface_interaction(const Ray3f &ray,
                                                         const PreliminaryIntersection3f &pi,
                                                         uint32_t ray_flags,
                                                         uint32_t recursion_depth,
                                                         Mask active) const {
    MI_MASK_ARGUMENT(active);

    if (recursion_depth > 0)
        return dr::zeros<SurfaceInteraction3f>();

    ShapePtr shape = pi.shape;

    if constexpr (!dr::is_cuda_v<Float>) {
        if constexpr (!dr::is_array_v<Float>) {
            Assert(pi.shape_index < m_shapes.size());
            shape = m_shapes[pi.shape_index];
        } else {
#if defined(MI_ENABLE_LLVM)
            shape = dr::gather<UInt32>(m_shapes_registry_ids, pi.shape_index, active);
#endif
        }
    }

    return shape->compute_surface_interaction(ray, pi, ray_flags, 1, active);
}

MI_VARIANT typename ShapeGroup<Float, Spectrum>::ScalarSize
ShapeGroup<Float, Spectrum>::primitive_count() const {
#if !defined(MI_ENABLE_EMBREE)
    if constexpr (!dr::is_cuda_v<Float>)
        return m_kdtree->primitive_count();
#endif

    ScalarSize count = 0;
    for (auto shape : m_shapes)
        count += shape->primitive_count();

    return count;
}

#if defined(MI_ENABLE_CUDA)
MI_VARIANT void ShapeGroup<Float, Spectrum>::optix_prepare_ias(
    const OptixDeviceContext &context, std::vector<OptixInstance> &instances,
    uint32_t instance_id, const ScalarTransform4f &transf) {
    prepare_ias(context, m_shapes, m_sbt_offset, m_accel, instance_id, transf, instances);
}

MI_VARIANT void ShapeGroup<Float, Spectrum>::optix_fill_hitgroup_records(std::vector<HitGroupSbtRecord> &hitgroup_records,
                                                                         const OptixProgramGroup *program_groups) {
    m_sbt_offset = (uint32_t) hitgroup_records.size();
    fill_hitgroup_records(m_shapes, hitgroup_records, program_groups);
}

#endif

MI_VARIANT std::string ShapeGroup<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
        oss << "ShapeGroup[" << std::endl
            << "  name = \"" << m_id << "\"," << std::endl
            << "  prim_count = " << primitive_count() << std::endl
            << "]";
    return oss.str();
}

MI_IMPLEMENT_CLASS_VARIANT(ShapeGroup, Shape)
MI_INSTANTIATE_CLASS(ShapeGroup)
NAMESPACE_END(mitsuba)
