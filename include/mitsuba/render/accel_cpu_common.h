/*
    accel_cpu_common.h -- Small shared helpers for the CPU ray-tracing backends
    (Embree and the native kd-tree), which are mutually exclusive but share the
    registry-id construction and the JIT acceleration-handle lifecycle.

    Copyright (c) 2026 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE file.
*/

#pragma once

#include <mitsuba/render/fwd.h>
#include <memory>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Build the per-shape registry-id buffer used by the CPU backends to
 * recover a \c ShapePtr from a hit's geometry / instance index.
 *
 * Returns an empty buffer when the scene has no shapes.
 */
template <typename Float, typename Spectrum>
DynamicBuffer<dr::uint32_array_t<Float>>
build_registry_ids(const std::vector<ref<Shape<Float, Spectrum>>> &shapes) {
    using UInt32 = dr::uint32_array_t<Float>;
    if (shapes.empty())
        return dr::zeros<DynamicBuffer<UInt32>>();
    std::unique_ptr<uint32_t[]> data(new uint32_t[shapes.size()]);
    for (size_t i = 0; i < shapes.size(); ++i)
        data[i] = jit_registry_id(shapes[i]);
    return dr::load<DynamicBuffer<UInt32>>(data.get(), shapes.size());
}

/**
 * \brief Point a JIT acceleration-structure handle at a fresh native pointer.
 *
 * Detaches any previous free callback (so a scene *update* doesn't release the
 * structure), maps the handle's data to \c ptr, and attaches
 * \c callback / \c payload. The by-value accel state dies with the \c Scene, so
 * the callback owns the native object it captures and frees it once no
 * ray-tracing kernel refers to the handle anymore.
 */
template <typename UInt64>
void reset_mapped_handle(UInt64 &handle, void *ptr,
                         void (*callback)(uint32_t, int, void *),
                         void *payload) {
    if (handle.index())
        jit_var_set_callback(handle.index(), nullptr, nullptr);
    handle = UInt64::map_(ptr, 1, false);
    jit_var_set_callback(handle.index(), callback, payload);
}

NAMESPACE_END(mitsuba)
