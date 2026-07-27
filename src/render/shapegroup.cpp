#include <mitsuba/core/properties.h>
#include <mitsuba/render/shapegroup.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT ShapeGroup<Float, Spectrum>::ShapeGroup(const Properties &props)
    : Shape<Float, Spectrum>(props) {
    // ID is now stored in base class JitObject

#if !defined(MI_ENABLE_EMBREE)
    if constexpr (!dr::is_cuda_v<Float> && !dr::is_metal_v<Float>)
        m_kdtree = new ShapeKDTree(props);
#endif
    m_shape_types = 0;
    Base::m_shape_type = ShapeType::ShapeGroup;

    // Add children to the underlying data structure
    for (auto &prop : props.objects()) {
        Base *shape = prop.try_get<Base>();
        if (!shape)
            Throw("Tried to add an unsupported object of type \"%s\"", prop.get<ref<Object>>().get());
        if (shape->is_shape_group())
            Throw("Nested ShapeGroup is not permitted");
        if (shape->is_emitter())
            Throw("Instancing of emitters is not supported");
        if (shape->is_instance())
            Throw("Nested instancing is not permitted");
        if (shape->is_sensor())
            Throw("Instancing of sensors is not supported");
        else {
            m_shapes.push_back(shape);
            shape->mark_as_instance();

#if defined(MI_ENABLE_EMBREE) || defined(MI_ENABLE_CUDA) || defined(MI_ENABLE_METAL)
            m_bbox.expand(shape->bbox());
#endif

#if !defined(MI_ENABLE_EMBREE)
            if constexpr (!dr::is_cuda_v<Float> && !dr::is_metal_v<Float>)
                m_kdtree->add_shape(shape);
#endif
            uint32_t type = shape->shape_type();
            m_shape_types |= type;
        }
    }
#if !defined(MI_ENABLE_EMBREE)
    if constexpr (!dr::is_cuda_v<Float> && !dr::is_metal_v<Float>) {
        if (!m_kdtree->ready())
            m_kdtree->build();

        m_bbox = m_kdtree->bbox();
    }
#endif

#if defined(MI_ENABLE_LLVM) || defined(MI_ENABLE_METAL)
    if constexpr (dr::is_llvm_v<Float> || dr::is_metal_v<Float>) {
        // Get shapes registry ids
        std::unique_ptr<uint32_t[]> data(new uint32_t[m_shapes.size()]);
        for (size_t i = 0; i < m_shapes.size(); i++)
            data[i] = jit_registry_id(m_shapes[i]);
        m_shapes_registry_ids =
            dr::load<DynamicBuffer<UInt32>>(data.get(), m_shapes.size());
    }
#endif

    // Initialize gradient enabled cache
    m_parameters_grad_enabled_cache = false;
    for (auto s : m_shapes) {
        if (s->parameters_grad_enabled()) {
            m_parameters_grad_enabled_cache = true;
            break;
        }
    }
    m_parameters_grad_enabled_dirty = false;
}

MI_VARIANT ShapeGroup<Float, Spectrum>::~ShapeGroup() { }

MI_VARIANT void ShapeGroup<Float, Spectrum>::traverse(TraversalCallback *cb) {
    for (auto s : m_shapes) {
        std::string_view id = s->id();
        if (id.empty() || string::starts_with(id, "_unnamed_"))
            cb->put("shape", s, ParamFlags::Differentiable);
        else
            cb->put(std::string(id), s, ParamFlags::Differentiable);
    }
}

MI_VARIANT void ShapeGroup<Float, Spectrum>::parameters_changed(const std::vector<std::string> &/*keys*/) {
    for (auto &s : m_shapes) {
        if (s->dirty()) {
            m_dirty = true;
            break;
        }
    }

    // Mark gradient cache as dirty since parameters may have changed
    m_parameters_grad_enabled_dirty = true;

    Base::parameters_changed();
}


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

    // OptiX and Metal recover the hit child shape directly: ``pi.shape`` is set
    // per-geometry (from the SBT record / the Metal geom_shape table), so it
    // already names the actual child. The scalar and LLVM/Embree backends instead
    // resolve it from a within-group leaf index (``pi.shape_index``).
    if constexpr (!dr::is_cuda_v<Float> && !dr::is_metal_v<Float>) {
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
    if constexpr (!dr::is_cuda_v<Float> && !dr::is_metal_v<Float>)
        return m_kdtree->primitive_count();
#endif

    ScalarSize count = 0;
    for (auto shape : m_shapes)
        count += shape->primitive_count();

    return count;
}

#if !defined(MI_ENABLE_EMBREE)
MI_VARIANT
std::tuple<bool,
           typename ShapeGroup<Float, Spectrum>::ScalarFloat,
           typename ShapeGroup<Float, Spectrum>::ScalarPoint2f,
           typename ShapeGroup<Float, Spectrum>::ScalarUInt32,
           typename ShapeGroup<Float, Spectrum>::ScalarUInt32>
ShapeGroup<Float, Spectrum>::ray_intersect_preliminary_scalar(const ScalarRay3f &ray) const {
    auto pi = m_kdtree->template ray_intersect_scalar<false>(ray);
    return { pi.valid, pi.t, pi.prim_uv, pi.shape_index, pi.prim_index };
}

MI_VARIANT
bool ShapeGroup<Float, Spectrum>::ray_test_scalar(const ScalarRay3f &ray) const {
    return m_kdtree->template ray_intersect_scalar<true>(ray).is_valid();
}
#endif

MI_VARIANT bool ShapeGroup<Float, Spectrum>::parameters_grad_enabled() const {
    // Recompute cache if dirty
    if (m_parameters_grad_enabled_dirty) {
        m_parameters_grad_enabled_cache = false;
        for (auto s : m_shapes) {
            if (s->parameters_grad_enabled()) {
                m_parameters_grad_enabled_cache = true;
                break;
            }
        }
        m_parameters_grad_enabled_dirty = false;
    }
    return m_parameters_grad_enabled_cache;
}

MI_VARIANT std::string ShapeGroup<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
        oss << "ShapeGroup[" << std::endl
            << "  name = \"" << this->id() << "\"," << std::endl
            << "  prim_count = " << primitive_count() << std::endl
            << "]";
    return oss.str();
}

MI_IMPLEMENT_TRAVERSE_CB(ShapeGroup, Base)
MI_INSTANTIATE_CLASS(ShapeGroup)
NAMESPACE_END(mitsuba)
