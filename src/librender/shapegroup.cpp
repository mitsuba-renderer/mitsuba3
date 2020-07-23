#include <mitsuba/core/properties.h>
#include <mitsuba/render/shapegroup.h>
#include <mitsuba/render/optix_api.h>

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT ShapeGroup<Float, Spectrum>::ShapeGroup(const Properties &props) {
    m_id = props.id();

#if !defined(MTS_ENABLE_EMBREE)
    m_kdtree = new ShapeKDTree(props);
#endif

    // Add children to the underlying datastructure
    for (auto &kv : props.objects()) {
        const Class *c_class = kv.second->class_();
        if (c_class->name() == "Instance") {
            Throw("Nested instancing is not permitted");
        } else if (c_class->derives_from(MTS_CLASS(Base))) {
            Base *shape = static_cast<Base *>(kv.second.get());
            ShapeGroup *shapegroup = dynamic_cast<ShapeGroup *>(kv.second.get());
            if (shapegroup)
                Throw("Nested ShapeGroup is not permitted");
            if (shape->is_emitter())
                Throw("Instancing of emitters is not supported");
            if (shape->is_sensor())
                Throw("Instancing of sensors is not supported");
            else {
#if defined(MTS_ENABLE_EMBREE) || defined(MTS_ENABLE_OPTIX)
                m_shapes.push_back(shape);
                m_bbox.expand(shape->bbox());
#endif
#if !defined(MTS_ENABLE_EMBREE)
                m_kdtree->add_shape(shape);
#endif
            }
        } else {
            Throw("Tried to add an unsupported object of type \"%s\"", kv.second);
        }
    }
#if !defined(MTS_ENABLE_EMBREE)
    if (!m_kdtree->ready())
        m_kdtree->build();

    m_bbox = m_kdtree->bbox();
#endif
}

MTS_VARIANT ShapeGroup<Float, Spectrum>::~ShapeGroup() {
#if defined(MTS_ENABLE_EMBREE)
    if constexpr (!is_cuda_array_v<Float>)
        rtcReleaseScene(m_embree_scene);
#endif
}

#if defined(MTS_ENABLE_EMBREE)
MTS_VARIANT RTCGeometry ShapeGroup<Float, Spectrum>::embree_geometry(RTCDevice device) override {
    if constexpr (!is_cuda_array_v<Float>) {
        // Construct the BVH only once
        if (m_embree_scene == nullptr) {
            m_embree_scene = rtcNewScene(device);
            for (auto shape : m_shapes)
                rtcAttachGeometry(m_embree_scene, shape->embree_geometry(device));
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
MTS_VARIANT typename ShapeGroup<Float, Spectrum>::PreliminaryIntersection3f
ShapeGroup<Float, Spectrum>::ray_intersect_preliminary(const Ray3f &ray,
                                                       Mask active) const {
    MTS_MASK_ARGUMENT(active);

    if constexpr (is_cuda_array_v<Float>)
        Throw("ShapeGroup::ray_intersect_preliminary() should only be called in CPU mode.");

    return m_kdtree->template ray_intersect_preliminary<false>(ray, active);
}

MTS_VARIANT typename ShapeGroup<Float, Spectrum>::Mask
ShapeGroup<Float, Spectrum>::ray_test(const Ray3f &ray, Mask active) const {
    MTS_MASK_ARGUMENT(active);

    if constexpr (is_cuda_array_v<Float>)
        Throw("ShapeGroup::ray_test() should only be called in CPU mode.");

    return m_kdtree->template ray_intersect_preliminary<true>(ray, active).is_valid();
}
#endif

MTS_VARIANT typename ShapeGroup<Float, Spectrum>::SurfaceInteraction3f
ShapeGroup<Float, Spectrum>::compute_surface_interaction(const Ray3f &ray,
                                                         PreliminaryIntersection3f pi,
                                                         HitComputeFlags flags,
                                                         Mask active) const {
    MTS_MASK_ARGUMENT(active);

#if defined(MTS_ENABLE_EMBREE)
    if constexpr (!is_cuda_array_v<Float>) {
        if constexpr (!is_array_v<Float>) {
            Assert(pi.shape_index < m_shapes.size());
            pi.shape = m_shapes[pi.shape_index];
        } else {
            using ShapePtr = replace_scalar_t<Float, const Base *>;
            Assert(all(pi.shape_index < m_shapes.size()));
            pi.shape = gather<ShapePtr>(m_shapes.data(), pi.shape_index, active);
        }

        SurfaceInteraction3f si = pi.shape->compute_surface_interaction(ray, pi, flags, active);
        si.shape = pi.shape;

        return si;
    }
#endif

    return pi.shape->compute_surface_interaction(ray, pi, flags, active);
}

MTS_VARIANT typename ShapeGroup<Float, Spectrum>::ScalarSize
ShapeGroup<Float, Spectrum>::primitive_count() const {
#if !defined(MTS_ENABLE_EMBREE)
    if constexpr (!is_cuda_array_v<Float>)
        return m_kdtree->primitive_count();
#endif

#if defined(MTS_ENABLE_EMBREE) || defined(MTS_ENABLE_OPTIX)
    ScalarSize count = 0;
    for (auto shape : m_shapes)
        count += shape->primitive_count();

    return count;
#endif
}

#if defined(MTS_ENABLE_OPTIX)
MTS_VARIANT void ShapeGroup<Float, Spectrum>::optix_prepare_ias(
    const OptixDeviceContext &context, std::vector<OptixInstance> &instances,
    uint32_t instance_id, const ScalarTransform4f &transf) {
    prepare_ias(context, m_shapes, m_sbt_offset, m_accel, instance_id, transf, instances);
}

MTS_VARIANT void ShapeGroup<Float, Spectrum>::optix_fill_hitgroup_records(std::vector<HitGroupSbtRecord> &hitgroup_records,
                                                                          const OptixProgramGroup *program_groups) {
    m_sbt_offset = (uint32_t) hitgroup_records.size();
    fill_hitgroup_records(m_shapes, hitgroup_records, program_groups);
}

#endif

MTS_VARIANT std::string ShapeGroup<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
        oss << "ShapeGroup[" << std::endl
            << "  name = \"" << m_id << "\"," << std::endl
            << "  prim_count = " << primitive_count() << std::endl
            << "]";
    return oss.str();
}

MTS_IMPLEMENT_CLASS_VARIANT(ShapeGroup, Shape)
MTS_INSTANTIATE_CLASS(ShapeGroup)
NAMESPACE_END(mitsuba)
