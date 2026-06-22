/*
    metal_accel.mm -- Metal acceleration structure builder.
*/

#include "metal/accel.h"

#if defined(MI_ENABLE_METAL)

#include <mitsuba/core/logger.h>
#include "metal/shapes.h"
#include <drjit-core/metal.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <mutex>
#include <utility>

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

/// Precompiled intersection-function library (built from
/// metal/intersection_functions.metal), embedded as a byte array by CMakeLists.
extern "C" const unsigned char mi_metal_isect_metallib[];
extern "C" const size_t mi_metal_isect_metallib_size;

NAMESPACE_BEGIN(mitsuba)

/// MSL function names, in MetalIntersectionFn order.
static const char *const metal_isect_fn_names[] = {
    "intersection_sphere",     "intersection_disk", "intersection_cylinder",
    "intersection_ellipsoids", "intersection_sdfgrid"
};
static_assert(std::size(metal_isect_fn_names) == METAL_ISECT_FN_COUNT,
              "metal_isect_fn_names must have one entry per MetalIntersectionFn.");

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
};

/// Instantiate (once per device) the shared intersection-function library.
/// Dr.Jit caches pipelines across scenes and resolves function handles against
/// the linked library, so all scenes must share one library.
static id<MTLLibrary> intersection_fn_library(id<MTLDevice> device) {
    static std::mutex mutex;
    static id<MTLDevice> cached_device = nil;
    static id<MTLLibrary> cached_library = nil;
    std::lock_guard<std::mutex> guard(mutex);
    if (cached_library && cached_device == device)
        return cached_library;

    dispatch_data_t lib_data = dispatch_data_create(
        mi_metal_isect_metallib, mi_metal_isect_metallib_size, nullptr,
        DISPATCH_DATA_DESTRUCTOR_DEFAULT);

    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithData: lib_data error: &error];
    if (!library)
        Throw("MetalAccel: failed to instantiate the precompiled intersection "
              "function library: %s",
              error ? error.localizedDescription.UTF8String
                    : "no error information");

    cached_device = device;
    cached_library = library;
    return library;
}

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
           const std::vector<InstanceEntry> &instances, bool compact) {
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

        // Pre-pass: size one combined primitive-data buffer per custom shape
        // type. The MSL intersection functions locate their slice via the
        // (instance, geometry) lookup table built below.
        size_t type_total[METAL_ISECT_FN_COUNT] = {};
        size_t type_elem_size[METAL_ISECT_FN_COUNT] = {};
        bool any_custom = false, any_curves = false;
        bool any_backface_culled_triangles = false;
        std::vector<bool> blas_backface_cull(blases.size(), false);

        // SDFGrid data is variable-length: tracked by 16-byte-aligned byte
        // offsets rather than element counts.
        auto align16 = [](size_t v) { return (v + 15) & ~(size_t) 15; };
        size_t sdf_total = 0;

        // Total AABB count across custom geometries (see aabb_pool below).
        size_t aabb_total = 0;

        for (size_t blas_idx = 0; blas_idx < blases.size(); ++blas_idx) {
            const BlasEntry &blas = blases[blas_idx];
            for (const ShapeIR &g : blas.geoms) {
                if (g.kind == ShapeIR::Kind::BSplineCurve ||
                    g.kind == ShapeIR::Kind::LinearCurve)
                    any_curves = true;
                if (g.kind == ShapeIR::Kind::TrianglesCulled) {
                    any_backface_culled_triangles = true;
                    blas_backface_cull[blas_idx] = true;
                }
                if (g.kind != ShapeIR::Kind::Custom)
                    continue;
                any_custom = true;
                uint32_t fn = metal_fn_index(g.type);
                if (fn >= METAL_ISECT_FN_COUNT)
                    Throw("MetalAccel: shape type 0x%x has no intersection "
                          "function.", (uint32_t) g.type);
                aabb_total += g.prim_count;
                size_t total = g.data_size_bytes();
                if (fn == METAL_ISECT_FN_SDFGRID)
                    sdf_total += align16(total);
                else if (g.pdata_size > 0 && g.prim_count > 0) {
                    // The combined buffer strides by a single per-type element
                    // size, so all shapes of one type must agree on it.
                    if (type_elem_size[fn] != 0 &&
                        type_elem_size[fn] != g.pdata_size)
                        Throw("MetalAccel: shapes of type 0x%x disagree on "
                              "their per-primitive data size (%zu vs %zu).",
                              (uint32_t) g.type, type_elem_size[fn],
                              (size_t) g.pdata_size);
                    type_total[fn] += g.prim_count;
                    type_elem_size[fn] = g.pdata_size;
                }
            }
        }

        id<MTLBuffer> combined_buf[METAL_ISECT_FN_COUNT] = {};
        void *combined_ptr[METAL_ISECT_FN_COUNT] = {};
        size_t type_cursor[METAL_ISECT_FN_COUNT] = {};
        for (uint32_t t = 0; t < METAL_ISECT_FN_COUNT; ++t) {
            if (type_total[t] == 0)
                continue;
            BufferAllocation alloc(type_total[t] * type_elem_size[t], true);
            combined_ptr[t] = alloc.ptr;
            combined_buf[t] = alloc.buffer();
            accel->buffers.push_back(combined_buf[t]);
            accel->allocations.push_back(std::move(alloc));
        }

        id<MTLBuffer> sdf_buf = nil;
        void *sdf_ptr = nullptr;
        size_t sdf_cursor = 0;
        if (sdf_total > 0) {
            BufferAllocation alloc(sdf_total, true);
            sdf_ptr = alloc.ptr;
            sdf_buf = alloc.buffer();
            accel->buffers.push_back(sdf_buf);
            accel->allocations.push_back(std::move(alloc));
        }

        // ------------------------------------------------------------------
        // Build one BLAS per IR entry.
        // ------------------------------------------------------------------

        // IFT names follow bounding-box geometries in BLAS/geometry order.
        std::vector<const char *> ift_names;

        // Per-BLAS values consumed by the TLAS instance descriptors below
        std::vector<uint32_t> blas_ift_base(blases.size(), 0u);

        // Per-BLAS lookup records: one word per bounding-box geometry holding
        // its slice in the type's combined data buffer (an element index, or a
        // byte offset for SDFGrid). BLASes are single-kind, so a geometry's
        // geometry_id is its position within these records.
        std::vector<std::vector<uint32_t>> blas_lookup(blases.size());

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
            uint32_t local_ift_idx = 0;

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

                        MTLAccelerationStructureTriangleGeometryDescriptor *gd =
                            [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
                        gd.vertexBuffer       = v_buf;
                        gd.vertexBufferOffset = v_off;
                        gd.vertexStride       = 3 * sizeof(float);
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
                        gd.curveEndCaps             = MTLCurveEndCapsSphere;
                        gd.opaque                   = YES;
                        [geoms addObject: gd];

                        accel->buffers.push_back(cp_buf);
                        accel->buffers.push_back(ix_buf);
                        break;
                    }

                    case ShapeIR::Kind::Custom: {
                        uint32_t fn = metal_fn_index(g.type);
                        size_t total = g.data_size_bytes();
                        if (g.prim_count == 0 || total == 0)
                            Throw("MetalAccel: bounding-box geometry with "
                                  "zero AABBs / no primitive data.");

                        // This geometry's slice of the shared AABB pool.
                        size_t aabb_off = aabb_cursor * 6 * sizeof(float);
                        g.fill_aabbs(g.ctx,
                                     (uint8_t *) aabb_pool_ptr + aabb_off);
                        aabb_cursor += g.prim_count;

                        // Write the primitive data and record the geometry's
                        // slice in the lookup table (an element index for
                        // fixed-size types, a byte offset for SDFGrid).
                        uint32_t lookup_value = 0;
                        if (fn == METAL_ISECT_FN_SDFGRID) {
                            size_t offset = sdf_cursor;
                            g.fill_data(g.ctx, (uint8_t *) sdf_ptr + offset);
                            sdf_cursor += align16(total);
                            lookup_value = (uint32_t) offset;
                        } else if (combined_buf[fn]) {
                            size_t elem = type_cursor[fn];
                            uint8_t *dst =
                                (uint8_t *) combined_ptr[fn] +
                                elem * type_elem_size[fn];
                            g.fill_data(g.ctx, dst);
                            type_cursor[fn] += g.prim_count;
                            lookup_value = (uint32_t) elem;
                        }
                        blas_lookup[blas_idx].push_back(lookup_value);

                        MTLAccelerationStructureBoundingBoxGeometryDescriptor *gd =
                            [MTLAccelerationStructureBoundingBoxGeometryDescriptor descriptor];
                        gd.boundingBoxBuffer       = aabb_pool;
                        gd.boundingBoxBufferOffset = aabb_off;
                        gd.boundingBoxStride       = 6 * sizeof(float);
                        gd.boundingBoxCount        = g.prim_count;
                        gd.intersectionFunctionTableOffset = local_ift_idx;
                        gd.opaque                  = YES;
                        [geoms addObject: gd];

                        if (local_ift_idx == 0)
                            blas_ift_base[blas_idx] = (uint32_t) ift_names.size();
                        ift_names.push_back(metal_isect_fn_names[fn]);
                        ++local_ift_idx;
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

        // Build the (instance, geometry) -> data-slice lookup table: the first
        // n_instances words hold each instance's base into the per-BLAS records
        // that follow (see intersection_functions.metal).
        if (instances.empty())
            Throw("MetalAccel: scene description contains no instances.");

        id<MTLBuffer> lookup_buf = nil;
        void *lookup_ptr = nullptr;
        if (any_custom) {
            size_t n_inst = instances.size();
            std::vector<uint32_t> blas_lookup_base(blases.size(), 0u);
            uint32_t cursor = (uint32_t) n_inst;
            for (size_t b = 0; b < blases.size(); ++b) {
                blas_lookup_base[b] = cursor;
                cursor += (uint32_t) blas_lookup[b].size();
            }

            BufferAllocation alloc(cursor * sizeof(uint32_t), true);
            lookup_ptr = alloc.ptr;
            lookup_buf = alloc.buffer();
            uint32_t *lookup = (uint32_t *) lookup_ptr;
            for (size_t i = 0; i < n_inst; ++i)
                lookup[i] = blas_lookup_base[instances[i].blas_index];
            for (size_t b = 0; b < blases.size(); ++b)
                std::copy(blas_lookup[b].begin(), blas_lookup[b].end(),
                          lookup + blas_lookup_base[b]);
            accel->buffers.push_back(lookup_buf);
            accel->allocations.push_back(std::move(alloc));
        }

        // ------------------------------------------------------------------
        // Build the TLAS over all instances
        // ------------------------------------------------------------------
        // userID is recovered on the device as pi.instance: the owning
        // Instance's registry id, or 0 (a null shape) for a top-level shape,
        // matching the OptiX backend. [[instance_id]] stays the raw instance
        // index the IFT lookup table is keyed by.
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
            d.mask                            = 0xFFu;
            d.intersectionFunctionTableOffset = blas_ift_base[inst.blas_index];
            d.accelerationStructureIndex      = inst.blas_index;
            d.userID                          =
                inst.owner_registry_id == SCENE_IR_NO_OWNER
                    ? 0u
                    : inst.owner_registry_id;
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
        // Per-device singleton library. The Dr.Jit scene retains its own ref.
        id<MTLLibrary> isect_library =
            any_custom ? intersection_fn_library(device) : nil;

        // Bit 0: triangles, bit 1: bounding boxes, bit 2: curves, bit 3:
        // triangle backface culling. Dr.Jit uses this to select the MSL
        // intersector<...> template tags and culling mode.
        uint32_t geom_mask = 0x1u;
        if (any_custom) geom_mask |= 0x2u;
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

        // IFT buffer bindings (shared argument table): each present type's data
        // buffer, plus the (instance, geometry) lookup table at slot
        // METAL_ISECT_FN_COUNT. Bounded, so use fixed-size arrays.
        void    *bind_buffers[METAL_ISECT_FN_COUNT + 1];
        uint32_t bind_slots[METAL_ISECT_FN_COUNT + 1];
        uint32_t bind_count = 0;
        for (uint32_t t = 0; t < METAL_ISECT_FN_COUNT; ++t) {
            id<MTLBuffer> buf =
                t == METAL_ISECT_FN_SDFGRID ? sdf_buf : combined_buf[t];
            if (buf) {
                bind_buffers[bind_count] = (__bridge void *) buf;
                bind_slots[bind_count]   = t;
                ++bind_count;
            }
        }
        if (lookup_buf) {
            bind_buffers[bind_count] = (__bridge void *) lookup_buf;
            bind_slots[bind_count]   = METAL_ISECT_FN_COUNT;
            ++bind_count;
        }

        uint32_t scene_index = jit_metal_configure_scene(
            (__bridge void *) accel->tlas,
            resources.data(), (uint32_t) resources.size(),
            (__bridge void *) isect_library,
            (uint32_t) ift_names.size(),
            ift_names.empty() ? nullptr : ift_names.data(),
            bind_count,
            bind_count ? bind_buffers : nullptr,
            bind_count ? bind_slots   : nullptr,
            geom_mask);

        // Tie the Metal objects' lifetime to the scene variable. Pending
        // kernels or frozen recordings may outlive MetalAccel::release().
        jit_metal_scene_set_cleanup(
            scene_index, [](void *p) { delete (MetalAccelData *) p; }, accel.get());

        Log(Debug, "MetalAccel: built acceleration structures (%zu BLAS, "
                   "%zu instances, %zu custom geometries%s)",
            accel->blases.size(), instances.size(), ift_names.size(),
            any_curves ? ", curves" : "");

        return { accel.release(), scene_index };
    }
}

std::pair<MetalAccelData *, uint32_t>
build_metal_accel(const SceneIR &sd, bool compact) {
    return build_impl(sd.blases, sd.instances, compact);
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
