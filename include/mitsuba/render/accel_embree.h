/*
    accel_embree.h -- Typed, by-value Embree acceleration-structure state held
    by Scene.

    Header-hygiene-clean half of the Embree backend. The native Embree scene is
    referenced via a forward-declared pointer (no <embree3/rtcore.h> in
    scene.h); its lifetime is tied to the JIT handle variable so it outlives
    Scene while ray-tracing kernels are still pending. Bodies live in
    src/render/scene_embree.inl.

    Copyright (c) 2026 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE file.
*/

#pragma once

#if defined(MI_ENABLE_EMBREE)

#include <mitsuba/render/fwd.h>
#include <drjit/array_traverse.h>
#include <vector>

/// Forward declaration of Embree's opaque scene type (RTCScene == RTCSceneTy*)
struct RTCSceneTy;

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum> struct EmbreeAccel {
    MI_IMPORT_TYPES(Shape, ShapePtr)

    /// Native Embree scene (rtcNewScene). Lifetime tied to ``accel_handle``
    /// in JIT variants; released immediately in scalar mode.
    RTCSceneTy *accel = nullptr;
    std::vector<int> geometries;
    bool is_nested_scene = false;
    /// Width-specialized Embree intersection function pointer (rtcIntersectN)
    void *func_ptr = nullptr;

    /// JIT handle that pins the Embree scene's lifetime (data = ``accel``)
    UInt64 accel_handle;
    /// JIT handle exposing ``func_ptr`` as a traversable variable
    UInt64 func_handle;
    /// Per-shape registry IDs, gathered to recover the ShapePtr from a hit
    DynamicBuffer<UInt32> shapes_registry_ids;

    EmbreeAccel() = default;
    EmbreeAccel(const EmbreeAccel &) = delete;
    EmbreeAccel &operator=(const EmbreeAccel &) = delete;
    EmbreeAccel(EmbreeAccel &&) = delete;
    EmbreeAccel &operator=(EmbreeAccel &&) = delete;
    ~EmbreeAccel() { release(); }

    // --- Lifecycle (bodies in scene_embree.inl) ---
    void init(Scene<Float, Spectrum> *scene, const Properties &props);
    void parameters_changed(Scene<Float, Spectrum> *scene);
    void release();

    static void static_initialization() { }
    static void static_shutdown() { }

    // --- Ray queries (bodies in scene_embree.inl) ---
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
    const char *name_() const { return "EmbreeAccel"; }
    auto labels_() const {
        return drjit::detail::unpack_labels(
            "accel_handle, func_handle, shapes_registry_ids",
            std::make_index_sequence<decltype(fields_())::Size>());
    }
};

NAMESPACE_END(mitsuba)

#endif // MI_ENABLE_EMBREE
