NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT void Scene<Float, Spectrum>::accel_init_cpu(const Properties &props) {
    ShapeKDTree *kdtree = new ShapeKDTree(props);
    kdtree->inc_ref();
    for (Shape *shape : m_shapes)
        kdtree->add_shape(shape);
    ScopedPhase phase(ProfilerPhase::InitAccel);
    kdtree->build();
    m_accel = kdtree;
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_release_cpu() {
    ((ShapeKDTree *) m_accel)->dec_ref();
    m_accel = nullptr;
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_parameters_changed_cpu() {
    Throw("accel_parameters_changed_cpu() not supported with Mitsuba native "
          "kdtree. Embree should be used instead.");
}

template <typename Float, typename Spectrum, bool ShadowRay>
void kdtree_embree_func_wrapper(const int* /*valid*/, void* ptr, void* /*context*/, void** args) {
    using Scalar = ek::scalar_t<Float>;
    using UInt32 = ek::uint32_array_t<Scalar>;
    using Vector3f = Vector<Scalar, 3>;
    using Point3f = Point<Scalar, 3>;
    using ScalarSpectrum = ek::replace_value_t<Spectrum, Scalar>;
    using Ray3f = Ray<Point3f, ScalarSpectrum>;
    using PreliminaryIntersection3f = PreliminaryIntersection<Scalar, ScalarSpectrum>;
    using ShapeKDTree = ShapeKDTree<Float, Spectrum>;

    const ShapeKDTree *kdtree = (const ShapeKDTree *) ptr;

    uint32_t width = jit_llvm_vector_width();
    for (size_t i = 0; i < width; i++) {
        Point3f  ray_o = { ((Scalar*) args[1])[i], ((Scalar*) args[2])[i], ((Scalar*) args[3])[i] };
        Vector3f ray_d = { ((Scalar*) args[5])[i], ((Scalar*) args[6])[i], ((Scalar*) args[7])[i] };

        Ray3f ray = Ray3f(
            ray_o, ray_d,
            ((Scalar*) args[4])[i], // mint
            ((Scalar*) args[9])[i], // maxt
            ((Scalar*) args[8])[i], // time
            wavelength_t<ScalarSpectrum>()
        );

        bool active = ((bool*) args[0])[i]; // TODO use valid?
        if (!active)
            continue;

        if constexpr (ShadowRay) {
            bool hit = kdtree->template ray_intersect_preliminary<true>(ray, active).is_valid();
            if (hit)
                ((Scalar*) args[9])[i]  = 0.f;
        } else {
            PreliminaryIntersection3f pi =
                kdtree->template ray_intersect_preliminary<false>(ray, active);
            // Write outputs
            ((Scalar*) args[9])[i]  = pi.t;
            ((Scalar*) args[16])[i] = pi.prim_uv[0];
            ((Scalar*) args[17])[i] = pi.prim_uv[1];
            ((UInt32*) args[18])[i] = pi.prim_index;
            ((UInt32*) args[19])[i] = pi.shape_index;
            ((UInt32*) args[20])[i] = ((unsigned int)-1); // TODO
        }
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_cpu(const Ray3f &ray, uint32_t, Mask active) const {
    if constexpr (!ek::is_jit_array_v<Float>) {
        const ShapeKDTree *kdtree = (const ShapeKDTree *) m_accel;
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

        // TODO
        // ShapePtr shape = ek::gather<UInt32>(s.shapes_registry_ids, index, hit);

        // pi.instance = shape & hit_inst;
        // pi.shape    = shape & !hit_inst;

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
    const ShapeKDTree *kdtree = (const ShapeKDTree *) m_accel;

    PreliminaryIntersection3f pi =
        kdtree->template ray_intersect_naive<false>(ray, active);

    return pi.compute_surface_interaction(ray, +HitComputeFlags::All, active);
}

MTS_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_cpu(const Ray3f &ray, uint32_t /*hit_flags*/, Mask active) const {
    const ShapeKDTree *kdtree = (ShapeKDTree *) m_accel;
    return kdtree->template ray_intersect_preliminary<true>(ray, active).is_valid();
}

NAMESPACE_END(mitsuba)
