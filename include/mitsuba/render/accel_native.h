/*
    accel_native.h -- Typed, by-value native (kd-tree) acceleration-structure
    state held by Scene (used when Embree is disabled).

    Bodies live in src/render/scene_native.inl.

    Copyright (c) 2026 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE file.
*/

#pragma once

#if !defined(MI_ENABLE_EMBREE)

#include <mitsuba/render/fwd.h>
#include <drjit/array_traverse.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum> class ShapeKDTree;

template <typename Float, typename Spectrum> struct NativeAccel {
    MI_IMPORT_TYPES(Shape, ShapePtr)

    /// Native kd-tree (ref-counted). Lifetime tied to ``accel_handle`` in JIT
    /// variants; released immediately in scalar mode.
    ShapeKDTree<Float, Spectrum> *accel = nullptr;
    /// Width-specialized kd-tree trace function pointer
    void *func_ptr = nullptr;

    UInt64 accel_handle;
    UInt64 func_handle;
    DynamicBuffer<UInt32> shapes_registry_ids;

    NativeAccel() = default;
    NativeAccel(const NativeAccel &) = delete;
    NativeAccel &operator=(const NativeAccel &) = delete;
    NativeAccel(NativeAccel &&) = delete;
    NativeAccel &operator=(NativeAccel &&) = delete;
    ~NativeAccel() { release(); }

    // --- Lifecycle (bodies in scene_native.inl) ---
    void init(Scene<Float, Spectrum> *scene, const Properties &props);
    void parameters_changed(Scene<Float, Spectrum> *scene);
    void release();

    static void static_initialization() { }
    static void static_shutdown() { }

    // --- Ray queries (bodies in scene_native.inl) ---
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

    // --- Declarative traversal ---
    auto fields_() {
        return drjit::tie(accel_handle, func_handle, shapes_registry_ids);
    }
    auto fields_() const {
        return drjit::tie(accel_handle, func_handle, shapes_registry_ids);
    }
    const char *name_() const { return "NativeAccel"; }
    auto labels_() const {
        return drjit::detail::unpack_labels(
            "accel_handle, func_handle, shapes_registry_ids",
            std::make_index_sequence<decltype(fields_())::Size>());
    }
};

NAMESPACE_END(mitsuba)

#endif // !MI_ENABLE_EMBREE
