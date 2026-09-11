/*
    accel_optix.h -- OptiX acceleration backend declarations.
*/

#pragma once

#if defined(MI_ENABLE_CUDA)

#include <mitsuba/render/fwd.h>
#include <drjit/array_traverse.h>

// Hit object field selector of jit_optix_ray_trace(), see drjit-core/optix.h
enum class OptixHitObjectField : uint32_t;

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
        Mask active, UInt32 ray_mask) const;
    ShadowTest<Mask> ray_test(const Scene<Float, Spectrum> *scene,
                              const Ray3f &ray, Mask coherent, Mask active,
                              UInt32 ray_mask,
                              bool skip_null) const;
    static constexpr bool stops_at_first_hit = true;
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

private:
    void trace(const Ray3f &ray, Mask active, UInt32 ray_mask,
               uint32_t ray_flags, uint32_t n_fields,
               const OptixHitObjectField *fields, uint32_t *out,
               bool reorder = false, UInt32 reorder_hint = 0,
               uint32_t reorder_hint_bits = 0) const;
};

NAMESPACE_END(mitsuba)

#endif // MI_ENABLE_CUDA
