#include <mitsuba/render/scene.h>
#include "accel_cpu_common.h"

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

    rebuild(scene);
}

template <typename Float, typename Spectrum>
void NativeAccel<Float, Spectrum>::rebuild(
    Scene<Float, Spectrum> *scene) {
    // Ensure all ray tracing kernels are terminated before rebuilding
    if constexpr (dr::is_llvm_v<Float>)
        dr::sync_thread();

    accel->clear();
    for (Shape *shape : scene->m_shapes)
        accel->add_shape(shape);
    ScopedPhase phase(ProfilerPhase::InitAccel);
    accel->build();

    // The kd-tree is rebuilt in place, so initialize the handles only once.
    // The cleanup callback keeps it alive until pending ray-tracing kernels
    // finish.
    if constexpr (dr::is_llvm_v<Float>) {
        if (!accel_handle.index()) {
            init_mapped_handle(
                accel_handle, (void *) accel,
                [](uint32_t /* index */, int free, void *payload) {
                    if (free)
                        jit_enqueue_host_func(
                            JitBackend::LLVM,
                            [](void *p) {
                                auto *kdtree = (ShapeKDTree<Float, Spectrum> *) p;
                                kdtree->clear();
                                kdtree->dec_ref();
                            },
                            payload);
                },
                (void *) accel);

            // The LLVM vector width is fixed over the scene's lifetime.
            int jit_width  = jit_llvm_vector_width();
            switch (jit_width) {
                case 1:  func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, false, 1>;  occlude_func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, true, 1>;  break;
                case 4:  func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, false, 4>;  occlude_func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, true, 4>;  break;
                case 8:  func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, false, 8>;  occlude_func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, true, 8>;  break;
                case 16: func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, false, 16>; occlude_func_ptr = (void *) kdtree_trace_func_wrapper<Float, Spectrum, true, 16>; break;
                default:
                    Throw("NativeAccel::rebuild(): Dr.Jit is configured for "
                          "vectors of width %u, which is not supported by the "
                          "kd-tree ray tracing backend!", jit_width);
            }

            map_func_handles(func_handle, occlude_handle, func_ptr,
                             occlude_func_ptr);
        }
    }
}

template <typename Float, typename Spectrum>
void NativeAccel<Float, Spectrum>::release() {
    if (!accel)
        return;
    if constexpr (dr::is_llvm_v<Float>) {
        // Ensure all ray tracing kernels are terminated before releasing
        dr::sync_thread();

        // Drop the handle reference. Its callback releases the kd-tree once no
        // ray tracing calls remain.
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
                ray_maxt = -dr::Infinity<ScalarFloat>;
        } else {
            auto pi = kdtree->template ray_intersect_scalar<false>(ray);
            if (pi.is_valid()) {
                ScalarFloat& prim_u = ((ScalarFloat*) &args[offsetof(RayHit, u) * Width])[i];
                ScalarFloat& prim_v = ((ScalarFloat*) &args[offsetof(RayHit, v) * Width])[i];
                uint32_t& prim_id = ((uint32_t*) &args[offsetof(RayHit, prim_id) * Width])[i];
                uint32_t& geom_id = ((uint32_t*) &args[offsetof(RayHit, geom_id) * Width])[i];
                uint32_t& inst_id = ((uint32_t*) &args[offsetof(RayHit, inst_id) * Width])[i];

                ray_maxt  = pi.t;
                prim_u = pi.prim_uv[0];
                prim_v = pi.prim_uv[1];
                prim_id = pi.prim_index;
                geom_id = pi.shape_index;
                inst_id = pi.instance ? (uint32_t) (size_t) pi.shape
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
        dr::Array<Float, 3> ray_o(ray.o), ray_d(ray.d);

        uint32_t out[8] { };
        cpu_llvm_ray_trace<Float>((void *) func_ptr, func_handle.index(),
                                  (void *) accel, accel_handle.index(), ray_o,
                                  ray_d, ray.time, ray.maxt, coherent, active,
                                  0, out);

        // The kd-tree traces in ``Float`` precision, so the hit fields are
        // stolen at that width.
        return decode_cpu_llvm_pi<Float, Spectrum, Float>(out,
                                                          shapes_registry_ids);
    }
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
        dr::Array<Float, 3> ray_o(ray.o), ray_d(ray.d);

        // Shadow ray: trace against the any-hit wrapper, which stops at the
        // first hit and returns a boolean hit mask.
        uint32_t out[1] { };
        cpu_llvm_ray_trace<Float>((void *) occlude_func_ptr,
                                  occlude_handle.index(), (void *) accel,
                                  accel_handle.index(), ray_o, ray_d, ray.time,
                                  ray.maxt, coherent, active, 1, out);

        return Mask::steal(out[0]);
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
