/*
    accel_metal.h -- Typed, by-value Metal acceleration-structure state held
    by Scene.

    This is the header-hygiene-clean half of the Metal backend: it declares the
    JIT-tracked state (lookup tables + opaque native handle) as a traversable
    by-value member of Scene, plus the ray-query / lifecycle entry points whose
    bodies live in src/render/scene_metal.inl.

    Copyright (c) 2026 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE file.
*/

#pragma once

#if defined(MI_ENABLE_METAL)

#include <mitsuba/render/fwd.h>
#include <drjit/array_traverse.h>

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(metal_accel)
/// Opaque handle owning the native Metal objects of a built scene (defined in
/// src/render/metal_accel.mm); only ever referenced through a pointer here.
struct Accel;
NAMESPACE_END(metal_accel)

/**
 * \brief Metal acceleration-structure state (the \c Scene::m_accel member for
 * \c metal_* variants).
 *
 * The scene-owner \c accel_handle and the JIT-tracked lookup tables are
 * traversed (and hence rebindable by frozen functions). The opaque \c accel
 * pointer and \c scene_index live outside the JIT graph: their lifetime is tied
 * to the Dr.Jit scene variable (see metal_accel.mm), so they are intentionally
 * not part of \c fields_().
 */
template <typename Float, typename Spectrum> struct MetalAccel {
    MI_IMPORT_TYPES(Shape, ShapePtr)

    /// Opaque handle owning the Metal objects (TLAS/BLAS/buffers/library)
    metal_accel::Accel *accel = nullptr;
    /// Dr.Jit variable index returned by jit_metal_configure_scene(). Threaded
    /// through jit_metal_ray_trace() to identify *this* scene at codegen +
    /// launch time. Zero for empty scenes.
    uint32_t scene_index = 0;
    /// Handle variable whose data pointer is the MetalScene owner. Traversed as
    /// a frozen-function input so the recorder binds this scene's ray-tracing
    /// resource parameters (Accel/IFT, same owner) to a rebindable input slot
    /// (cross-scene replay) instead of capturing them as constants.
    UInt64 accel_handle;
    /// Per-TLAS-instance offset into ``leaf_idx_table``, or
    /// ``METAL_LEAF_IDX_NONE`` for non-instance (top-level shape) hits. The
    /// "is this an instance hit?" predicate is ``offset != NONE``.
    DynamicBuffer<UInt32> leaf_idx_offsets;
    /// Flat table mapping (leaf_idx_offsets[instance_id] + geometry_id) to the
    /// leaf shape's index within its ShapeGroup. Only consulted for instance
    /// hits.
    DynamicBuffer<UInt32> leaf_idx_table;

    MetalAccel() = default;
    MetalAccel(const MetalAccel &) = delete;
    MetalAccel &operator=(const MetalAccel &) = delete;
    MetalAccel(MetalAccel &&) = delete;
    MetalAccel &operator=(MetalAccel &&) = delete;
    ~MetalAccel() { release(); }

    // --- Lifecycle (bodies in scene_metal.inl) ---
    void init(Scene<Float, Spectrum> *scene, const Properties &props);
    void parameters_changed(Scene<Float, Spectrum> *scene);
    void release();

    static void static_initialization() { }
    static void static_shutdown() { }

    // --- Ray queries (bodies in scene_metal.inl) ---
    PreliminaryIntersection3f ray_intersect_preliminary(
        const Scene<Float, Spectrum> *scene, const Ray3f &ray, Mask coherent,
        bool reorder, UInt32 reorder_hint, uint32_t reorder_hint_bits,
        Mask active) const;
    SurfaceInteraction3f ray_intersect(
        const Scene<Float, Spectrum> *scene, const Ray3f &ray,
        uint32_t ray_flags, Mask coherent, bool reorder, UInt32 reorder_hint,
        uint32_t reorder_hint_bits, Mask active) const;
    Mask ray_test(const Scene<Float, Spectrum> *scene, const Ray3f &ray,
                  Mask coherent, Mask active) const;
    SurfaceInteraction3f ray_intersect_naive(
        const Scene<Float, Spectrum> *scene, const Ray3f &ray,
        Mask active) const;

    // --- Declarative traversal: the scene-owner handle (rebindable input)
    //     plus the JIT-tracked lookup tables ---
    auto fields_() {
        return drjit::tie(accel_handle, leaf_idx_offsets, leaf_idx_table);
    }
    auto fields_() const {
        return drjit::tie(accel_handle, leaf_idx_offsets, leaf_idx_table);
    }
    const char *name_() const { return "MetalAccel"; }
    auto labels_() const {
        return drjit::detail::unpack_labels(
            "accel_handle, leaf_idx_offsets, leaf_idx_table",
            std::make_index_sequence<decltype(fields_())::Size>());
    }
};

NAMESPACE_END(mitsuba)

#endif // MI_ENABLE_METAL
