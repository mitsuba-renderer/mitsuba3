NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
struct NativeState {
    MI_IMPORT_CORE_TYPES()
    ShapeKDTree<Float, Spectrum> *accel;
    DynamicBuffer<UInt32> shapes_registry_ids;
};

MI_VARIANT void Scene<Float, Spectrum>::accel_init_cpu(const Properties &props) {
    ShapeKDTree *kdtree = new ShapeKDTree(props);
    kdtree->inc_ref();

    if constexpr (dr::is_llvm_v<Float>) {
        m_accel = new NativeState<Float, Spectrum>();
        NativeState<Float, Spectrum> &s = *(NativeState<Float, Spectrum> *) m_accel;
        s.accel = kdtree;

        // Get shapes registry ids
        if (!m_shapes.empty()) {
            std::unique_ptr<uint32_t[]> data(new uint32_t[m_shapes.size()]);
            for (size_t i = 0; i < m_shapes.size(); i++)
                data[i] = jit_registry_id(m_shapes[i]);
            s.shapes_registry_ids
                = dr::load<DynamicBuffer<UInt32>>(data.get(), m_shapes.size());
        } else {
            s.shapes_registry_ids = dr::zeros<DynamicBuffer<UInt32>>();
        }
    } else {
        m_accel = kdtree;
    }

    accel_parameters_changed_cpu();
}

MI_VARIANT void Scene<Float, Spectrum>::accel_parameters_changed_cpu() {
    // Ensure all ray tracing kernels are terminated before releasing the scene
    if constexpr (dr::is_llvm_v<Float>)
        dr::sync_thread();

    ShapeKDTree *kdtree;
    if constexpr (dr::is_llvm_v<Float>)
        kdtree = ((NativeState<Float, Spectrum> *) m_accel)->accel;
    else
        kdtree = (ShapeKDTree *) m_accel;

    kdtree->clear();
    for (Shape *shape : m_shapes)
        kdtree->add_shape(shape);
    ScopedPhase phase(ProfilerPhase::InitAccel);
    kdtree->build();

    /* Set up a callback on the handle variable to release the Embree
       acceleration data structure (IAS) when this variable is freed. This
       ensures that the lifetime of the IAS goes beyond the one of the Scene
       instance if there are still some pending ray tracing calls (e.g. non
       evaluated variables depending on a ray tracing call). */
    if constexpr (dr::is_llvm_v<Float>) {
        // Prevents the IAS to be released when updating the scene parameters
        if (m_accel_handle.index())
            jit_var_set_callback(m_accel_handle.index(), nullptr, nullptr);
        m_accel_handle = dr::opaque<UInt64>(kdtree);
        jit_var_set_callback(
            m_accel_handle.index(),
            [](uint32_t /* index */, int free, void *payload) {
                if (free) {
                    // Free KDTree on another thread to avoid deadlock with Dr.Jit mutex
                    Task *task = dr::do_async([payload](){
                        Log(Debug, "Free KDTree..");
                        NativeState<Float, Spectrum> *s =
                            (NativeState<Float, Spectrum> *) payload;
                        s->accel->clear();
                        s->accel->dec_ref();
                        delete s;
                    });
                    Thread::register_task(task);
                }
            },
            (void *) m_accel
        );
    }

    clear_shapes_dirty();
}

MI_VARIANT void Scene<Float, Spectrum>::accel_release_cpu() {
    if constexpr (dr::is_llvm_v<Float>) {
        // Ensure all ray tracing kernels are terminated before releasing the scene
        dr::sync_thread();

        /* Decrease the reference count of the handle variable. This will
           trigger the release of the Embree acceleration data structure if no
           ray tracing calls are pending. */
        m_accel_handle = 0;
    } else {
        ((ShapeKDTree *) m_accel)->dec_ref();
    }

    m_accel = nullptr;
}

#if defined(_MSC_VER)
#  pragma pack(push, 1)
#endif

template <typename ScalarFloat> struct RayHitT {
    ScalarFloat o_x, o_y, o_z, tnear;
    ScalarFloat d_x, d_y, d_z, time;
    ScalarFloat tfar;
    uint32_t mask, id, flags;
    ScalarFloat ng_x, ng_y, ng_z, u, v;
    uint32_t prim_id, geom_id, inst_id;
} DRJIT_PACK;

#if defined(_MSC_VER)
#  pragma pack(pop)
#endif

template <typename Float, typename Spectrum, bool ShadowRay, size_t Width>
void kdtree_trace_func_wrapper(const int *valid, void *ptr,
                               void* /* context */, uint8_t *args) {
    MI_IMPORT_TYPES()
    using ScalarRay3f = Ray<ScalarPoint3f, Spectrum>;
    using ShapeKDTree = ShapeKDTree<Float, Spectrum>;

    NativeState<Float, Spectrum> *s = (NativeState<Float, Spectrum> *) ptr;
    const ShapeKDTree *kdtree = s->accel;
    using RayHit = RayHitT<ScalarFloat>;

    for (size_t i = 0; i < Width; i++) {
        if (valid[i] == 0)
            continue;

        ScalarPoint3f ray_o;
        ray_o[0] = ((ScalarFloat*) &args[offsetof(RayHit, o_x) * Width])[i];
        ray_o[1] = ((ScalarFloat*) &args[offsetof(RayHit, o_y) * Width])[i];
        ray_o[2] = ((ScalarFloat*) &args[offsetof(RayHit, o_z) * Width])[i];

        ScalarVector3f ray_d;
        ray_d[0] = ((ScalarFloat*) &args[offsetof(RayHit, d_x) * Width])[i];
        ray_d[1] = ((ScalarFloat*) &args[offsetof(RayHit, d_y) * Width])[i];
        ray_d[2] = ((ScalarFloat*) &args[offsetof(RayHit, d_z) * Width])[i];

        ScalarFloat& ray_maxt = ((ScalarFloat*) &args[offsetof(RayHit, tfar) * Width])[i];
        ScalarFloat& ray_time = ((ScalarFloat*) &args[offsetof(RayHit, time) * Width])[i];

        ScalarRay3f ray = ScalarRay3f(ray_o, ray_d, ray_maxt, ray_time, wavelength_t<Spectrum>());

        if constexpr (ShadowRay) {
            bool hit = kdtree->template ray_intersect_scalar<true>(ray).is_valid();
            if (hit)
                ray_maxt = 0.f;
        } else {
            auto pi = kdtree->template ray_intersect_scalar<false>(ray);
            if (pi.is_valid()) {
                ScalarFloat& prim_u = ((ScalarFloat*) &args[offsetof(RayHit, u) * Width])[i];
                ScalarFloat& prim_v = ((ScalarFloat*) &args[offsetof(RayHit, v) * Width])[i];
                uint32_t& prim_id = ((uint32_t*) &args[offsetof(RayHit, prim_id) * Width])[i];
                uint32_t& geom_id = ((uint32_t*) &args[offsetof(RayHit, geom_id) * Width])[i];
                uint32_t& inst_id = ((uint32_t*) &args[offsetof(RayHit, inst_id) * Width])[i];

                // Write outputs
                ray_maxt  = pi.t;
                prim_u = pi.prim_uv[0];
                prim_v = pi.prim_uv[1];
                prim_id = pi.prim_index;
                geom_id = pi.shape_index;
                inst_id = pi.instance ? (uint32_t) (size_t) pi.shape // shape_index
                                      : (uint32_t) -1;
            }
        }
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_cpu(const Ray3f &ray,
                                                      Mask coherent,
                                                      Mask active) const {
    if constexpr (!dr::is_array_v<Float>) {
        DRJIT_MARK_USED(coherent);
        const ShapeKDTree *kdtree = (const ShapeKDTree *) m_accel;
        return kdtree->template ray_intersect_preliminary<false>(ray, active);
    } else {
        NativeState<Float, Spectrum> *s = (NativeState<Float, Spectrum> *) m_accel;
        void *func_ptr = nullptr,
             *scene_ptr = m_accel;

        int jit_width = jit_llvm_vector_width();
        switch (jit_width) {
            case 1:  func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, false, 1>; break;
            case 4:  func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, false, 4>; break;
            case 8:  func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, false, 8>; break;
            case 16: func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, false, 16>; break;
            default:
                Throw("ray_intersect_preliminary_cpu(): Dr.Jit is "
                      "configured for vectors of width %u, which is not "
                      "supported by the kd-tree ray tracing backend!", jit_width);
        }

        UInt64 func_v = UInt64::steal(
                   jit_var_pointer(JitBackend::LLVM, func_ptr, m_accel_handle.index(), 0)),
               scene_v = UInt64::steal(
                   jit_var_pointer(JitBackend::LLVM, scene_ptr, 0, 0));

        UInt32 zero = dr::zeros<UInt32>();
        Float ray_mint = dr::zeros<Float>();

        uint32_t in[14] = { coherent.index(),  active.index(),
                            ray.o.x().index(), ray.o.y().index(),
                            ray.o.z().index(), ray_mint.index(),
                            ray.d.x().index(), ray.d.y().index(),
                            ray.d.z().index(), ray.time.index(),
                            ray.maxt.index(),  zero.index(),
                            zero.index(),      zero.index() };
        uint32_t out[6] { };

        jit_llvm_ray_trace(func_v.index(), scene_v.index(), 0, in, out);

        PreliminaryIntersection3f pi;

        Float t(Float::steal(out[0]));

        pi.prim_uv = Vector2f(Float::steal(out[1]),
                              Float::steal(out[2]));

        pi.prim_index  = UInt32::steal(out[3]);
        pi.shape_index = UInt32::steal(out[4]);

        UInt32 inst_index = UInt32::steal(out[5]);

        Mask hit = active && (t != ray.maxt);

        pi.t = dr::select(hit, t, dr::Infinity<Float>);

        // Set si.instance and si.shape
        Mask hit_inst = hit && (inst_index != ((uint32_t)-1));
        UInt32 index = dr::select(hit_inst, inst_index, pi.shape_index);

        ShapePtr shape = dr::gather<UInt32>(s->shapes_registry_ids, index, hit);

        pi.instance = shape & hit_inst;
        pi.shape    = shape & !hit_inst;

        return pi;
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_cpu(const Ray3f &ray, uint32_t ray_flags,
                                          Mask coherent, Mask active) const {
    if constexpr (!dr::is_cuda_v<Float>) {
        PreliminaryIntersection3f pi =
            ray_intersect_preliminary_cpu(ray, coherent, active);
        return pi.compute_surface_interaction(ray, ray_flags, active);
    } else {
        DRJIT_MARK_USED(ray);
        DRJIT_MARK_USED(ray_flags);
        DRJIT_MARK_USED(coherent);
        DRJIT_MARK_USED(active);
        Throw("ray_intersect_cpu() should only be called in CPU mode.");
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_cpu(const Ray3f &ray,
                                     Mask coherent, Mask active) const {
    if constexpr (!dr::is_jit_v<Float>) {
        DRJIT_MARK_USED(coherent);
        const ShapeKDTree *kdtree = (const ShapeKDTree *) m_accel;
        return kdtree->template ray_intersect_preliminary<true>(ray, active).is_valid();
    } else {
        void *func_ptr = nullptr, *scene_ptr = m_accel;

        int jit_width = jit_llvm_vector_width();
        switch (jit_width) {
            case 1:  func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, true, 1>; break;
            case 4:  func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, true, 4>; break;
            case 8:  func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, true, 8>; break;
            case 16: func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, true, 16>; break;
            default:
                Throw("ray_test_cpu(): Dr.Jit is configured for vectors of "
                      "width %u, which is not supported by the kd-tree ray "
                      "tracing backend!", jit_width);
        }

        UInt64 func_v = UInt64::steal(
                   jit_var_pointer(JitBackend::LLVM, func_ptr, m_accel_handle.index(), 0)),
               scene_v = UInt64::steal(
                   jit_var_pointer(JitBackend::LLVM, scene_ptr, 0, 0));

        UInt32 zero = dr::zeros<UInt32>();
        Float ray_mint = dr::zeros<Float>();

        uint32_t in[14] = { coherent.index(),  active.index(),
                            ray.o.x().index(), ray.o.y().index(),
                            ray.o.z().index(), ray_mint.index(),
                            ray.d.x().index(), ray.d.y().index(),
                            ray.d.z().index(), ray.time.index(),
                            ray.maxt.index(),  zero.index(),
                            zero.index(),      zero.index() };
        uint32_t out[1] { };

        jit_llvm_ray_trace(func_v.index(), scene_v.index(), 1, in, out);

        return active && (Float::steal(out[0]) != ray.maxt);
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_naive_cpu(const Ray3f &ray, Mask active) const {
    const ShapeKDTree *kdtree;
    if constexpr (dr::is_llvm_v<Float>)
        kdtree = ((NativeState<Float, Spectrum> *) m_accel)->accel;
    else
        kdtree = (const ShapeKDTree *) m_accel;

    PreliminaryIntersection3f pi =
        kdtree->template ray_intersect_naive<false>(ray, active);

    return pi.compute_surface_interaction(ray, +RayFlags::All, active);
}

NAMESPACE_END(mitsuba)
