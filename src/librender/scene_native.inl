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

    m_accel = new NativeState<Float, Spectrum>();
    NativeState<Float, Spectrum> &s = *(NativeState<Float, Spectrum> *) m_accel;
    s.accel = kdtree;

    if constexpr (ek::is_llvm_array_v<Float>) {
        // Get shapes registry ids
        std::unique_ptr<uint32_t[]> data(new uint32_t[m_shapes.size()]);
        for (size_t i = 0; i < m_shapes.size(); i++)
            data[i] = jit_registry_get_id(JitBackend::LLVM, m_shapes[i]);
        s.shapes_registry_ids
            = ek::load<DynamicBuffer<UInt32>>(data.get(), m_shapes.size());
    }
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_release_cpu() {
    NativeState<Float, Spectrum> *s = (NativeState<Float, Spectrum> *) m_accel;
    s->accel->dec_ref();
    delete s;
    m_accel = nullptr;
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_parameters_changed_cpu() {
    Throw("accel_parameters_changed_cpu() not supported with Mitsuba native "
          "kdtree. Embree should be used instead.");
}

template <typename Float, typename Spectrum, bool ShadowRay>
void kdtree_embree_func_wrapper(const int* /*valid*/, void* ptr, void* /*context*/, void** args) {
    MTS_IMPORT_TYPES()
    using ScalarRay3f = Ray<ScalarPoint3f, Spectrum>;
    using ShapeKDTree = ShapeKDTree<Float, Spectrum>;

    const ShapeKDTree *kdtree = (const ShapeKDTree *) ptr;

    uint32_t width = jit_llvm_vector_width();
    for (size_t i = 0; i < width; i++) {
        ScalarPoint3f ray_o  = { ((ScalarFloat *) args[1])[i],
                                 ((ScalarFloat *) args[2])[i],
                                 ((ScalarFloat *) args[3])[i] };
        ScalarVector3f ray_d = { ((ScalarFloat *) args[5])[i],
                                 ((ScalarFloat *) args[6])[i],
                                 ((ScalarFloat *) args[7])[i] };

        ScalarRay3f ray = ScalarRay3f(
            ray_o, ray_d,
            ((ScalarFloat*) args[4])[i], // mint
            ((ScalarFloat*) args[9])[i], // maxt
            ((ScalarFloat*) args[8])[i], // time
            wavelength_t<Spectrum>()
        );

        bool active = ((bool*) args[0])[i]; // TODO use valid?
        if (!active)
            continue;

        if constexpr (ShadowRay) {
            bool hit = kdtree->template ray_intersect_scalar<true>(ray).is_valid();
            if (hit)
                ((ScalarFloat*) args[9])[i]  = 0.f;
        } else {
            auto pi = kdtree->template ray_intersect_scalar<false>(ray);
            // Write outputs
            ((ScalarFloat*) args[9])[i]  = pi.t;
            ((ScalarFloat*) args[16])[i] = pi.prim_uv[0];
            ((ScalarFloat*) args[17])[i] = pi.prim_uv[1];
            ((ScalarUInt32*) args[18])[i] = pi.prim_index;
            ((ScalarUInt32*) args[19])[i] = pi.shape_index;
            ((ScalarUInt32*) args[20])[i] = ((unsigned int)-1); // TODO
        }
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_cpu(const Ray3f &ray, uint32_t, Mask active) const {
    NativeState<Float, Spectrum> *s = (NativeState<Float, Spectrum> *) m_accel;
    const ShapeKDTree *kdtree = s->accel;

    if constexpr (!ek::is_jit_array_v<Float>) {
        return kdtree->template ray_intersect_preliminary<false>(ray, active);
    } else {
        void *func_ptr  = (void *) kdtree_embree_func_wrapper<Float, Spectrum, false>,
             *ctx_ptr   = nullptr,
             *scene_ptr = m_accel;

        UInt64 func_v = UInt64::steal(
                   jit_var_new_pointer(JitBackend::LLVM, func_ptr, 0, 0)),
               ctx_v = UInt64::steal(
                   jit_var_new_pointer(JitBackend::LLVM, ctx_ptr, 0, 0)),
               scene_v = UInt64::steal(
                   jit_var_new_pointer(JitBackend::LLVM, scene_ptr, 0, 0));

        Int32 valid = ek::select(active, (int32_t) -1, 0);
        UInt32 zero = ek::zero<UInt32>();

        using Single = ek::float32_array_t<Float>;
        ek::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_mint(ray.mint), ray_maxt(ray.maxt), ray_time(ray.time);

        uint32_t in[13] = { valid.index(),      ray_o.x().index(),
                            ray_o.y().index(),  ray_o.z().index(),
                            ray_mint.index(),   ray_d.x().index(),
                            ray_d.y().index(),  ray_d.z().index(),
                            ray_time.index(),   ray_maxt.index(),
                            zero.index(),       zero.index(),
                            zero.index() };

        uint32_t out[6] { };

        jit_embree_trace(func_v.index(), ctx_v.index(),
                         scene_v.index(), 0, in, out);

        PreliminaryIntersection3f pi;

        Float t(Single::steal(out[0]));
        pi.prim_uv = Vector2f(Single::steal(out[1]),
                              Single::steal(out[2]));

        pi.prim_index  = UInt32::steal(out[3]);
        pi.shape_index = UInt32::steal(out[4]);

        UInt32 inst_index = UInt32::steal(out[5]);

        Mask hit = active && ek::neq(t, ray_maxt);

        pi.t = ek::select(hit, t, ek::Infinity<Float>);

        // Set si.instance and si.shape
        Mask hit_inst = hit && ek::neq(inst_index, ((unsigned int)-1));
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
    NativeState<Float, Spectrum> *s = (NativeState<Float, Spectrum> *) m_accel;
    const ShapeKDTree *kdtree = s->accel;

    PreliminaryIntersection3f pi =
        kdtree->template ray_intersect_naive<false>(ray, active);

    return pi.compute_surface_interaction(ray, +HitComputeFlags::All, active);
}

MTS_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_cpu(const Ray3f &ray, uint32_t /*hit_flags*/, Mask active) const {
    NativeState<Float, Spectrum> *s = (NativeState<Float, Spectrum> *) m_accel;
    const ShapeKDTree *kdtree = s->accel;

    if constexpr (!ek::is_jit_array_v<Float>) {
        return kdtree->template ray_intersect_preliminary<true>(ray, active).is_valid();
    } else {
        // TODO
        return true;
    }
}

NAMESPACE_END(mitsuba)
