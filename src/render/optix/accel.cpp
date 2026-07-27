/*
    optix/accel.cpp -- OptiX acceleration structure builder (implements
    optix/accel.h).
*/

#include "accel.h"

#if defined(MI_ENABLE_CUDA)

#include <mitsuba/core/logger.h>
#include <drjit-core/optix.h> // jit_malloc/free, jit_optix_check, jit_cuda_stream, ...

#include <array>

NAMESPACE_BEGIN(mitsuba)

MiOptixAccelData::~MiOptixAccelData() {
    for (HandleData &h : gas)
        if (h.buffer)
            jit_free(h.buffer);
}

/// Allocate (once) \c g's custom-primitive SBT data buffer in \c data_buffers
/// (indexed by the stable ``g.data_slot``) and return its stable device
/// pointer, without writing its contents. Triangles, curves, and shape groups
/// carry no SBT data here and return ``nullptr``. The SBT only needs the stable
/// pointer at pack time; \ref optix_refresh_shape_data writes the data.
static void *optix_shape_data_ptr(const ShapeIR &g,
                                  ShapeDataBuffers &data_buffers) {
    if (g.type == ShapeType::ShapeGroup ||
        g.kind != ShapeIR::Kind::Custom)
        return nullptr;
    size_t data_size = g.data_size_bytes();
    if (!data_size || !g.fill_data || g.data_slot == (uint32_t) -1)
        return nullptr;

    // Stable per-shape device buffer (allocated once, refilled in place).
    if (g.data_slot >= data_buffers.size())
        data_buffers.resize(g.data_slot + 1, nullptr);
    void *&buf = data_buffers[g.data_slot];
    if (!buf)
        buf = jit_malloc(JitBackend::CUDA, data_size);
    return buf;
}

void optix_refresh_shape_data(const SceneIR &sd,
                              ShapeDataBuffers &data_buffers) {
    // Refill every custom shape's SBT data buffer in place (allocating it on
    // first use). The SBT records keep pointing at the same buffers, so updated
    // per-primitive values reach the device without rebuilding the SBT. Order
    // does not matter: each custom shape's buffer is independent.
    for (const BlasEntry &b : sd.blases)
        for (const ShapeIR &g : b.geoms) {
            void *buf = optix_shape_data_ptr(g, data_buffers);
            if (!buf)
                continue;

            // Fill host-visible shared scratch directly, upload it with an
            // async queue-ordered copy.
            size_t data_size = g.data_size_bytes();
            void *shared = jit_malloc(JitBackend::CUDA, data_size, /* shared = */ 1);
            g.fill_data(g.ctx, shared);
            jit_memcpy_async(JitBackend::CUDA, buf, shared, data_size);
            jit_free(shared);
        }
}

size_t count_hitgroup_records(const std::vector<BlasEntry> &blases) {
    size_t count = 0;
    for (const BlasEntry &b : blases)
        count += b.geoms.size();
    return count;
}

void fill_hitgroup_records(const std::vector<BlasEntry> &blases,
                           HitGroupSbtRecord *out, size_t &cursor,
                           const OptixProgramGroup *pg,
                           const OptixProgramGroupMapping &pg_mapping,
                           ShapeDataBuffers &data_buffers) {
    // The BLAS list is already in canonical (kind, slot) order, so packing one
    // record per geom in BLAS order produces the contiguous-per-BLAS SBT layout
    // whose base offsets prepare_ias() assigns to the IAS instances. Each record
    // carries the hit shape's registry id (jit_registry_id(g.ctx)), recovered as
    // pi.shape on the device, plus the program group for the shape's type.
    for (const BlasEntry &b : blases) {
        for (const ShapeIR &g : b.geoms) {
            void *data = optix_shape_data_ptr(g, data_buffers);
            HitGroupSbtRecord rec{ { jit_registry_id(g.ctx), data } };
            uint32_t pg_index = pg_mapping.at(g.type);
            jit_optix_check(optixSbtRecordPackHeader(pg[pg_index], &rec));
            out[cursor++] = rec;
        }
    }
}

/**
 * \brief Fill an \ref OptixBuildInput from a shape's \ref describe()
 * descriptor \c g (triangles, curves or custom primitives).
 *
 * \c ptr_storage provides stable backing for device-buffer pointers that OptiX
 * references by address. It must outlive the build that consumes \c build_input.
 * For host-AABB custom primitives, \c aabb_ptr is the device address of this
 * shape's slice in the shared AABB pool. Shapes with their own device AABB
 * buffer ignore it.
 */
static void optix_fill_build_input(OptixBuildInput &build_input,
                                   const ShapeIR &g, void *aabb_ptr,
                                   void *ptr_storage[2]) {
    static const uint32_t flags_disable_anyhit[1] =
        { OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT };
    static const uint32_t flags_none[1] = { OPTIX_GEOMETRY_FLAG_NONE };

    switch (g.kind) {
        case ShapeIR::Kind::Triangles:
        case ShapeIR::Kind::TrianglesCulled:
            ptr_storage[0] = (void *) g.vertex_ptr;
            build_input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
            build_input.triangleArray.vertexFormat     = OPTIX_VERTEX_FORMAT_FLOAT3;
            build_input.triangleArray.indexFormat      = OPTIX_INDICES_FORMAT_UNSIGNED_INT3;
            build_input.triangleArray.numVertices      = (unsigned int) g.vertex_count;
            build_input.triangleArray.vertexBuffers    = (CUdeviceptr *) &ptr_storage[0];
            build_input.triangleArray.numIndexTriplets = (unsigned int) g.face_count;
            build_input.triangleArray.indexBuffer      = (CUdeviceptr) g.index_ptr;
            build_input.triangleArray.flags            = flags_disable_anyhit;
            build_input.triangleArray.numSbtRecords    = 1;
            break;

        case ShapeIR::Kind::BSplineCurve:
        case ShapeIR::Kind::LinearCurve:
            ptr_storage[0] = (void *) g.cp_ptr;
            ptr_storage[1] = (void *) ((const float *) g.cp_ptr + 3);
            build_input.type = OPTIX_BUILD_INPUT_TYPE_CURVES;
            build_input.curveArray.curveType            =
                g.kind == ShapeIR::Kind::BSplineCurve
                ? OPTIX_PRIMITIVE_TYPE_ROUND_CUBIC_BSPLINE
                : OPTIX_PRIMITIVE_TYPE_ROUND_LINEAR;
            build_input.curveArray.numPrimitives        = (unsigned int) g.seg_count;
            build_input.curveArray.vertexBuffers        = (CUdeviceptr *) &ptr_storage[0];
            build_input.curveArray.numVertices          = (unsigned int) g.cp_count;
            build_input.curveArray.vertexStrideInBytes  = sizeof(float) * 4;
            build_input.curveArray.widthBuffers         = (CUdeviceptr *) &ptr_storage[1];
            build_input.curveArray.widthStrideInBytes   = sizeof(float) * 4;
            build_input.curveArray.indexBuffer          = (CUdeviceptr) g.seg_ptr;
            build_input.curveArray.indexStrideInBytes   = sizeof(uint32_t);
            build_input.curveArray.normalBuffers        = 0;
            build_input.curveArray.normalStrideInBytes  = 0;
            build_input.curveArray.flag                 = OPTIX_GEOMETRY_FLAG_DISABLE_ANYHIT;
            build_input.curveArray.primitiveIndexOffset = 0;
            build_input.curveArray.endcapFlags          = OPTIX_CURVE_ENDCAP_DEFAULT;
            break;

        case ShapeIR::Kind::Custom:
            // Zero-copy device AABBs (ellipsoids/sdfgrid) if supplied, otherwise
            // this shape's slice in the build's shared AABB pool.
            ptr_storage[0] = g.aabb_buffer ? (void *) g.aabb_buffer : aabb_ptr;
            build_input.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
            build_input.customPrimitiveArray.aabbBuffers   = (CUdeviceptr *) &ptr_storage[0];
            build_input.customPrimitiveArray.numPrimitives = (unsigned int) g.prim_count;
            build_input.customPrimitiveArray.strideInBytes = 6 * sizeof(float);
            build_input.customPrimitiveArray.flags         =
                g.type == ShapeType::Ellipsoids ? flags_none
                                                : flags_disable_anyhit;
            build_input.customPrimitiveArray.numSbtRecords = 1;
            break;

        default:
            Throw("optix_fill_build_input(): unexpected geometry kind.");
    }
}

void build_gas(const OptixDeviceContext &context,
               const std::vector<BlasEntry> &blases,
               const std::vector<uint32_t> &indices,
               MiOptixAccelData &out_accel,
               bool compact) {

    // Release this scope's current GAS buffers, then size the handle list for
    // this build.
    for (MiOptixAccelData::HandleData &h : out_accel.gas)
        if (h.buffer)
            jit_free(h.buffer);
    out_accel.gas.assign(indices.size(), MiOptixAccelData::HandleData{});
    if (indices.empty())
        return;

    // Per-GAS state carried from the build phase to the compaction phase.
    struct PendingGas {
        MiOptixAccelData::HandleData *handle;
        OptixTraversableHandle accel;
        void *output_buffer;
        size_t output_size;
    };
    std::vector<PendingGas> pending;
    pending.reserve(indices.size());

    // Build one GAS over a BLAS's same-kind geoms.
    auto build_single_gas = [&context, &pending, compact](
            const std::vector<ShapeIR> &geoms,
            MiOptixAccelData::HandleData &handle,
            CUdeviceptr compact_size_slot) {
        size_t shapes_count = geoms.size();

        OptixAccelBuildOptions accel_options = {};
        accel_options.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
        if (compact)
            accel_options.buildFlags |= OPTIX_BUILD_FLAG_ALLOW_COMPACTION;
        accel_options.operation  = OPTIX_BUILD_OPERATION_BUILD;
        accel_options.motionOptions.numKeys = 0;

        // Host-filled custom AABBs share a single transient device buffer for
        // this GAS, suballocated by aabb_offset[i]. Ellipsoids and SDFGrid keep
        // their own device AABB buffers.
        std::vector<size_t> aabb_offset(shapes_count, (size_t) -1);
        size_t aabb_total = 0;
        for (size_t i = 0; i < shapes_count; i++) {
            const ShapeIR &g = geoms[i];
            if (g.kind == ShapeIR::Kind::Custom && !g.aabb_buffer) {
                aabb_offset[i] = aabb_total;
                aabb_total += g.prim_count;
            }
        }

        void *aabb_pool = nullptr;
        if (aabb_total > 0) {
            size_t bytes = aabb_total * 6 * sizeof(float);
            // Fill host-visible shared scratch directly, then upload it with an
            // async queue-ordered copy.
            void *shared = jit_malloc(JitBackend::CUDA, bytes, /* shared = */ 1);
            for (size_t i = 0; i < shapes_count; i++) {
                if (aabb_offset[i] == (size_t) -1)
                    continue;
                const ShapeIR &g = geoms[i];
                g.fill_aabbs(g.ctx, (uint8_t *) shared +
                                        aabb_offset[i] * 6 * sizeof(float));
            }
            aabb_pool = jit_malloc(JitBackend::CUDA, bytes);
            jit_memcpy_async(JitBackend::CUDA, aabb_pool, shared, bytes);
            jit_free(shared);
        }

        // ``bi_ptrs[i]`` backs device-buffer pointers that OptiX references by
        // address, so it outlives the build below.
        std::vector<OptixBuildInput> build_inputs(shapes_count);
        std::vector<std::array<void *, 2>> bi_ptrs(shapes_count);
        for (size_t i = 0; i < shapes_count; i++) {
            void *aabb_ptr =
                aabb_offset[i] == (size_t) -1
                    ? nullptr
                    : (void *) ((uint8_t *) aabb_pool +
                                aabb_offset[i] * 6 * sizeof(float));
            optix_fill_build_input(build_inputs[i], geoms[i], aabb_ptr,
                                   bi_ptrs[i].data());
        }

        OptixAccelBufferSizes buffer_sizes;
        jit_optix_check(optixAccelComputeMemoryUsage(
            context,
            &accel_options,
            build_inputs.data(),
            (unsigned int) shapes_count,
            &buffer_sizes
        ));

        void* d_temp_buffer = jit_malloc(JitBackend::CUDA, buffer_sizes.tempSizeInBytes);
        void* output_buffer = jit_malloc(JitBackend::CUDA, buffer_sizes.outputSizeInBytes);

        OptixAccelEmitDesc emit_property = {};
        OptixAccelEmitDesc *emit_property_ptr = nullptr;
        uint32_t emit_property_count = 0;
        if (compact) {
            emit_property.type   = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;
            emit_property.result = compact_size_slot; // 8-byte-aligned slot
            emit_property_ptr    = &emit_property;
            emit_property_count  = 1;
        }

        OptixTraversableHandle accel;
        jit_optix_check(optixAccelBuild(
            context,
            (CUstream) jit_cuda_stream(),
            &accel_options,
            build_inputs.data(),
            (unsigned int) shapes_count,
            (CUdeviceptr) d_temp_buffer,
            buffer_sizes.tempSizeInBytes,
            (CUdeviceptr) output_buffer,
            buffer_sizes.outputSizeInBytes,
            &accel,
            emit_property_ptr,
            emit_property_count
        ));

        jit_free(d_temp_buffer);
        // OptiX consumes the transient AABB pool during the GAS build.
        if (aabb_pool)
            jit_free(aabb_pool);

        pending.push_back({ &handle, accel, output_buffer,
                            buffer_sizes.outputSizeInBytes });
    };

    scoped_optix_context guard;

    size_t n_builds = indices.size();
    void *compact_sizes = nullptr;
    if (compact)
        compact_sizes = jit_malloc(JitBackend::CUDA,
                                   n_builds * sizeof(uint64_t));

    // ``out_accel.gas`` is sized above and never resized here, and ``pending``
    // is reserved, so the &handle / &pending entry pointers stay stable.
    for (size_t k = 0; k < n_builds; k++) {
        CUdeviceptr compact_size_slot =
            compact
                ? (CUdeviceptr) ((uint8_t *) compact_sizes +
                                 k * sizeof(uint64_t))
                : 0;
        build_single_gas(blases[indices[k]].geoms, out_accel.gas[k],
                         compact_size_slot);
    }

    if (!compact) {
        for (PendingGas &p : pending) {
            p.handle->handle = p.accel;
            p.handle->buffer = p.output_buffer;
        }
        return;
    }

    // Read every emitted compacted size with one synchronizing copy.
    std::vector<uint64_t> sizes(n_builds);
    jit_memcpy(JitBackend::CUDA, sizes.data(), compact_sizes,
               n_builds * sizeof(uint64_t));
    jit_free(compact_sizes);

    // Compact the GASes that shrink. The IAS build that consumes these handles
    // is ordered after the compactions on the same stream.
    for (size_t i = 0; i < pending.size(); i++) {
        PendingGas &p = pending[i];
        OptixTraversableHandle accel = p.accel;
        void *output_buffer = p.output_buffer;
        if (sizes[i] < p.output_size) {
            void* compact_buffer = jit_malloc(JitBackend::CUDA, sizes[i]);
            jit_optix_check(optixAccelCompact(
                context,
                (CUstream) jit_cuda_stream(),
                accel,
                (CUdeviceptr) compact_buffer,
                sizes[i],
                &accel
            ));
            jit_free(output_buffer);
            output_buffer = compact_buffer;
        }

        p.handle->handle = accel;
        p.handle->buffer = output_buffer;
    }
}

void prepare_ias(const SceneIR &sd,
                 const std::vector<OptixTraversableHandle> &blas_handle,
                 const std::vector<uint32_t> &blas_sbt_offset,
                 OptixInstance *out) {
    for (size_t i = 0; i < sd.instances.size(); ++i) {
        const InstanceEntry &inst = sd.instances[i];
        const BlasEntry &blas = sd.blases[inst.blas_index];

        // Face culling is enabled only for back-face-culled triangles
        // (EllipsoidsMesh). Every other kind disables triangle face culling so
        // ordinary meshes stay double-sided.
        uint32_t flags = blas.kind == ShapeIR::Kind::TrianglesCulled
                             ? OPTIX_INSTANCE_FLAG_NONE
                             : OPTIX_INSTANCE_FLAG_DISABLE_TRIANGLE_FACE_CULLING;

        // instanceId is recovered on the device as pi.instance: the owning
        // Instance's registry id for an instanced hit, or 0 (null) for a
        // top-level shape.
        uint32_t instance_id = inst.owner_registry_id == SCENE_IR_NO_OWNER
                                   ? 0u
                                   : inst.owner_registry_id;

        // to_world is column-major 3x4 (to_world[col*3 + row]). OptiX wants
        // row-major 3x4.
        float t[12];
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
                t[row * 4 + col] = inst.to_world[col * 3 + row];

        out[i] = OptixInstance{
            { t[0], t[1], t[2],  t[3],
              t[4], t[5], t[6],  t[7],
              t[8], t[9], t[10], t[11] },
            instance_id, blas_sbt_offset[inst.blas_index],
            /* visibilityMask = */ 255, flags,
            blas_handle[inst.blas_index], /* pads = */ { 0, 0 }
        };
    }
}

NAMESPACE_END(mitsuba)

#endif // defined(MI_ENABLE_CUDA)
