/*
    accel_optix.h -- Typed, by-value OptiX acceleration-structure state held by
    Scene.

    Header-hygiene-clean half of the OptiX backend: the per-scene OptiX state
    (SBT, GAS/IAS device data) is pimpl'd behind a forward-declared pointer so
    that <optix.h> never leaks into scene.h. The native state's lifetime is
    tied to the JIT IAS-handle variable so it outlives Scene while ray-tracing
    kernels are still pending. Bodies live in src/render/scene_optix.inl.

    Copyright (c) 2026 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE file.
*/

#pragma once

#if defined(MI_ENABLE_CUDA)

#include <mitsuba/render/fwd.h>
#include <drjit/array_traverse.h>

NAMESPACE_BEGIN(mitsuba)

/// Per-scene OptiX state (SBT/GAS/IAS); defined in src/render/scene_optix.inl
struct MiOptixSceneState;

template <typename Float, typename Spectrum> struct OptixAccel {
    MI_IMPORT_TYPES(Shape, ShapePtr)

    /// Heap-allocated native OptiX state (pimpl). Lifetime tied to
    /// ``accel_handle`` so pending ray-tracing kernels stay valid.
    MiOptixSceneState *state = nullptr;
    /// JIT handle to the IAS (the traversable kernels trace against)
    UInt64 accel_handle;

    OptixAccel() = default;
    OptixAccel(const OptixAccel &) = delete;
    OptixAccel &operator=(const OptixAccel &) = delete;
    OptixAccel(OptixAccel &&) = delete;
    OptixAccel &operator=(OptixAccel &&) = delete;
    ~OptixAccel() { release(); }

    // --- Lifecycle (bodies in scene_optix.inl) ---
    void init(Scene<Float, Spectrum> *scene, const Properties &props);
    void parameters_changed(Scene<Float, Spectrum> *scene);
    void release();

    static void static_initialization() { }
    static void static_shutdown();

    // --- Ray queries (bodies in scene_optix.inl) ---
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

    // --- Declarative traversal (only the IAS handle) ---
    auto fields_() { return drjit::tie(accel_handle); }
    auto fields_() const { return drjit::tie(accel_handle); }
    const char *name_() const { return "OptixAccel"; }
    auto labels_() const {
        return drjit::detail::unpack_labels(
            "accel_handle",
            std::make_index_sequence<decltype(fields_())::Size>());
    }
};

NAMESPACE_END(mitsuba)

#endif // MI_ENABLE_CUDA
