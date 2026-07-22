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
                   occlude_handle, shapes_registry_ids,
                   batch_element_ids)

    /// Native Embree scene, lifetime tied to ``accel_handle`` in JIT variants.
    RTCSceneTy *accel = nullptr;
    std::vector<int> geometries;
    bool is_nested_scene = false;
    /// One nested Embree scene per ShapeGroup, shared by its Instances.
    tsl::robin_map<const void *, RTCSceneTy *, PointerHasher> group_scenes;
    /// Width-specialized Embree entry points.
    void *func_ptr = nullptr;
    void *occlude_func_ptr = nullptr;

    /// Freeze-visible handles and shape recovery table.
    UInt64 accel_handle;
    UInt64 func_handle;
    UInt64 occlude_handle;
    DynamicBuffer<UInt32> shapes_registry_ids;

    /// LLVM path: maps a top-level Embree geom_id to its batch element index
    /// within a MergeInstance. Value is (uint32_t) -1 for non-batch shapes.
    DynamicBuffer<UInt32> batch_element_ids;

    /// Scalar path: maps a top-level Embree geom_id to its owning index in
    /// \c scene->m_shapes. A MergeInstance shape emits one geometry per batch
    /// element, so several consecutive geom_ids can map to the same entry.
    std::vector<uint32_t> geom_id_to_shape_idx;

    /// Scalar path: maps a top-level Embree geom_id to its batch element
    /// index within a MergeInstance. Value is (uint32_t) -1 for non-batch
    /// shapes. Host-side counterpart of \c batch_element_ids.
    std::vector<uint32_t> geom_id_to_batch_idx;
};

NAMESPACE_END(mitsuba)

#endif // MI_ENABLE_EMBREE
