/*
    metal_accel.mm -- Metal acceleration structure builder.
*/

#include "metal/accel.h"

#if defined(MI_ENABLE_METAL)

#include <mitsuba/core/logger.h>
#include <drjit-core/metal.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <mutex>
#include <utility>

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

NAMESPACE_BEGIN(mitsuba)

/// Map a Dr.Jit device pointer back to its MTLBuffer + byte offset. Dr.Jit owns
/// the retained object.
static id<MTLBuffer> lookup_buffer(const void *ptr, size_t *offset,
                                   const char *what) {
    id<MTLBuffer> buf = (__bridge id<MTLBuffer>)
        jit_metal_lookup_buffer((void *) ptr, offset);
    if (!buf)
        Throw("MetalAccel: could not find the Metal buffer backing the "
              "%s data. The corresponding Dr.Jit array must be evaluated "
              "before building the acceleration structure.", what);
    return buf;
}

struct BufferAllocation {
    void *ptr = nullptr;

    BufferAllocation() = default;

    BufferAllocation(size_t size, bool shared)
        : ptr(jit_malloc(JitBackend::Metal, size, shared)) { }

    BufferAllocation(const BufferAllocation &) = delete;
    BufferAllocation &operator=(const BufferAllocation &) = delete;

    BufferAllocation(BufferAllocation &&other) noexcept
        : ptr(other.ptr) {
        other.ptr = nullptr;
    }

    BufferAllocation &operator=(BufferAllocation &&other) noexcept {
        if (this != &other) {
            reset();
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    ~BufferAllocation() { reset(); }

    void reset() {
        if (ptr) {
            jit_free(ptr);
            ptr = nullptr;
        }
    }

    id<MTLBuffer> buffer() const {
        if (!ptr)
            return nil;
        size_t offset = 0;
        id<MTLBuffer> buf = lookup_buffer(ptr, &offset, "scene-builder");
        if (offset != 0)
            Throw("MetalAccel: a scene-builder allocation must start at offset zero.");
        return buf;
    }
};

/// Strong references to Metal objects of a built scene
struct MetalAccelData {
    id<MTLAccelerationStructure> tlas;
    std::vector<id<MTLAccelerationStructure>> blases;
    /// Refernces to buffers the TLAS depends on
    std::vector<id<MTLBuffer>> buffers;
    /// Dr.Jit-backed scene-resident buffers.
    std::vector<BufferAllocation> allocations;
    /// Intersection function bindings of custom shapes
    std::vector<JitIsectBinding *> bindings;

    ~MetalAccelData() {
        for (JitIsectBinding *binding : bindings)
            jit_isect_unbind(binding);
    }
};

/// Allocate output + scratch storage for an acceleration structure and encode
/// its build into \c enc. The scratch allocation is appended to
/// \c temp_allocations.
static id<MTLAccelerationStructure>
encode_accel_build(id<MTLDevice> device,
                   id<MTLAccelerationStructureCommandEncoder> enc,
                   MTLAccelerationStructureDescriptor *desc,
                   std::vector<BufferAllocation> &temp_allocations) {
    MTLAccelerationStructureSizes sizes =
        [device accelerationStructureSizesWithDescriptor: desc];
    id<MTLAccelerationStructure> accel =
        [device newAccelerationStructureWithSize: sizes.accelerationStructureSize];
    BufferAllocation scratch(sizes.buildScratchBufferSize, false);
    id<MTLBuffer> scratch_buf = scratch.buffer();
    if (!accel || !scratch_buf)
        Throw("MetalAccel: failed to allocate acceleration structure "
              "storage (%zu + %zu bytes).",
              (size_t) sizes.accelerationStructureSize,
              (size_t) sizes.buildScratchBufferSize);

    [enc buildAccelerationStructure: accel
                         descriptor: desc
                      scratchBuffer: scratch_buf
                scratchBufferOffset: 0];
    temp_allocations.push_back(std::move(scratch));
    return accel;
}

/// Compact freshly built BLASes that shrink. The caller encodes the TLAS into
/// the returned command buffer.
static void compact_blases(id<MTLDevice> device, id<MTLCommandQueue> queue,
                           __strong id<MTLCommandBuffer> &cb,
                           MetalAccelData *accel,
                           std::vector<BufferAllocation> &temp_allocations) {
    size_t n = accel->blases.size();
    if (n == 0)
        return;

    // Write each BLAS's compacted size into one shared buffer (one uint32 slot
    // each) on an encoder ordered after the builds.
    BufferAllocation size_alloc(n * sizeof(uint32_t), true);
    id<MTLBuffer> size_buf = size_alloc.buffer();
    id<MTLAccelerationStructureCommandEncoder> size_enc =
        [cb accelerationStructureCommandEncoder];
    for (size_t i = 0; i < n; ++i)
        [size_enc writeCompactedAccelerationStructureSize: accel->blases[i]
                                                 toBuffer: size_buf
                                                   offset: i * sizeof(uint32_t)];
    [size_enc endEncoding];

    // The compacted sizes are produced by this raw Metal command buffer.
    [cb commit];
    [cb waitUntilCompleted];
    if (cb.status == MTLCommandBufferStatusError)
        Throw("MetalAccel: BLAS build failed: %s",
              cb.error ? cb.error.localizedDescription.UTF8String
                       : "no error information");
    temp_allocations.clear();

    const uint32_t *size_data = (const uint32_t *) size_alloc.ptr;
    std::vector<uint32_t> compacted_size(size_data, size_data + n);

    // Compact the shrinking BLASes on a fresh command buffer.
    cb = [queue commandBuffer];
    id<MTLAccelerationStructureCommandEncoder> enc =
        [cb accelerationStructureCommandEncoder];
    for (size_t i = 0; i < n; ++i) {
        if (compacted_size[i] >= accel->blases[i].size)
            continue; // No gain, keep the original BLAS
        id<MTLAccelerationStructure> compacted =
            [device newAccelerationStructureWithSize: compacted_size[i]];
        if (!compacted)
            Throw("MetalAccel: failed to allocate compacted acceleration "
                  "structure (%u bytes).", compacted_size[i]);
        [enc copyAndCompactAccelerationStructure: accel->blases[i]
                          toAccelerationStructure: compacted];
        accel->blases[i] = compacted;
    }
    [enc endEncoding];
    // The caller encodes the TLAS into this command buffer.
}

/// Build all Metal objects for the lowered scene and register a fresh Dr.Jit
/// scene variable.
static std::pair<MetalAccelData *, uint32_t>
build_impl(const std::vector<BlasEntry> &blases,
           const std::vector<InstanceEntry> &instances,
           const std::vector<uint32_t> &user_ids, bool compact) {
    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>) jit_metal_context();
        id<MTLCommandQueue> queue =
            (__bridge id<MTLCommandQueue>) jit_metal_command_queue();

        // Flush Dr.Jit's pending work (non-blocking) so our raw build command
        // buffers are enqueued behind the kernels that filled the geometry
        // buffers (mesh/curve data) on the same Metal queue.
        jit_flush_thread();

        // Owns the Metal objects until ownership transfers to the scene variable
        // below. Any Throw before then frees them automatically.
        auto accel = std::make_unique<MetalAccelData>();

        // Pre-pass over the geometry kinds. Custom shapes are intersected by
        // functions that Dr.Jit compiles from the shape's recorded
        // ray_intersect_preliminary(). Each occupies one entry of the
        // scene's intersection function table, in BLAS and geometry order.
        bool any_curves = false;
        bool any_backface_culled_triangles = false;
        std::vector<bool> blas_backface_cull(blases.size(), false);
        std::vector<uint32_t> blas_ift_base(blases.size(), 0u);
        uint32_t n_isect = 0;
        size_t aabb_total = 0;

        for (size_t blas_idx = 0; blas_idx < blases.size(); ++blas_idx) {
            const BlasEntry &blas = blases[blas_idx];
            blas_ift_base[blas_idx] = n_isect;
            for (const ShapeIR &g : blas.geoms) {
                if (g.kind == ShapeIR::Kind::BSplineCurve ||
                    g.kind == ShapeIR::Kind::LinearCurve)
                    any_curves = true;
                if (g.kind == ShapeIR::Kind::TrianglesCulled) {
                    any_backface_culled_triangles = true;
                    blas_backface_cull[blas_idx] = true;
                }
                if (g.kind == ShapeIR::Kind::Custom) {
                    n_isect++;
                    if (!g.aabb_buffer)
                        aabb_total += g.prim_count;
                }
            }
        }

        // ------------------------------------------------------------------
        // Build one BLAS per IR entry.
        // ------------------------------------------------------------------

        // One command buffer holds the BLAS builds and, unless compaction is
        // requested, the TLAS build.
        std::vector<BufferAllocation> temp_allocations;
        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLAccelerationStructureCommandEncoder> blas_enc =
            [cb accelerationStructureCommandEncoder];

        // One shared buffer for all custom AABBs (6 floats each), suballocated
        // by aabb_cursor. The build owns this buffer, so it is not resident at
        // trace time.
        id<MTLBuffer> aabb_pool = nil;
        void *aabb_pool_ptr = nullptr;
        size_t aabb_cursor = 0;
        if (aabb_total > 0) {
            BufferAllocation alloc(aabb_total * 6 * sizeof(float), true);
            aabb_pool_ptr = alloc.ptr;
            aabb_pool = alloc.buffer();
            temp_allocations.push_back(std::move(alloc));
        }

        for (size_t blas_idx = 0; blas_idx < blases.size(); ++blas_idx) {
            const BlasEntry &blas = blases[blas_idx];
            if (blas.geoms.empty())
                Throw("MetalAccel: BLAS %zu has no geometries.", blas_idx);

            NSMutableArray<MTLAccelerationStructureGeometryDescriptor *>
                *geoms = [NSMutableArray arrayWithCapacity: blas.geoms.size()];
            for (const ShapeIR &g : blas.geoms) {
                switch (g.kind) {
                    case ShapeIR::Kind::Triangles:
                    case ShapeIR::Kind::TrianglesCulled: {
                        size_t v_off = 0, i_off = 0;
                        id<MTLBuffer> v_buf =
                            lookup_buffer(g.vertex_ptr, &v_off, "mesh vertex");
                        id<MTLBuffer> i_buf =
                            lookup_buffer(g.index_ptr, &i_off, "mesh index");

                        // Declare the build's reads so the hazard tracker orders
                        // them after the kernels that wrote these buffers.
                        [blas_enc useResource: v_buf usage: MTLResourceUsageRead];
                        [blas_enc useResource: i_buf usage: MTLResourceUsageRead];

                        // Metal has no index stride parameter; describe()
                        // supplies a tightly packed buffer.
                        Assert(g.index_stride == 3 * sizeof(uint32_t));

                        MTLAccelerationStructureTriangleGeometryDescriptor *gd =
                            [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
                        gd.vertexBuffer       = v_buf;
                        gd.vertexBufferOffset = v_off;
                        gd.vertexStride       = g.vertex_stride;
                        gd.vertexFormat       = MTLAttributeFormatFloat3;
                        gd.indexBuffer        = i_buf;
                        gd.indexBufferOffset  = i_off;
                        gd.indexType          = MTLIndexTypeUInt32;
                        gd.triangleCount      = g.face_count;
                        gd.opaque             = YES;
                        [geoms addObject: gd];

                        accel->buffers.push_back(v_buf);
                        accel->buffers.push_back(i_buf);
                        break;
                    }

                    case ShapeIR::Kind::BSplineCurve:
                    case ShapeIR::Kind::LinearCurve: {
                        if (g.cp_count == 0 || g.seg_count == 0)
                            Throw("MetalAccel: curve geometry with zero "
                                  "control points or segments.");

                        size_t cp_off = 0, ix_off = 0;
                        id<MTLBuffer> cp_buf = lookup_buffer(
                            g.cp_ptr, &cp_off, "curve control point");
                        id<MTLBuffer> ix_buf = lookup_buffer(
                            g.seg_ptr, &ix_off, "curve segment index");

                        // Declare the build's reads for the hazard tracker
                        // (see the mesh-vertex case above).
                        [blas_enc useResource: cp_buf usage: MTLResourceUsageRead];
                        [blas_enc useResource: ix_buf usage: MTLResourceUsageRead];

                        // Control points are interleaved (x, y, z, radius)
                        // with a 16-byte stride. The radius view aliases the
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
                        gd.segmentCount             = g.seg_count;
                        bool bspline = g.kind == ShapeIR::Kind::BSplineCurve;
                        gd.segmentControlPointCount = bspline ? 4 : 2;
                        gd.curveBasis               = bspline
                                                          ? MTLCurveBasisBSpline
                                                          : MTLCurveBasisLinear;
                        gd.curveType                = MTLCurveTypeRound;
                        gd.curveEndCaps             = bspline
                                                          ? MTLCurveEndCapsNone
                                                          : MTLCurveEndCapsSphere;
                        gd.opaque                   = YES;
                        [geoms addObject: gd];

                        accel->buffers.push_back(cp_buf);
                        accel->buffers.push_back(ix_buf);
                        break;
                    }

                    case ShapeIR::Kind::Custom: {
                        if (g.prim_count == 0)
                            Throw("MetalAccel: bounding-box geometry with "
                                  "zero AABBs.");

                        // The shape's own device boxes, or its slice of the
                        // shared AABB pool
                        id<MTLBuffer> aabb_buf = aabb_pool;
                        size_t aabb_off = 0;
                        if (g.aabb_buffer) {
                            aabb_buf = lookup_buffer(g.aabb_buffer, &aabb_off,
                                                     "bounding box");
                            [blas_enc useResource: aabb_buf usage: MTLResourceUsageRead];
                        } else {
                            aabb_off = aabb_cursor * 6 * sizeof(float);
                            g.fill_aabbs(g.ctx, (uint8_t *) aabb_pool_ptr + aabb_off);
                            aabb_cursor += g.prim_count;
                        }

                        // BLASes are single-kind, so the geometry's position
                        // is its offset into the BLAS's slice of the table
                        MTLAccelerationStructureBoundingBoxGeometryDescriptor *gd =
                            [MTLAccelerationStructureBoundingBoxGeometryDescriptor descriptor];
                        gd.boundingBoxBuffer       = aabb_buf;
                        gd.boundingBoxBufferOffset = aabb_off;
                        gd.boundingBoxStride       = 6 * sizeof(float);
                        gd.boundingBoxCount        = g.prim_count;
                        gd.intersectionFunctionTableOffset = geoms.count;
                        gd.opaque                  = YES;
                        [geoms addObject: gd];
                        break;
                    }

                    case ShapeIR::Kind::Instance:
                        Throw("MetalAccel: instance geometry must be flattened "
                              "before reaching the BLAS builder.");
                }
            }

            MTLPrimitiveAccelerationStructureDescriptor *bdesc =
                [MTLPrimitiveAccelerationStructureDescriptor descriptor];
            bdesc.geometryDescriptors = geoms;
            accel->blases.push_back(
                encode_accel_build(device, blas_enc, bdesc,
                                   temp_allocations));
        }

        [blas_enc endEncoding];

        // Compaction must precede the TLAS build, which is encoded into the
        // same fresh command buffer.
        if (compact)
            compact_blases(device, queue, cb, accel.get(), temp_allocations);

        if (instances.empty())
            Throw("MetalAccel: scene description contains no instances.");

        // ------------------------------------------------------------------
        // Build the TLAS over all instances
        // ------------------------------------------------------------------
        // userID is the entry's base index into the shape recovery table
        // (see scene_metal.inl); the intersection function table offset is
        // the BLAS's first entry in the scene's table.
        size_t n_inst = instances.size();
        BufferAllocation inst_alloc(
            n_inst * sizeof(MTLAccelerationStructureUserIDInstanceDescriptor),
            true);
        id<MTLBuffer> inst_buf = inst_alloc.buffer();
        void *inst_ptr = inst_alloc.ptr;
        auto *inst_descs =
            (MTLAccelerationStructureUserIDInstanceDescriptor *) inst_ptr;
        temp_allocations.push_back(std::move(inst_alloc));
        for (size_t i = 0; i < n_inst; ++i) {
            const InstanceEntry &inst = instances[i];
            MTLAccelerationStructureUserIDInstanceDescriptor &d = inst_descs[i];
            d = {};
            for (int col = 0; col < 4; ++col)
                d.transformationMatrix.columns[col] =
                    MTLPackedFloat3Make(inst.to_world[col * 3 + 0],
                                        inst.to_world[col * 3 + 1],
                                        inst.to_world[col * 3 + 2]);
            d.options                         = MTLAccelerationStructureInstanceOptionOpaque;
            if (any_backface_culled_triangles &&
                !blas_backface_cull[inst.blas_index])
                d.options |= MTLAccelerationStructureInstanceOptionDisableTriangleCulling;
            d.mask                            = blases[inst.blas_index].visibility_mask;
            d.intersectionFunctionTableOffset = blas_ift_base[inst.blas_index];
            d.accelerationStructureIndex      = inst.blas_index;
            d.userID                          = user_ids[i];
        }

        NSMutableArray<id<MTLAccelerationStructure>> *blas_array =
            [NSMutableArray arrayWithCapacity: accel->blases.size()];
        for (id<MTLAccelerationStructure> blas : accel->blases)
            [blas_array addObject: blas];

        MTLInstanceAccelerationStructureDescriptor *tdesc =
            [MTLInstanceAccelerationStructureDescriptor descriptor];
        tdesc.instancedAccelerationStructures = blas_array;
        tdesc.instanceDescriptorBuffer        = inst_buf;
        tdesc.instanceCount                   = n_inst;
        tdesc.instanceDescriptorType =
            MTLAccelerationStructureInstanceDescriptorTypeUserID;

        // TLAS encoder, ordered after the BLAS builds via Metal's inter-encoder
        // resource tracking.
        id<MTLAccelerationStructureCommandEncoder> tlas_enc =
            [cb accelerationStructureCommandEncoder];
        accel->tlas = encode_accel_build(device, tlas_enc, tdesc,
                                         temp_allocations);
        [tlas_enc endEncoding];

        // Commit without waiting. Tracing kernels queue behind this build.
        [cb commit];
        // jit_free is stream-ordered on the same queue, so it runs after this build.
        temp_allocations.clear();

        // ------------------------------------------------------------------
        // Register the scene with Dr.Jit
        // ------------------------------------------------------------------
        // Bit 0: triangles, bit 1: bounding boxes, bit 2: curves, bit 3:
        // triangle backface culling. Dr.Jit uses this to select the MSL
        // intersector<...> template tags and culling mode.
        uint32_t geom_mask = 0x1u;
        if (n_isect) geom_mask |= 0x2u;
        if (any_curves) geom_mask |= 0x4u;
        if (any_backface_culled_triangles) geom_mask |= 0x8u;

        // Everything the TLAS references must be marked resident when a
        // kernel traces against this scene.
        std::vector<void *> resources;
        resources.reserve(accel->buffers.size() + accel->blases.size() + 1);
        for (id<MTLBuffer> buf : accel->buffers)
            resources.push_back((__bridge void *) buf);
        for (id<MTLAccelerationStructure> blas : accel->blases)
            resources.push_back((__bridge void *) blas);
        resources.push_back((__bridge void *) accel->tlas);

        uint32_t scene_index = jit_metal_configure_scene(
            (__bridge void *) accel->tlas,
            resources.data(), (uint32_t) resources.size(),
            n_isect, geom_mask);

        // Tie the Metal objects' lifetime to the scene variable. Pending
        // kernels or frozen recordings may outlive MetalAccel::release().
        MetalAccelData *accel_p = accel.release();
        jit_metal_scene_set_cleanup(
            scene_index, [](void *p) { delete (MetalAccelData *) p; }, accel_p);

        // Bind the recorded intersection function of each custom shape to
        // its table entry (see the pre-pass above for the numbering)
        uint32_t ift_index = 0;
        for (const BlasEntry &blas : blases)
            for (const ShapeIR &g : blas.geoms)
                if (g.kind == ShapeIR::Kind::Custom)
                    accel_p->bindings.push_back(jit_isect_bind(
                        g.isect_func, scene_index, ift_index++, nullptr));

        Log(Debug, "MetalAccel: built acceleration structures (%zu BLAS, "
                   "%zu instances, %u custom geometries%s)",
            accel_p->blases.size(), instances.size(), n_isect,
            any_curves ? ", curves" : "");

        return { accel_p, scene_index };
    }
}

std::pair<MetalAccelData *, uint32_t>
build_metal_accel(const SceneIR &sd, const std::vector<uint32_t> &user_ids,
                  bool compact) {
    return build_impl(sd.blases, sd.instances, user_ids, compact);
}

void release_metal_accel(MetalAccelData *accel, uint32_t scene_index) {
    @autoreleasepool {
        if (scene_index)
            // build_metal_accel() transferred ownership to the scene variable.
            // Dropping this reference frees the objects once nothing references it.
            jit_var_dec_ref(scene_index);
        else
            delete accel; // Empty scene that was never registered
    }
}

NAMESPACE_END(mitsuba)

#endif // MI_ENABLE_METAL
