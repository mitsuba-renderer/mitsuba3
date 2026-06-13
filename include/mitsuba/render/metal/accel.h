/*
    metal/accel.h -- Backend-agnostic interface to the Metal acceleration
    structure builder (src/render/metal_accel.mm).

    Mitsuba's templated scene code (src/render/scene_metal.inl) describes the
    scene as a list of bottom-level acceleration structures (BLAS) and TLAS
    instances using the plain-C++ types below. The Objective-C++ builder turns
    this description into Metal objects (MTLAccelerationStructure, buffers,
    an intersection function table specification) and registers the result
    with Dr.Jit via jit_metal_configure_scene().

    Copyright (c) 2026 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE file.
*/

#pragma once

#if defined(MI_ENABLE_METAL)

#include <mitsuba/core/platform.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(metal_accel)

/**
 * \brief One geometry within a BLAS
 *
 * Metal forbids mixing geometry kinds (triangle / bounding-box / curve)
 * within a single primitive acceleration structure, so all geometries of a
 * \ref BlasDesc must share the same \c kind. Device pointers below must
 * belong to evaluated Dr.Jit arrays on the Metal backend (the builder maps
 * them back to MTLBuffer objects via jit_metal_lookup_buffer()).
 */
struct GeometryDesc {
    enum Kind : uint32_t { Triangle, BoundingBox, Curve };

    Kind kind = Triangle;

    // --- Triangle geometry ---

    /// Device pointer to tightly packed float3 vertex positions
    const void *vertex_ptr = nullptr;
    /// Device pointer to uint32 triangle indices
    const void *index_ptr = nullptr;
    /// Number of triangles
    size_t triangle_count = 0;

    // --- Curve geometry ---

    /// Device pointer to interleaved (x, y, z, radius) control points
    const void *cp_ptr = nullptr;
    /// Number of control points
    size_t cp_count = 0;
    /// Device pointer to uint32 segment start indices
    const void *seg_index_ptr = nullptr;
    /// Number of curve segments
    size_t segment_count = 0;
    /// 1 = linear, 2 = cubic B-spline
    int curve_kind = 0;

    // --- Bounding-box (custom intersection) geometry ---

    /// Number of axis-aligned bounding boxes
    size_t aabb_count = 0;
    /// Per-primitive data element size in bytes
    size_t pdata_size = 0;
    /// Total primitive data size in bytes (shape-level layouts like SDFGrid
    /// may deviate from aabb_count * pdata_size)
    size_t total_data_size = 0;
    /// Intersection function (metal_shape::IntersectionFunctionIndex). Also
    /// determines the MSL [[buffer(..)]] slot of the primitive data. The
    /// geometry's slice in the combined per-type data buffer is published
    /// through the (instance, geometry) lookup table built by build_impl()
    /// (see intersection_functions.h).
    uint32_t fn_idx = 0;
    /// Writes aabb_count * 6 floats (min/max interleaved) to `out`
    std::function<void(void *out)> fill_aabbs;
    /// Writes total_data_size bytes of primitive data to `out`
    std::function<void(void *out)> fill_pdata;
};

/// A bottom-level acceleration structure: one or more same-kind geometries
struct BlasDesc {
    std::vector<GeometryDesc> geoms;
};

/// A TLAS instance referencing a BLAS with a 3x4 affine transformation
struct InstanceDesc {
    uint32_t blas_idx = 0;
    /// Per-instance userID written into the TLAS instance descriptor and
    /// returned by the trace as ``user_instance_id`` (the owning shape's
    /// registry id, used to recover the ShapePtr without a host gather).
    uint32_t user_id = 0;
    /// Column-major 3x4 affine transform (four columns of three floats)
    float transform[12] = { 1.f, 0.f, 0.f,   0.f, 1.f, 0.f,
                            0.f, 0.f, 1.f,   0.f, 0.f, 0.f };
};

struct SceneDesc {
    std::vector<BlasDesc> blases;
    std::vector<InstanceDesc> instances;
};

/// Opaque handle owning the Metal objects of a built scene
struct Accel;

/**
 * \brief Build BLAS/TLAS acceleration structures for the given scene
 * description and register them with Dr.Jit
 *
 * Returns the opaque \ref Accel handle together with the JIT variable index
 * produced by jit_metal_configure_scene(). The variable index must be passed
 * to subsequent jit_metal_ray_trace() calls and is owned by the caller (it
 * holds one reference).
 */
extern MI_EXPORT_LIB std::pair<Accel *, uint32_t> build(const SceneDesc &desc);

/**
 * \brief Release a scene built with \ref build()
 *
 * Invalidates the TLAS reference held by Dr.Jit (in case frozen-function
 * recordings keep the scene variable alive), releases the JIT variable
 * reference, and frees all Metal objects owned by the handle.
 */
extern MI_EXPORT_LIB void release(Accel *accel, uint32_t scene_index);

NAMESPACE_END(metal_accel)
NAMESPACE_END(mitsuba)

#endif // MI_ENABLE_METAL
