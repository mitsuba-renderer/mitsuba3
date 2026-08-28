/*
    accel_embree.h -- Embree acceleration backend declarations.
*/

#pragma once

#if defined(MI_ENABLE_EMBREE)

#include <mitsuba/render/fwd.h>
#include <drjit/array_traverse.h>
#include <drjit-core/hash.h>
#include <tsl/robin_map.h>
#include <vector>

/// Forward-declare Embree's opaque scene type.
struct RTCSceneTy;

NAMESPACE_BEGIN(mitsuba)

/// Vectorized CPU ray tracing acceleration via Embree
template <typename Float, typename Spectrum>
struct EmbreeAccel {
    MI_IMPORT_TYPES(Shape, ShapePtr)
    EmbreeAccel() = default;
    DRJIT_NON_COPYABLE(EmbreeAccel)

    ~EmbreeAccel() { release(); }

    // --- Lifecycle (bodies in scene_embree.inl) ---
    void init(Scene<Float, Spectrum> *scene, const Properties &props);
    void rebuild(Scene<Float, Spectrum> *scene);
    void release();

    static void static_initialization() { }
    static void static_shutdown() { }

    // --- Ray queries (bodies in scene_embree.inl) ---
    PreliminaryIntersection3f ray_intersect_preliminary(
        const Scene<Float, Spectrum> *scene, const Ray3f &ray, Mask coherent,
        bool reorder, UInt32 reorder_hint, uint32_t reorder_hint_bits,
        Mask active) const;
    Mask ray_test(const Scene<Float, Spectrum> *scene, const Ray3f &ray,
                  Mask coherent, Mask active) const;
    /// Embree has no brute-force traversal; defer to the accelerated path.
    SurfaceInteraction3f ray_intersect_naive(
        const Scene<Float, Spectrum> *scene, const Ray3f &ray,
        Mask active) const;

    // --- Declarative traversal ---
    DRJIT_TRAVERSE(EmbreeAccel, accel_handle, func_handle,
                   occlude_handle)

    /// Native Embree scene, lifetime tied to ``accel_handle`` in JIT variants
    RTCSceneTy *accel = nullptr;

    /// Geometry IDs currently attached to ``accel`` (detached on rebuild)
    std::vector<unsigned int> geometries;

    /// Whether this scene was created while another scene renders (see init())
    bool is_nested_scene = false;

    /// One nested Embree scene per ShapeGroup, shared by its Instances
    tsl::robin_map<const void *, RTCSceneTy *, PointerHasher> group_scenes;

    /// Width-specialized Embree closest-hit entry point
    void *func_ptr = nullptr;

    /// Width-specialized Embree any-hit (shadow ray) entry point
    void *occlude_func_ptr = nullptr;

    /// Freeze-visible handle owning ``accel``
    UInt64 accel_handle;

    /// Freeze-visible handle wrapping ``func_ptr``
    UInt64 func_handle;

    /// Freeze-visible handle wrapping ``occlude_func_ptr``
    UInt64 occlude_handle;

    /// Number of instances.
    /// The Embree backends reserve the geometry IDs [0, instance_count) for
    /// instances, so that a hit's ``instID`` can be directly interpreted as the
    /// instance index. Non-instanced geometry follows at ``instance_count``
    /// plus its registry ID (LLVM mode) or sequentially (scalar mode)
    uint32_t instance_count = 0;
};

NAMESPACE_END(mitsuba)

#endif // MI_ENABLE_EMBREE
