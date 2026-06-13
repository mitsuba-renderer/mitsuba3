#include <mitsuba/render/scene.h>
#include <mitsuba/render/accel_cpu_common.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum, bool ShadowRay, size_t Width>
void kdtree_trace_func_wrapper(const int *valid, void *ptr,
                               void* /* context */, uint8_t *args);

// -----------------------------------------------------------------------
//  NativeAccel<Float, Spectrum> -- lifecycle
// -----------------------------------------------------------------------

template <typename Float, typename Spectrum>
void NativeAccel<Float, Spectrum>::init(Scene<Float, Spectrum> *scene,
                                        const Properties &props) {
    accel = new ShapeKDTree<Float, Spectrum>(props);
    accel->inc_ref();

    if constexpr (dr::is_llvm_v<Float>)
        shapes_registry_ids = build_registry_ids<Float, Spectrum>(scene->m_shapes);

    parameters_changed(scene);
}

template <typename Float, typename Spectrum>
void NativeAccel<Float, Spectrum>::parameters_changed(
    Scene<Float, Spectrum> *scene) {
    // Ensure all ray tracing kernels are terminated before rebuilding
    if constexpr (dr::is_llvm_v<Float>)
        dr::sync_thread();

    accel->clear();
    for (Shape *shape : scene->m_shapes)
        accel->add_shape(shape);
    ScopedPhase phase(ProfilerPhase::InitAccel);
    accel->build();

    /* Set up a callback on the handle variable to release the acceleration
       data structure (AS) when this variable is freed. This ensures that the
       lifetime of the AS goes beyond the one of the Scene instance if there
       are still some pending ray tracing calls. The by-value NativeAccel dies
       with the Scene, so the callback owns only the native kd-tree it captures
       by pointer. */
    if constexpr (dr::is_llvm_v<Float>) {
        /* Swap the handle to the (rebuilt) kd-tree. The callback frees the
           native kd-tree lazily so it outlives the Scene while ray-tracing
           kernels are still pending (e.g. recorded by a frozen function). */
        reset_mapped_handle(
            accel_handle, (void *) accel,
            [](uint32_t /* index */, int free, void *payload) {
                if (free)
                    jit_enqueue_host_func(
                        JitBackend::LLVM,
                        [](void *p) {
                            Log(Debug, "Free KDTree..");
                            auto *kdtree = (ShapeKDTree<Float, Spectrum> *) p;
                            kdtree->clear();
                            kdtree->dec_ref();
                        },
                        payload);
            },
            (void *) accel);

        // To support frozen functions the func_ptr has to exist as a variable
        // when the scene is traversed.
        // Since the LLVM vector width should not change over the lifetime of
        // the scene, we determine the intersection function here.
        int jit_width  = jit_llvm_vector_width();
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

        func_handle = UInt64::map_(func_ptr, 1, false);
    }

    scene->clear_shapes_dirty();
}

template <typename Float, typename Spectrum>
void NativeAccel<Float, Spectrum>::release() {
    if (!accel)
        return;
    if constexpr (dr::is_llvm_v<Float>) {
        // Ensure all ray tracing kernels are terminated before releasing
        dr::sync_thread();

        /* Drop the reference count of the handle variable, triggering the
           deferred release of the kd-tree once no ray tracing calls remain. */
        accel_handle = 0;
    } else {
        accel->dec_ref();
    }
    accel = nullptr;
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

    const ShapeKDTree *kdtree = (const ShapeKDTree *) ptr;
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

// -----------------------------------------------------------------------
//  NativeAccel<Float, Spectrum> -- ray queries
// -----------------------------------------------------------------------

template <typename Float, typename Spectrum>
typename NativeAccel<Float, Spectrum>::PreliminaryIntersection3f
NativeAccel<Float, Spectrum>::ray_intersect_preliminary(
    const Scene<Float, Spectrum> * /*scene*/, const Ray3f &ray, Mask coherent,
    bool /*reorder*/, UInt32 /*reorder_hint*/, uint32_t /*reorder_hint_bits*/,
    Mask active) const {
    if constexpr (!dr::is_array_v<Float>) {
        DRJIT_MARK_USED(coherent);
        return accel->template ray_intersect_preliminary<false>(ray, active);
    } else {
        void *fptr      = func_ptr,
             *scene_ptr = (void *) accel;

        UInt64 func_v = UInt64::steal(
                   jit_var_pointer(JitBackend::LLVM, fptr, func_handle.index(), 0)),
               scene_v = UInt64::steal(
                   jit_var_pointer(JitBackend::LLVM, scene_ptr, accel_handle.index(), 0));

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

        ShapePtr shape = dr::gather<UInt32>(shapes_registry_ids, index, hit);

        pi.instance = shape & hit_inst;
        pi.shape    = shape & !hit_inst;

        return pi;
    }
}

template <typename Float, typename Spectrum>
typename NativeAccel<Float, Spectrum>::SurfaceInteraction3f
NativeAccel<Float, Spectrum>::ray_intersect(
    const Scene<Float, Spectrum> *scene, const Ray3f &ray, uint32_t ray_flags,
    Mask coherent, bool reorder, UInt32 reorder_hint,
    uint32_t reorder_hint_bits, Mask active) const {
    PreliminaryIntersection3f pi = ray_intersect_preliminary(
        scene, ray, coherent, reorder, reorder_hint, reorder_hint_bits, active);
    return pi.compute_surface_interaction(ray, ray_flags, active);
}

template <typename Float, typename Spectrum>
typename NativeAccel<Float, Spectrum>::Mask
NativeAccel<Float, Spectrum>::ray_test(const Scene<Float, Spectrum> * /*scene*/,
                                       const Ray3f &ray, Mask coherent,
                                       Mask active) const {
    if constexpr (!dr::is_jit_v<Float>) {
        DRJIT_MARK_USED(coherent);
        return accel->template ray_intersect_preliminary<true>(ray, active).is_valid();
    } else {
        void *fptr      = func_ptr,
             *scene_ptr = (void *) accel;

        UInt64 func_v = UInt64::steal(
                   jit_var_pointer(JitBackend::LLVM, fptr, func_handle.index(), 0)),
               scene_v = UInt64::steal(
                   jit_var_pointer(JitBackend::LLVM, scene_ptr, accel_handle.index(), 0));

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

template <typename Float, typename Spectrum>
typename NativeAccel<Float, Spectrum>::SurfaceInteraction3f
NativeAccel<Float, Spectrum>::ray_intersect_naive(
    const Scene<Float, Spectrum> * /*scene*/, const Ray3f &ray,
    Mask active) const {
    PreliminaryIntersection3f pi =
        accel->template ray_intersect_naive<false>(ray, active);

    return pi.compute_surface_interaction(ray, +RayFlags::All, active);
}

NAMESPACE_END(mitsuba)
