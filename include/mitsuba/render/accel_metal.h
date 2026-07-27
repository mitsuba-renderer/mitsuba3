/*
    accel_metal.h -- Metal acceleration backend declarations.
*/

#pragma once

#if defined(MI_ENABLE_METAL)

#include <mitsuba/render/fwd.h>
#include <drjit/array_traverse.h>

NAMESPACE_BEGIN(mitsuba)

/// Opaque handle owning the native Metal objects, see src/render/metal_accel.mm
struct MetalAccelData;

/// Vectorized GPU ray tracing acceleration via Apple Metal
template <typename Float, typename Spectrum>
struct MetalAccel {
    MI_IMPORT_TYPES(Shape, ShapePtr)
    MetalAccel() = default;
    DRJIT_NON_COPYABLE(MetalAccel)

    ~MetalAccel() { release(); }

    // --- Lifecycle (bodies in scene_metal.inl) ---
    void init(Scene<Float, Spectrum> *scene, const Properties &props);
    void rebuild(Scene<Float, Spectrum> *scene);
    void release();

    static void static_initialization() { }
    static void static_shutdown() { }

    // --- Ray queries (bodies in scene_metal.inl) ---
    PreliminaryIntersection3f ray_intersect_preliminary(
        const Scene<Float, Spectrum> *scene, const Ray3f &ray, Mask coherent,
        bool reorder, UInt32 reorder_hint, uint32_t reorder_hint_bits,
        Mask active) const;
    Mask ray_test(const Scene<Float, Spectrum> *scene, const Ray3f &ray,
                  Mask coherent, Mask active) const;
    /// Metal has no brute-force traversal; defer to the accelerated path.
    SurfaceInteraction3f ray_intersect_naive(
        const Scene<Float, Spectrum> *scene, const Ray3f &ray,
        Mask active) const;

    // --- Declarative traversal (scene handle + recovery tables) ---
    DRJIT_TRAVERSE(MetalAccel, accel_handle, geom_shape_offsets,
                   geom_shape_table)

    /// Opaque handle owning the Metal objects (TLAS/BLAS/buffers/library)
    MetalAccelData *accel = nullptr;
    /// Dr.Jit scene id from jit_metal_configure_scene(), 0 for empty scenes
    uint32_t scene_index = 0;
    /// Handle variable representing the Metal scene for @dr.freeze
    UInt64 accel_handle;
    /// Per-instance recovery tables resolving \c pi.shape from a hit's
    /// (instance_id, geometry_id), built in scene_metal.inl.
    DynamicBuffer<UInt32> geom_shape_offsets;
    DynamicBuffer<UInt32> geom_shape_table;

private:
    /// Trace \c ray, writing eight result variable indices to \c out. With
    /// \c shadow, an occlusion query writes only ``out[0]``.
    void trace(const Ray3f &ray, Mask active, uint32_t out[8],
               bool shadow) const;
};

NAMESPACE_END(mitsuba)

#endif // MI_ENABLE_METAL
