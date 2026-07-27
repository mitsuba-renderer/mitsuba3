/*
   accel_native.h -- Builtin kd-tree acceleration backend declarations.
*/

#pragma once

#if !defined(MI_ENABLE_EMBREE)

#include <mitsuba/render/fwd.h>
#include <drjit/array_traverse.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum> class ShapeKDTree;

/// Vectorized CPU ray tracing acceleration via Mitsuba's builtin kd-tree.
template <typename Float, typename Spectrum>
struct NativeAccel {
    MI_IMPORT_TYPES(Shape, ShapePtr)
    NativeAccel() = default;
    DRJIT_NON_COPYABLE(NativeAccel)

    ~NativeAccel() { release(); }

    // --- Lifecycle (bodies in scene_native.inl) ---
    void init(Scene<Float, Spectrum> *scene, const Properties &props);
    void rebuild(Scene<Float, Spectrum> *scene);
    void release();

    static void static_initialization() { }
    static void static_shutdown() { }

    // --- Ray queries (bodies in scene_native.inl) ---
    PreliminaryIntersection3f ray_intersect_preliminary(
        const Scene<Float, Spectrum> *scene, const Ray3f &ray, Mask coherent,
        bool reorder, UInt32 reorder_hint, uint32_t reorder_hint_bits,
        Mask active) const;
    Mask ray_test(const Scene<Float, Spectrum> *scene, const Ray3f &ray,
                  Mask coherent, Mask active) const;
    /// The kd-tree supports a genuine brute-force traversal (used by tests).
    SurfaceInteraction3f ray_intersect_naive(
        const Scene<Float, Spectrum> *scene, const Ray3f &ray,
        Mask active) const;

    // --- Declarative traversal ---
    DRJIT_TRAVERSE(NativeAccel, accel_handle, func_handle,
                   occlude_handle, shapes_registry_ids)

    /// Native kd-tree, lifetime tied to ``accel_handle`` in JIT variants.
    ShapeKDTree<Float, Spectrum> *accel = nullptr;
    /// Width-specialized kd-tree entry points.
    void *func_ptr = nullptr;
    void *occlude_func_ptr = nullptr;

    /// Freeze-visible handles and shape recovery table.
    UInt64 accel_handle;
    UInt64 func_handle;
    UInt64 occlude_handle;
    DynamicBuffer<UInt32> shapes_registry_ids;
};

NAMESPACE_END(mitsuba)

#endif // !MI_ENABLE_EMBREE
