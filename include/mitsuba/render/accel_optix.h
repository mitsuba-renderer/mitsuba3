/*
    accel_optix.h -- OptiX acceleration backend declarations.
*/

#pragma once

#if defined(MI_ENABLE_CUDA)

#include <mitsuba/render/fwd.h>
#include <drjit/array_traverse.h>

NAMESPACE_BEGIN(mitsuba)

/// Per-scene OptiX state (SBT/GAS/IAS), defined in src/render/scene_optix.inl
struct MiOptixSceneState;

/// Vectorized GPU ray tracing acceleration via CUDA/OptiX
template <typename Float, typename Spectrum>
struct OptixAccel {
    MI_IMPORT_TYPES(Shape, ShapePtr)
    OptixAccel() = default;
    DRJIT_NON_COPYABLE(OptixAccel)

    ~OptixAccel() { release(); }

    // --- Lifecycle (bodies in scene_optix.inl) ---
    void init(Scene<Float, Spectrum> *scene, const Properties &props);
    void rebuild(Scene<Float, Spectrum> *scene);
    void release();

    static void static_initialization() { }
    static void static_shutdown();

    // --- Ray queries (bodies in scene_optix.inl) ---
    PreliminaryIntersection3f ray_intersect_preliminary(
        const Scene<Float, Spectrum> *scene, const Ray3f &ray, Mask coherent,
        bool reorder, UInt32 reorder_hint, uint32_t reorder_hint_bits,
        Mask active) const;
    Mask ray_test(const Scene<Float, Spectrum> *scene, const Ray3f &ray,
                  Mask coherent, Mask active) const;
    /// OptiX exposes no brute-force traversal, so this throws.
    SurfaceInteraction3f ray_intersect_naive(
        const Scene<Float, Spectrum> *scene, const Ray3f &ray,
        Mask active) const;

    // --- Declarative traversal (the IAS handle + the rebindable SBT handle) ---
    DRJIT_TRAVERSE(OptixAccel, accel_handle, sbt_handle)

    /// Heap-allocated native OptiX state.
    MiOptixSceneState *state = nullptr;
    /// Freeze-visible IAS and SBT owner handles.
    UInt64 accel_handle;
    UInt64 sbt_handle;
};

NAMESPACE_END(mitsuba)

#endif // MI_ENABLE_CUDA
