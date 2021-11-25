NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
struct NativeState {
    MTS_IMPORT_CORE_TYPES()
    ShapeKDTree<Float, Spectrum> *accel;
    DynamicBuffer<UInt32> shapes_registry_ids;
};

MTS_VARIANT void Scene<Float, Spectrum>::accel_init_cpu(const Properties &props) {
    ShapeKDTree *kdtree = new ShapeKDTree(props);
    kdtree->inc_ref();
    for (Shape *shape : m_shapes)
        kdtree->add_shape(shape);
    ScopedPhase phase(ProfilerPhase::InitAccel);
    kdtree->build();

    if constexpr (ek::is_llvm_array_v<Float>) {
        m_accel = new NativeState<Float, Spectrum>();
        NativeState<Float, Spectrum> &s = *(NativeState<Float, Spectrum> *) m_accel;
        s.accel = kdtree;

        // Get shapes registry ids
        std::unique_ptr<uint32_t[]> data(new uint32_t[m_shapes.size()]);
        for (size_t i = 0; i < m_shapes.size(); i++)
            data[i] = jit_registry_get_id(JitBackend::LLVM, m_shapes[i]);
        s.shapes_registry_ids
            = ek::load<DynamicBuffer<UInt32>>(data.get(), m_shapes.size());
    } else {
        m_accel = kdtree;
    }
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_release_cpu() {
    if constexpr (ek::is_llvm_array_v<Float>) {
        NativeState<Float, Spectrum> *s = (NativeState<Float, Spectrum> *) m_accel;
        s->accel->dec_ref();
        delete s;
    } else {
        ((ShapeKDTree *) m_accel)->dec_ref();
    }
    m_accel = nullptr;
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_parameters_changed_cpu() {
    ShapeKDTree *kdtree;
    if constexpr (ek::is_llvm_array_v<Float>)
        kdtree = ((NativeState<Float, Spectrum> *) m_accel)->accel;
    else
        kdtree = (ShapeKDTree *) m_accel;

    kdtree->clear();
    for (Shape *shape : m_shapes)
        kdtree->add_shape(shape);
    kdtree->build();
}

template <typename Float, typename Spectrum, bool ShadowRay>
void kdtree_trace_func_wrapper(const int* valid, void* ptr, uint8_t* args) {
    MTS_IMPORT_TYPES()
    using ScalarRay3f = Ray<ScalarPoint3f, Spectrum>;
    using ShapeKDTree = ShapeKDTree<Float, Spectrum>;

    NativeState<Float, Spectrum> *s = (NativeState<Float, Spectrum> *) ptr;
    const ShapeKDTree *kdtree = s->accel;

    struct Args {
        ScalarFloat o_x, o_y, o_z, tnear, d_x, d_y, d_z, time, tfar;
        ScalarUInt32 mask, id, flags;
        ScalarFloat ng_x, ng_y, ng_z, u, v;
        ScalarUInt32 prim_id, geom_id, inst_id;
    }__attribute__((packed));

    uint32_t width = jit_llvm_vector_width();
    for (size_t i = 0; i < width; i++) {
        if (valid[i] == 0)
            continue;

        ScalarPoint3f ray_o;
        ray_o[0] = ((ScalarFloat*) &args[offsetof(Args, o_x) * width])[i];
        ray_o[1] = ((ScalarFloat*) &args[offsetof(Args, o_y) * width])[i];
        ray_o[2] = ((ScalarFloat*) &args[offsetof(Args, o_z) * width])[i];

        ScalarPoint3f ray_d;
        ray_d[0] = ((ScalarFloat*) &args[offsetof(Args, d_x) * width])[i];
        ray_d[1] = ((ScalarFloat*) &args[offsetof(Args, d_y) * width])[i];
        ray_d[2] = ((ScalarFloat*) &args[offsetof(Args, d_z) * width])[i];

        ScalarFloat& ray_mint = ((ScalarFloat*) &args[offsetof(Args, tnear) * width])[i];
        ScalarFloat& ray_maxt = ((ScalarFloat*) &args[offsetof(Args, tfar) * width])[i];
        ScalarFloat& ray_time = ((ScalarFloat*) &args[offsetof(Args, time) * width])[i];

        ScalarRay3f ray = ScalarRay3f(ray_o, ray_d, ray_mint, ray_maxt,
                                      ray_time, wavelength_t<Spectrum>());

        if constexpr (ShadowRay) {
            bool hit = kdtree->template ray_intersect_scalar<true>(ray).is_valid();
            if (hit)
                ray_maxt = 0.f;
        } else {
            auto pi = kdtree->template ray_intersect_scalar<false>(ray);
            if (pi.is_valid()) {
                ScalarFloat& prim_u = ((ScalarFloat*) &args[offsetof(Args, u) * width])[i];
                ScalarFloat& prim_v = ((ScalarFloat*) &args[offsetof(Args, v) * width])[i];
                ScalarUInt32& prim_id = ((ScalarUInt32*) &args[offsetof(Args, prim_id) * width])[i];
                ScalarUInt32& geom_id = ((ScalarUInt32*) &args[offsetof(Args, geom_id) * width])[i];
                ScalarUInt32& inst_id = ((ScalarUInt32*) &args[offsetof(Args, inst_id) * width])[i];

                // Write outputs
                ray_maxt  = pi.t;
                prim_u = pi.prim_uv[0];
                prim_v = pi.prim_uv[1];
                prim_id = pi.prim_index;
                geom_id = pi.shape_index;
                inst_id = pi.instance ? (ScalarUInt32)(size_t) pi.shape // shape_index
                                      : (ScalarUInt32) -1;
            }
        }
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_cpu(const Ray3f &ray, uint32_t, Mask active) const {
    if constexpr (!ek::is_jit_array_v<Float>) {
        const ShapeKDTree *kdtree = (const ShapeKDTree *) m_accel;
        return kdtree->template ray_intersect_preliminary<false>(ray, active);
    } else {
        NativeState<Float, Spectrum> *s = (NativeState<Float, Spectrum> *) m_accel;

        void *func_ptr  = (void *) kdtree_trace_func_wrapper<Float, Spectrum, false>,
             *scene_ptr = m_accel;

        UInt64 func_v = UInt64::steal(
                   jit_var_new_pointer(JitBackend::LLVM, func_ptr, 0, 0)),
               scene_v = UInt64::steal(
                   jit_var_new_pointer(JitBackend::LLVM, scene_ptr, 0, 0));

        Int32 valid = ek::select(active, (int32_t) -1, 0);
        UInt32 zero = ek::zero<UInt32>();
        Float ray_mint = ek::zero<Float>();

        uint32_t in[13] = { valid.index(),      ray.o.x().index(),
                            ray.o.y().index(),  ray.o.z().index(),
                            ray_mint.index(),   ray.d.x().index(),
                            ray.d.y().index(),  ray.d.z().index(),
                            ray.time.index(),   ray.maxt.index(),
                            zero.index(),       zero.index(),
                            zero.index() };
        uint32_t out[6] { };

        jit_llvm_ray_trace(func_v.index(), scene_v.index(), 0, in, out);

        PreliminaryIntersection3f pi;

        Float t(Float::steal(out[0]));

        pi.prim_uv = Vector2f(Float::steal(out[1]), Float::steal(out[2]));

        pi.prim_index  = UInt32::steal(out[3]);
        pi.shape_index = UInt32::steal(out[4]);

        UInt32 inst_index = UInt32::steal(out[5]);

        Mask hit = active && ek::neq(t, ray.maxt);

        pi.t = ek::select(hit, t, ek::Infinity<Float>);

        // Set si.instance and si.shape
        Mask hit_inst = hit && ek::neq(inst_index, ((uint32_t)-1));
        UInt32 index = ek::select(hit_inst, inst_index, pi.shape_index);

        ShapePtr shape = ek::gather<UInt32>(s->shapes_registry_ids, index, hit);

        pi.instance = shape & hit_inst;
        pi.shape    = shape & !hit_inst;

        return pi;
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_cpu(const Ray3f &ray, uint32_t hit_flags, Mask active) const {
    if constexpr (!ek::is_cuda_array_v<Float>) {
        PreliminaryIntersection3f pi = ray_intersect_preliminary_cpu(ray, hit_flags, active);
        return pi.compute_surface_interaction(ray, hit_flags, active);
    } else {
        ENOKI_MARK_USED(ray);
        ENOKI_MARK_USED(hit_flags);
        ENOKI_MARK_USED(active);
        Throw("ray_intersect_cpu() should only be called in CPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_naive_cpu(const Ray3f &ray, Mask active) const {
    const ShapeKDTree *kdtree;
    if constexpr (ek::is_llvm_array_v<Float>)
        kdtree = ((NativeState<Float, Spectrum> *) m_accel)->accel;
    else
        kdtree = (const ShapeKDTree *) m_accel;

    PreliminaryIntersection3f pi =
        kdtree->template ray_intersect_naive<false>(ray, active);

    return pi.compute_surface_interaction(ray, +RayFlags::All, active);
}

MTS_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_cpu(const Ray3f &ray, uint32_t /*hit_flags*/, Mask active) const {
    if constexpr (!ek::is_jit_array_v<Float>) {
        const ShapeKDTree *kdtree = (const ShapeKDTree *) m_accel;
        return kdtree->template ray_intersect_preliminary<true>(ray, active).is_valid();
    } else {
        void *func_ptr  = (void *) kdtree_trace_func_wrapper<Float, Spectrum, true>,
             *scene_ptr = m_accel;

        UInt64 func_v = UInt64::steal(
                   jit_var_new_pointer(JitBackend::LLVM, func_ptr, 0, 0)),
               scene_v = UInt64::steal(
                   jit_var_new_pointer(JitBackend::LLVM, scene_ptr, 0, 0));

        Int32 valid = ek::select(active, (int32_t) -1, 0);
        UInt32 zero = ek::zero<UInt32>();
        Float ray_mint = ek::zero<Float>();

        uint32_t in[13] = { valid.index(),      ray.o.x().index(),
                            ray.o.y().index(),  ray.o.z().index(),
                            ray_mint.index(),   ray.d.x().index(),
                            ray.d.y().index(),  ray.d.z().index(),
                            ray.time.index(),   ray.maxt.index(),
                            zero.index(),       zero.index(),
                            zero.index() };
        uint32_t out[1] { };

        jit_llvm_ray_trace(func_v.index(), scene_v.index(), 1, in, out);

        return active && ek::neq(Float::steal(out[0]), ray.maxt);
    }
}

NAMESPACE_END(mitsuba)
