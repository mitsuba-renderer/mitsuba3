/*
    metal_accel.mm -- Metal acceleration structure builder

    Implements the interface declared in mitsuba/render/metal/accel.h:
    translates a backend-agnostic scene description (BLAS/TLAS/geometry
    descriptors assembled by scene_metal.inl) into Metal objects and
    registers them with Dr.Jit via jit_metal_configure_scene().

    Copyright (c) 2026 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE file.
*/

#include <mitsuba/render/metal/accel.h>

#if defined(MI_ENABLE_METAL)

#include <mitsuba/core/logger.h>
#include <mitsuba/render/metal/shapes.h>
#include <mitsuba/render/metal/intersection_functions.h>
#include <drjit-core/metal.h>

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(metal_accel)

/// Owns the Metal objects of a built scene (ARC keeps members alive until
/// the Accel instance is deleted in release()).
struct Accel {
    id<MTLAccelerationStructure> tlas;
    std::vector<id<MTLAccelerationStructure>> blases;
    /// Strong references to every buffer the TLAS depends on. This includes
    /// buffers borrowed from Dr.Jit arrays (vertex/index/control-point data);
    /// holding an extra reference on those is harmless and guards against
    /// premature release while the acceleration structure is alive.
    std::vector<id<MTLBuffer>> buffers;
};

/// Compile (once per device) the shared intersection-function library.
///
/// The MSL source is a single global constant (``kIntersectionFunctionsMSL``),
/// so every scene can — and *must* — share one ``MTLLibrary``. This is a
/// correctness requirement, not merely an optimization: Dr.Jit caches compiled
/// ray-tracing kernels and their pipeline states across scenes with identical
/// geometry. A pipeline links specific ``MTLFunction`` objects, and
/// ``functionHandleWithFunction:`` only resolves a function against the *same*
/// library that was linked into the pipeline. If each scene compiled its own
/// library, a cached pipeline (linked against an earlier scene's library)
/// would fail to resolve a later scene's intersection functions — silently
/// yielding null function-table handles and thus missing intersections for
/// custom (bounding-box / curve) shapes.
static id<MTLLibrary> intersection_fn_library(id<MTLDevice> device) {
    static id<MTLDevice> cached_device = nil;
    static id<MTLLibrary> cached_library = nil;
    if (cached_library && cached_device == device)
        return cached_library;

    MTLCompileOptions *opts = [MTLCompileOptions new];
    // [[primitive_data]] requires MSL 3.0+, curve tags MSL 3.1+
    opts.languageVersion = MTLLanguageVersion3_2;

    NSError *error = nil;
    id<MTLLibrary> library = [device
        newLibraryWithSource:
            [NSString stringWithUTF8String:
                metal_shape::kIntersectionFunctionsMSL]
                     options: opts
                       error: &error];
    if (!library)
        Throw("metal_accel: failed to compile the intersection "
              "function library: %s",
              error ? error.localizedDescription.UTF8String
                    : "no error information");

    cached_device = device;
    cached_library = library;
    return library;
}

/// Build a primitive acceleration structure and synchronously wait for it
static id<MTLAccelerationStructure>
build_accel(id<MTLDevice> device, id<MTLCommandQueue> queue,
            MTLAccelerationStructureDescriptor *desc) {
    MTLAccelerationStructureSizes sizes =
        [device accelerationStructureSizesWithDescriptor: desc];
    id<MTLAccelerationStructure> accel =
        [device newAccelerationStructureWithSize: sizes.accelerationStructureSize];
    id<MTLBuffer> scratch =
        [device newBufferWithLength: sizes.buildScratchBufferSize
                            options: MTLResourceStorageModePrivate];
    if (!accel || !scratch)
        Throw("metal_accel: failed to allocate acceleration structure "
              "storage (%zu + %zu bytes).",
              (size_t) sizes.accelerationStructureSize,
              (size_t) sizes.buildScratchBufferSize);

    id<MTLCommandBuffer> cb = [queue commandBuffer];
    id<MTLAccelerationStructureCommandEncoder> enc =
        [cb accelerationStructureCommandEncoder];
    [enc buildAccelerationStructure: accel
                         descriptor: desc
                      scratchBuffer: scratch
                scratchBufferOffset: 0];
    [enc endEncoding];
    [cb commit];
    [cb waitUntilCompleted];

    return accel;
}

/// Map a Dr.Jit device pointer back to its MTLBuffer + byte offset
static id<MTLBuffer> lookup_buffer(const void *ptr, size_t *offset,
                                   const char *what) {
    id<MTLBuffer> buf = (__bridge id<MTLBuffer>)
        jit_metal_lookup_buffer((void *) ptr, offset);
    if (!buf)
        Throw("metal_accel: could not find the Metal buffer backing the "
              "%s data. The corresponding Dr.Jit array must be evaluated "
              "before building the acceleration structure.", what);
    return buf;
}

/// Shared implementation of build() / update(). When ``update_index`` is
/// nonzero, the freshly built Metal objects replace those of the existing
/// scene instead of registering a new one.
static std::pair<Accel *, uint32_t> build_impl(const SceneDesc &desc,
                                               uint32_t update_index) {
    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>) jit_metal_context();
        id<MTLCommandQueue> queue =
            (__bridge id<MTLCommandQueue>) jit_metal_command_queue();

        auto *accel = new Accel();

        // ------------------------------------------------------------------
        // Pre-pass: allocate one combined primitive-data buffer per custom
        // shape type. Each bounding-box geometry's data is written into its
        // type's buffer; the MSL intersection function locates the right
        // slice through a scene-global (instance, geometry) lookup table
        // built further below (see intersection_functions.h).
        // ------------------------------------------------------------------
        size_t type_total[metal_shape::INTERSECTION_FN_COUNT] = {};
        size_t type_elem_size[metal_shape::INTERSECTION_FN_COUNT] = {};
        bool any_custom = false, any_curves = false;

        /* SDFGrid data is shape-level and variable-length; its slices are
           tracked with byte offsets (16-byte aligned) rather than element
           counts. */
        auto align16 = [](size_t v) { return (v + 15) & ~(size_t) 15; };
        size_t sdf_total = 0;

        for (const BlasDesc &blas : desc.blases) {
            for (const GeometryDesc &g : blas.geoms) {
                if (g.kind == GeometryDesc::Curve)
                    any_curves = true;
                if (g.kind != GeometryDesc::BoundingBox)
                    continue;
                any_custom = true;
                if (g.fn_idx >= metal_shape::INTERSECTION_FN_COUNT)
                    Throw("metal_accel: invalid intersection function "
                          "index %u.", g.fn_idx);
                if (g.fn_idx == metal_shape::INTERSECTION_FN_SDFGRID)
                    sdf_total += align16(g.total_data_size);
                else if (g.pdata_size > 0 && g.aabb_count > 0) {
                    type_total[g.fn_idx] += g.aabb_count;
                    type_elem_size[g.fn_idx] = g.pdata_size;
                }
            }
        }

        id<MTLBuffer> combined_buf[metal_shape::INTERSECTION_FN_COUNT] = {};
        size_t type_cursor[metal_shape::INTERSECTION_FN_COUNT] = {};
        for (uint32_t t = 0; t < metal_shape::INTERSECTION_FN_COUNT; ++t) {
            if (type_total[t] == 0)
                continue;
            combined_buf[t] =
                [device newBufferWithLength: type_total[t] * type_elem_size[t]
                                    options: MTLResourceStorageModeShared];
            accel->buffers.push_back(combined_buf[t]);
        }

        id<MTLBuffer> sdf_buf = nil;
        size_t sdf_cursor = 0;
        if (sdf_total > 0) {
            sdf_buf = [device newBufferWithLength: sdf_total
                                          options: MTLResourceStorageModeShared];
            accel->buffers.push_back(sdf_buf);
        }

        // ------------------------------------------------------------------
        // Build one BLAS per BlasDesc
        // ------------------------------------------------------------------

        // Scene-wide intersection function table specification (one entry
        // per bounding-box geometry, allocated in BLAS/geometry order)
        std::vector<std::string> ift_names;

        // Per-BLAS values consumed by the TLAS instance descriptors below
        std::vector<uint32_t> blas_ift_base(desc.blases.size(), 0u);

        /* Per-BLAS record of the (instance, geometry) lookup table: one
           word per bounding-box geometry, holding the geometry's slice in
           its type's combined data buffer — an element index for the
           fixed-size types, a byte offset for SDFGrid. BLASes are
           single-kind (Metal forbids mixing), so a bounding-box geometry's
           geometry_id is its position within these records. */
        std::vector<std::vector<uint32_t>> blas_lookup(desc.blases.size());

        for (size_t blas_idx = 0; blas_idx < desc.blases.size(); ++blas_idx) {
            const BlasDesc &blas = desc.blases[blas_idx];
            if (blas.geoms.empty())
                Throw("metal_accel: BLAS %zu has no geometries.", blas_idx);

            NSMutableArray<MTLAccelerationStructureGeometryDescriptor *>
                *geoms = [NSMutableArray arrayWithCapacity: blas.geoms.size()];
            uint32_t local_ift_idx = 0;

            for (const GeometryDesc &g : blas.geoms) {
                switch (g.kind) {
                    case GeometryDesc::Triangle: {
                        size_t v_off = 0, i_off = 0;
                        id<MTLBuffer> v_buf =
                            lookup_buffer(g.vertex_ptr, &v_off, "mesh vertex");
                        id<MTLBuffer> i_buf =
                            lookup_buffer(g.index_ptr, &i_off, "mesh index");

                        MTLAccelerationStructureTriangleGeometryDescriptor *gd =
                            [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
                        gd.vertexBuffer       = v_buf;
                        gd.vertexBufferOffset = v_off;
                        gd.vertexStride       = 3 * sizeof(float);
                        gd.vertexFormat       = MTLAttributeFormatFloat3;
                        gd.indexBuffer        = i_buf;
                        gd.indexBufferOffset  = i_off;
                        gd.indexType          = MTLIndexTypeUInt32;
                        gd.triangleCount      = g.triangle_count;
                        gd.opaque             = YES;
                        [geoms addObject: gd];

                        accel->buffers.push_back(v_buf);
                        accel->buffers.push_back(i_buf);
                        break;
                    }

                    case GeometryDesc::Curve: {
                        size_t cp_off = 0, ix_off = 0;
                        id<MTLBuffer> cp_buf = lookup_buffer(
                            g.cp_ptr, &cp_off, "curve control point");
                        id<MTLBuffer> ix_buf = lookup_buffer(
                            g.seg_index_ptr, &ix_off, "curve segment index");

                        // Control points are interleaved (x, y, z, radius)
                        // with a 16-byte stride; the radius view aliases the
                        // same buffer at offset 12.
                        MTLAccelerationStructureCurveGeometryDescriptor *gd =
                            [MTLAccelerationStructureCurveGeometryDescriptor descriptor];
                        gd.controlPointBuffer       = cp_buf;
                        gd.controlPointBufferOffset = cp_off;
                        gd.controlPointFormat       = MTLAttributeFormatFloat3;
                        gd.controlPointStride       = 4 * sizeof(float);
                        gd.controlPointCount        = g.cp_count;
                        gd.radiusBuffer             = cp_buf;
                        gd.radiusBufferOffset       = cp_off + 3 * sizeof(float);
                        gd.radiusFormat             = MTLAttributeFormatFloat;
                        gd.radiusStride             = 4 * sizeof(float);
                        gd.indexBuffer              = ix_buf;
                        gd.indexBufferOffset        = ix_off;
                        gd.indexType                = MTLIndexTypeUInt32;
                        gd.segmentCount             = g.segment_count;
                        gd.segmentControlPointCount = g.curve_kind == 2 ? 4 : 2;
                        gd.curveBasis               = g.curve_kind == 2
                                                          ? MTLCurveBasisBSpline
                                                          : MTLCurveBasisLinear;
                        gd.curveType                = MTLCurveTypeRound;
                        gd.curveEndCaps             = MTLCurveEndCapsSphere;
                        gd.opaque                   = YES;
                        [geoms addObject: gd];

                        accel->buffers.push_back(cp_buf);
                        accel->buffers.push_back(ix_buf);
                        break;
                    }

                    case GeometryDesc::BoundingBox: {
                        if (g.aabb_count == 0 || g.total_data_size == 0)
                            Throw("metal_accel: bounding-box geometry with "
                                  "zero AABBs / no primitive data.");

                        id<MTLBuffer> aabb_buf = [device
                            newBufferWithLength: g.aabb_count * 6 * sizeof(float)
                                        options: MTLResourceStorageModeShared];
                        g.fill_aabbs(aabb_buf.contents);
                        accel->buffers.push_back(aabb_buf);

                        // Write the primitive data and record the geometry's
                        // slice in the lookup table (an element index for
                        // fixed-size types, a byte offset for SDFGrid).
                        uint32_t lookup_value = 0;
                        if (g.fn_idx == metal_shape::INTERSECTION_FN_SDFGRID) {
                            size_t offset = sdf_cursor;
                            g.fill_pdata((uint8_t *) sdf_buf.contents + offset);
                            sdf_cursor += align16(g.total_data_size);
                            lookup_value = (uint32_t) offset;
                        } else if (combined_buf[g.fn_idx]) {
                            size_t elem = type_cursor[g.fn_idx];
                            uint8_t *dst =
                                (uint8_t *) combined_buf[g.fn_idx].contents +
                                elem * type_elem_size[g.fn_idx];
                            g.fill_pdata(dst);
                            type_cursor[g.fn_idx] += g.aabb_count;
                            lookup_value = (uint32_t) elem;
                        }
                        blas_lookup[blas_idx].push_back(lookup_value);

                        MTLAccelerationStructureBoundingBoxGeometryDescriptor *gd =
                            [MTLAccelerationStructureBoundingBoxGeometryDescriptor descriptor];
                        gd.boundingBoxBuffer       = aabb_buf;
                        gd.boundingBoxBufferOffset = 0;
                        gd.boundingBoxStride       = 6 * sizeof(float);
                        gd.boundingBoxCount        = g.aabb_count;
                        gd.intersectionFunctionTableOffset = local_ift_idx;
                        gd.opaque                  = YES;
                        [geoms addObject: gd];

                        if (local_ift_idx == 0)
                            blas_ift_base[blas_idx] = (uint32_t) ift_names.size();
                        ift_names.emplace_back(
                            metal_shape::kFunctionNames[g.fn_idx]);
                        ++local_ift_idx;
                        break;
                    }
                }
            }

            MTLPrimitiveAccelerationStructureDescriptor *bdesc =
                [MTLPrimitiveAccelerationStructureDescriptor descriptor];
            bdesc.geometryDescriptors = geoms;
            accel->blases.push_back(build_accel(device, queue, bdesc));
        }

        // ------------------------------------------------------------------
        // Build the (instance, geometry) -> data-slice lookup table consumed
        // by the bounding-box intersection functions (see the comment in
        // intersection_functions.h): the first n_instances words hold each
        // instance's base index into the per-BLAS records that follow.
        // ------------------------------------------------------------------
        if (desc.instances.empty())
            Throw("metal_accel: scene description contains no instances.");

        id<MTLBuffer> lookup_buf = nil;
        if (any_custom) {
            size_t n_inst = desc.instances.size();
            std::vector<uint32_t> blas_lookup_base(desc.blases.size(), 0u);
            uint32_t cursor = (uint32_t) n_inst;
            for (size_t b = 0; b < desc.blases.size(); ++b) {
                blas_lookup_base[b] = cursor;
                cursor += (uint32_t) blas_lookup[b].size();
            }

            std::vector<uint32_t> lookup(cursor);
            for (size_t i = 0; i < n_inst; ++i)
                lookup[i] = blas_lookup_base[desc.instances[i].blas_idx];
            for (size_t b = 0; b < desc.blases.size(); ++b)
                std::copy(blas_lookup[b].begin(), blas_lookup[b].end(),
                          lookup.begin() + blas_lookup_base[b]);

            lookup_buf = [device
                newBufferWithBytes: lookup.data()
                            length: lookup.size() * sizeof(uint32_t)
                           options: MTLResourceStorageModeShared];
            accel->buffers.push_back(lookup_buf);
        }

        // ------------------------------------------------------------------
        // Build the TLAS over all instances
        // ------------------------------------------------------------------
        std::vector<MTLAccelerationStructureInstanceDescriptor>
            inst_descs(desc.instances.size());
        for (size_t i = 0; i < desc.instances.size(); ++i) {
            const InstanceDesc &inst = desc.instances[i];
            MTLAccelerationStructureInstanceDescriptor &d = inst_descs[i];
            d = {};
            for (int col = 0; col < 4; ++col)
                d.transformationMatrix.columns[col] =
                    MTLPackedFloat3Make(inst.transform[col * 3 + 0],
                                        inst.transform[col * 3 + 1],
                                        inst.transform[col * 3 + 2]);
            d.options                         = MTLAccelerationStructureInstanceOptionOpaque;
            d.mask                            = 0xFFu;
            d.intersectionFunctionTableOffset = blas_ift_base[inst.blas_idx];
            d.accelerationStructureIndex      = inst.blas_idx;
        }

        id<MTLBuffer> inst_buf = [device
            newBufferWithBytes: inst_descs.data()
                        length: inst_descs.size() * sizeof(inst_descs[0])
                       options: MTLResourceStorageModeShared];

        NSMutableArray<id<MTLAccelerationStructure>> *blas_array =
            [NSMutableArray arrayWithCapacity: accel->blases.size()];
        for (id<MTLAccelerationStructure> blas : accel->blases)
            [blas_array addObject: blas];

        MTLInstanceAccelerationStructureDescriptor *tdesc =
            [MTLInstanceAccelerationStructureDescriptor descriptor];
        tdesc.instancedAccelerationStructures = blas_array;
        tdesc.instanceDescriptorBuffer        = inst_buf;
        tdesc.instanceCount                   = inst_descs.size();
        tdesc.instanceDescriptorType =
            MTLAccelerationStructureInstanceDescriptorTypeDefault;

        accel->tlas = build_accel(device, queue, tdesc);

        // ------------------------------------------------------------------
        // Compile the intersection function library (if needed) and register
        // the scene with Dr.Jit
        // ------------------------------------------------------------------
        /* The library is a per-device singleton; the Dr.Jit scene retains
           its own reference in jit_metal_configure_scene(). */
        id<MTLLibrary> isect_library =
            any_custom ? intersection_fn_library(device) : nil;

        // Bit 0: triangles, bit 1: bounding boxes, bit 2: curves. Dr.Jit
        // uses this to select the MSL intersector<...> template tags.
        uint32_t geom_mask = 0x1u;
        if (any_custom) geom_mask |= 0x2u;
        if (any_curves) geom_mask |= 0x4u;

        // Everything the TLAS references must be marked resident when a
        // kernel traces against this scene.
        std::vector<void *> resources;
        resources.reserve(accel->buffers.size() + accel->blases.size() + 1);
        for (id<MTLBuffer> buf : accel->buffers)
            resources.push_back((__bridge void *) buf);
        for (id<MTLAccelerationStructure> blas : accel->blases)
            resources.push_back((__bridge void *) blas);
        resources.push_back((__bridge void *) accel->tlas);

        std::vector<const char *> name_ptrs;
        name_ptrs.reserve(ift_names.size());
        for (const std::string &s : ift_names)
            name_ptrs.push_back(s.c_str());

        /* IFT buffer bindings (the table's argument table is shared by all
           entries): the combined data buffer of every shape type present,
           plus the (instance, geometry) lookup table at slot
           INTERSECTION_FN_COUNT. */
        std::vector<void *>   bind_buffers;
        std::vector<uint32_t> bind_slots;
        for (uint32_t t = 0; t < metal_shape::INTERSECTION_FN_COUNT; ++t) {
            id<MTLBuffer> buf =
                t == metal_shape::INTERSECTION_FN_SDFGRID ? sdf_buf
                                                          : combined_buf[t];
            if (buf) {
                bind_buffers.push_back((__bridge void *) buf);
                bind_slots.push_back(t);
            }
        }
        if (lookup_buf) {
            bind_buffers.push_back((__bridge void *) lookup_buf);
            bind_slots.push_back(metal_shape::INTERSECTION_FN_COUNT);
        }

        /* Register a new scene with Dr.Jit (update_index == 0), or swap the
           freshly built Metal objects into the existing one. */
        uint32_t scene_index = jit_metal_configure_scene(
            update_index,
            (__bridge void *) accel->tlas,
            resources.data(), (uint32_t) resources.size(),
            (__bridge void *) isect_library,
            (uint32_t) ift_names.size(),
            name_ptrs.empty()    ? nullptr : name_ptrs.data(),
            (uint32_t) bind_buffers.size(),
            bind_buffers.empty() ? nullptr : bind_buffers.data(),
            bind_slots.empty()   ? nullptr : bind_slots.data(),
            geom_mask);

        /* Tie the lifetime of the Metal objects to that of the scene
           variable: unevaluated kernels and frozen-function recordings may
           keep the scene alive past accel_release_metal() (e.g. a
           PreliminaryIntersection obtained before a geometry update is
           still valid afterwards, mirroring the Embree/OptiX backends). */
        jit_metal_scene_set_cleanup(
            scene_index, [](void *p) { delete (Accel *) p; }, accel);

        Log(Debug, "metal_accel: built acceleration structures (%zu BLAS, "
                   "%zu instances, %zu custom geometries%s)",
            accel->blases.size(), desc.instances.size(), ift_names.size(),
            any_curves ? ", curves" : "");

        return { accel, scene_index };
    }
}

std::pair<Accel *, uint32_t> build(const SceneDesc &desc) {
    return build_impl(desc, 0);
}

Accel *update(uint32_t scene_index, Accel *old_accel, const SceneDesc &desc) {
    Accel *accel = build_impl(desc, scene_index).first;
    /* In-flight command buffers retain the resources they reference, so the
       previous generation of Metal objects can be released right away. */
    delete old_accel;
    return accel;
}

void release(Accel *accel, uint32_t scene_index) {
    @autoreleasepool {
        if (scene_index)
            /* Ownership of the Metal objects was transferred to the scene
               variable in build(); dropping this reference frees them once
               no kernel/recording references the scene anymore. */
            jit_var_dec_ref(scene_index);
        else
            delete accel; // Empty scene that was never registered
    }
}

NAMESPACE_END(metal_accel)
NAMESPACE_END(mitsuba)

#endif // MI_ENABLE_METAL
