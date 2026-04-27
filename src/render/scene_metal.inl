/*
    scene_metal.inl -- Metal GPU ray tracing via Metal Acceleration Structures

    Copyright (c) 2026 Wenzel Jakob / Sebastien Speierer

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE file.
*/

#include <drjit-core/metal.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/metal/intersection_functions.h>
#include <mitsuba/render/metal/shapes.h>

NAMESPACE_BEGIN(mitsuba)

// Pack a 4x4 affine matrix into Metal's column-major MTL::PackedFloat4x3
// (4 columns of PackedFloat3: columns 0-2 are basis vectors, column 3 is
// translation). Mirrors OptiX's row-major float[12] layout in
// include/mitsuba/render/optix/shapes.h:252-258.
static inline MTL::PackedFloat4x3
mtl_pack_4x3(const dr::Matrix<float, 4> &M) {
    MTL::PackedFloat4x3 r;
    r.columns[0] = MTL::PackedFloat3{(float) M(0, 0), (float) M(1, 0), (float) M(2, 0)};
    r.columns[1] = MTL::PackedFloat3{(float) M(0, 1), (float) M(1, 1), (float) M(2, 1)};
    r.columns[2] = MTL::PackedFloat3{(float) M(0, 2), (float) M(1, 2), (float) M(2, 2)};
    r.columns[3] = MTL::PackedFloat3{(float) M(0, 3), (float) M(1, 3), (float) M(2, 3)};
    return r;
}

static inline MTL::PackedFloat4x3 mtl_identity_4x3() {
    MTL::PackedFloat4x3 r;
    r.columns[0] = MTL::PackedFloat3{1.f, 0.f, 0.f};
    r.columns[1] = MTL::PackedFloat3{0.f, 1.f, 0.f};
    r.columns[2] = MTL::PackedFloat3{0.f, 0.f, 1.f};
    r.columns[3] = MTL::PackedFloat3{0.f, 0.f, 0.f};
    return r;
}

// Metal acceleration structure state, stored in Scene::m_accel
template <typename UInt32>
struct MetalAccelState {
    void *tlas = nullptr;  // MTL::InstanceAccelerationStructure*
    std::vector<void *> blas_list;  // per-shape BLAS handles
    std::vector<void *> resources;  // all buffers the TLAS references
    /// Buffers that we OWN (allocated via device->newBuffer in this file)
    /// and must release in accel_release_metal. The `resources` vector also
    /// tracks borrowed buffers (e.g. mesh vertex/index buffers obtained via
    /// jit_metal_lookup_buffer) which are NOT owned by us — releasing those
    /// would corrupt drjit's internal book-keeping.
    std::vector<void *> owned_buffers;
    DynamicBuffer<UInt32> shapes_registry_ids;  // shape index → registry ID
    void *intersection_library = nullptr;  // MTL::Library* with intersection fns
};

MI_VARIANT void Scene<Float, Spectrum>::accel_init_metal(const Properties &/*props*/) {
    if constexpr (dr::is_metal_v<Float>) {
        auto *device = (MTL::Device *) jit_metal_context();
        auto *queue  = (MTL::CommandQueue *) jit_metal_command_queue();

        auto *state = new MetalAccelState<UInt32>();
        m_accel = state;

        // Build shapes registry ID array (maps instance_id → shape registry ID)
        if (!m_shapes.empty()) {
            std::unique_ptr<uint32_t[]> reg_ids(new uint32_t[m_shapes.size()]);
            for (size_t i = 0; i < m_shapes.size(); i++)
                reg_ids[i] = jit_registry_id(m_shapes[i]);
            state->shapes_registry_ids =
                dr::load<DynamicBuffer<UInt32>>(reg_ids.get(), m_shapes.size());
        }

        // ----------------------------------------------------------------
        //  1. Build BLAS for each mesh
        // ----------------------------------------------------------------
        std::vector<MTL::AccelerationStructure *> blas_vec;
        std::vector<MTL::AccelerationStructureInstanceDescriptor> instances;

        // Per-IFT-entry tracking for custom-shape BLAS (one entry per BLAS).
        std::vector<std::string> ift_function_names;
        std::vector<void *> ift_data_buffers;
        std::vector<uint32_t> ift_buffer_slots;

        // We build TLAS instances in m_shapes order so that Metal's
        // `_hit.instance_id` directly indexes the shape registry array.
        // (Mitsuba reorders shapes internally — non-mesh shapes can appear
        // before meshes — so deferring custom shapes to a second pass would
        // break the mapping.)
        //
        // To enforce the invariant `instance_id == m_shapes_index` we use
        // shape_idx directly as the instance index. Any failure path inside
        // the loop must Throw rather than `continue`, otherwise subsequent
        // shapes get the wrong _hit.instance_id and the registry gather at
        // ray_intersect_preliminary_metal returns a wrong shape pointer.
        bool any_custom = false;
        bool any_curves = false;

        // Detect when multiple custom shapes share the same MSL buffer slot
        // (one per shape type). Currently the per-IFT-entry data buffer is
        // bound at the type-specific slot (sphere=0, disk=1, cylinder=2,
        // ellipsoids=3, sdfgrid=4). Because IntersectionFunctionTable
        // setBuffer:offset:atIndex: binds GLOBALLY across all entries (last
        // write wins), having two shapes of the same type silently makes
        // both read from the LATER shape's data buffer. Warn if this case
        // arises until we add per-type BLAS coalescing.
        bool seen_fn_slot[metal_shape::INTERSECTION_FN_COUNT] = { false };

        for (size_t shape_idx = 0; shape_idx < m_shapes.size(); ++shape_idx) {
            uint32_t instance_id = (uint32_t) shape_idx;
            auto &shape = m_shapes[shape_idx];
            auto *mesh = dynamic_cast<mitsuba::Mesh<Float, Spectrum> *>(shape.get());

            if (!mesh) {
                // ============================================================
                //  Native curve BLAS (Metal HW curves, macOS 14+)
                // ============================================================
                int curve_kind = shape->metal_curve_kind();
                if (curve_kind != 0) {
                    Shape *shape_ptr = shape.get();
                    uint32_t cp_var = 0, idx_var = 0;
                    size_t cp_count = 0, seg_count = 0;
                    shape_ptr->metal_get_curve_data(
                        &cp_var, &cp_count, &idx_var, &seg_count);

                    if (cp_count == 0 || seg_count == 0)
                        Throw("scene_metal: curve shape \"%s\" reports zero "
                              "control points or segments — cannot build a "
                              "BLAS for it (would break the instance_id ↔ "
                              "m_shapes_index mapping required by the shape "
                              "registry lookup).",
                              std::string(shape_ptr->id()).c_str());

                    // Materialize the buffers so we can find the underlying
                    // MTL::Buffer objects.
                    jit_var_eval(cp_var);
                    jit_var_eval(idx_var);
                    dr::sync_thread();

                    void *cp_ptr = nullptr, *idx_ptr = nullptr;
                    // jit_var_data returns a NEW reference to the evaluated
                    // variable — we must release it once we're done with the
                    // raw pointer (the MTL::Buffer survives via the shape's
                    // own JIT array). Forgetting to dec_ref leaks a JIT
                    // variable per call.
                    uint32_t cp_data_idx  = jit_var_data(cp_var, &cp_ptr);
                    uint32_t idx_data_idx = jit_var_data(idx_var, &idx_ptr);

                    size_t cp_off = 0, idx_off = 0;
                    auto *cp_buf  = (MTL::Buffer *) jit_metal_lookup_buffer(
                        cp_ptr, &cp_off);
                    auto *ix_buf  = (MTL::Buffer *) jit_metal_lookup_buffer(
                        idx_ptr, &idx_off);

                    jit_var_dec_ref(cp_data_idx);
                    jit_var_dec_ref(idx_data_idx);

                    if (!cp_buf || !ix_buf)
                        Throw("scene_metal: could not find Metal buffers for "
                              "curve shape \"%s\".",
                              std::string(shape_ptr->id()).c_str());
                    any_curves = true;

                    auto *geom_desc =
                        MTL::AccelerationStructureCurveGeometryDescriptor::alloc()->init();

                    // Control-point buffer: interleaved (x, y, z, radius),
                    // stride 16 bytes. Position is float3, radius is the 4th
                    // float (offset 12).
                    geom_desc->setControlPointBuffer(cp_buf);
                    geom_desc->setControlPointBufferOffset(cp_off);
                    geom_desc->setControlPointFormat(MTL::AttributeFormatFloat3);
                    geom_desc->setControlPointStride(4 * sizeof(float));
                    geom_desc->setControlPointCount(cp_count);

                    geom_desc->setRadiusBuffer(cp_buf);
                    geom_desc->setRadiusBufferOffset(cp_off + 3 * sizeof(float));
                    geom_desc->setRadiusFormat(MTL::AttributeFormatFloat);
                    geom_desc->setRadiusStride(4 * sizeof(float));

                    geom_desc->setIndexBuffer(ix_buf);
                    geom_desc->setIndexBufferOffset(idx_off);
                    geom_desc->setIndexType(MTL::IndexTypeUInt32);

                    geom_desc->setSegmentCount(seg_count);
                    geom_desc->setSegmentControlPointCount(curve_kind == 2 ? 4 : 2);
                    geom_desc->setCurveBasis(
                        curve_kind == 2 ? MTL::CurveBasisBSpline
                                        : MTL::CurveBasisLinear);
                    geom_desc->setCurveType(MTL::CurveTypeRound);
                    geom_desc->setCurveEndCaps(MTL::CurveEndCapsSphere);

                    NS::Array *geom_array = NS::Array::array(
                        (const NS::Object *const *) &geom_desc, 1);
                    auto *blas_desc =
                        MTL::PrimitiveAccelerationStructureDescriptor::alloc()->init();
                    blas_desc->setGeometryDescriptors(geom_array);

                    auto sizes = device->accelerationStructureSizes(blas_desc);
                    auto *blas = device->newAccelerationStructure(
                        sizes.accelerationStructureSize);
                    auto *scratch = device->newBuffer(
                        sizes.buildScratchBufferSize,
                        MTL::ResourceStorageModePrivate);

                    auto *cb = queue->commandBuffer();
                    auto *enc = cb->accelerationStructureCommandEncoder();
                    enc->buildAccelerationStructure(blas, blas_desc, scratch, 0);
                    enc->endEncoding();
                    cb->commit();
                    cb->waitUntilCompleted();

                    scratch->release();
                    geom_desc->release();
                    blas_desc->release();

                    blas_vec.push_back(blas);
                    state->blas_list.push_back(blas);
                    state->resources.push_back(cp_buf);
                    state->resources.push_back(ix_buf);
                    state->resources.push_back(blas);

                    MTL::AccelerationStructureInstanceDescriptor inst = {};
                    inst.transformationMatrix             = mtl_identity_4x3();
                    inst.options                          = MTL::AccelerationStructureInstanceOptionOpaque;
                    inst.mask                             = 0xFF;
                    inst.intersectionFunctionTableOffset  = 0;
                    inst.accelerationStructureIndex       = instance_id;
                    instances.push_back(inst);

                    continue;
                }

                // ------------------------------------------------------------
                //  Custom (bounding-box) shape BLAS
                // ------------------------------------------------------------
                Shape *shape_ptr = shape.get();
                size_t n_aabb = shape_ptr->metal_aabb_count();
                size_t pdata_size = shape_ptr->metal_primitive_data_size();
                size_t total_data_size = shape_ptr->metal_total_data_size();
                uint32_t fn_idx = shape_ptr->metal_intersection_function_index();

                if (n_aabb == 0 || total_data_size == 0)
                    Throw("scene_metal: custom shape \"%s\" reports zero "
                          "AABBs or no data — cannot build a BLAS for it "
                          "(would break the instance_id ↔ m_shapes_index "
                          "mapping required by the shape registry lookup).",
                          std::string(shape_ptr->id()).c_str());
                any_custom = true;

                if (fn_idx < metal_shape::INTERSECTION_FN_COUNT) {
                    if (seen_fn_slot[fn_idx])
                        Log(Warn, "scene_metal: scene has multiple custom "
                                  "shapes that share intersection-function "
                                  "slot %u (e.g. multiple instances of the "
                                  "same shape type). The MSL function reads "
                                  "from a single global per-slot buffer, so "
                                  "all such shapes will see the LAST shape's "
                                  "data. Group shapes of the same type into "
                                  "one BLAS to fix.", fn_idx);
                    seen_fn_slot[fn_idx] = true;
                }

                auto *aabb_buf = device->newBuffer(
                    n_aabb * 6 * sizeof(float),
                    MTL::ResourceStorageModeShared);
                auto *pdata_buf = device->newBuffer(
                    total_data_size,
                    MTL::ResourceStorageModeShared);

                shape_ptr->metal_fill_aabb_data(aabb_buf->contents());
                shape_ptr->metal_fill_primitive_data(pdata_buf->contents());

                auto *geom_desc =
                    MTL::AccelerationStructureBoundingBoxGeometryDescriptor::alloc()->init();
                geom_desc->setBoundingBoxBuffer(aabb_buf);
                geom_desc->setBoundingBoxBufferOffset(0);
                geom_desc->setBoundingBoxStride(6 * sizeof(float));
                geom_desc->setBoundingBoxCount(n_aabb);
                if (pdata_size > 0) {
                    geom_desc->setPrimitiveDataBuffer(pdata_buf);
                    geom_desc->setPrimitiveDataBufferOffset(0);
                    geom_desc->setPrimitiveDataStride(pdata_size);
                    geom_desc->setPrimitiveDataElementSize(pdata_size);
                }
                geom_desc->setIntersectionFunctionTableOffset(0);

                NS::Array *geom_array = NS::Array::array(
                    (const NS::Object *const *) &geom_desc, 1);
                auto *blas_desc =
                    MTL::PrimitiveAccelerationStructureDescriptor::alloc()->init();
                blas_desc->setGeometryDescriptors(geom_array);

                auto sizes = device->accelerationStructureSizes(blas_desc);
                auto *blas = device->newAccelerationStructure(
                    sizes.accelerationStructureSize);
                auto *scratch = device->newBuffer(
                    sizes.buildScratchBufferSize,
                    MTL::ResourceStorageModePrivate);

                auto *cb = queue->commandBuffer();
                auto *enc = cb->accelerationStructureCommandEncoder();
                enc->buildAccelerationStructure(blas, blas_desc, scratch, 0);
                enc->endEncoding();
                cb->commit();
                cb->waitUntilCompleted();

                scratch->release();
                geom_desc->release();
                blas_desc->release();

                blas_vec.push_back(blas);
                state->blas_list.push_back(blas);
                state->resources.push_back(aabb_buf);
                state->resources.push_back(pdata_buf);
                state->resources.push_back(blas);
                // We allocated these via device->newBuffer; record them so
                // accel_release_metal can release the retain.
                state->owned_buffers.push_back(aabb_buf);
                state->owned_buffers.push_back(pdata_buf);

                uint32_t ift_entry_idx = (uint32_t) ift_function_names.size();
                ift_function_names.emplace_back(
                    metal_shape::kFunctionNames[fn_idx]);
                ift_data_buffers.push_back(pdata_buf);
                ift_buffer_slots.push_back(fn_idx);

                MTL::AccelerationStructureInstanceDescriptor inst = {};
                inst.transformationMatrix             = mtl_identity_4x3();
                inst.options                          = MTL::AccelerationStructureInstanceOptionOpaque;
                inst.mask                             = 0xFF;
                inst.intersectionFunctionTableOffset  = ift_entry_idx;
                inst.accelerationStructureIndex       = instance_id;
                instances.push_back(inst);

                continue;
            }

            // Get vertex and index data (must be evaluated on device)
            dr::eval(mesh->vertex_positions_buffer(),
                     mesh->faces_buffer());
            dr::sync_thread();

            void *vertex_ptr = nullptr, *index_ptr = nullptr;
            size_t vertex_count = mesh->vertex_count();
            size_t face_count   = mesh->face_count();

            // jit_var_data returns a NEW JIT-variable reference — we must
            // release it once we have looked up the underlying MTL::Buffer
            // (the buffer itself survives via the mesh's own JIT array).
            // Forgetting to dec_ref leaks a JIT variable per call.
            uint32_t vbuf_idx = jit_var_data(
                mesh->vertex_positions_buffer().index(), &vertex_ptr);
            uint32_t ibuf_idx = jit_var_data(
                mesh->faces_buffer().index(), &index_ptr);

            // Find the MTL::Buffer objects for vertex/index data
            size_t v_off = 0, i_off = 0;
            auto *v_buf = (MTL::Buffer *) jit_metal_lookup_buffer(
                vertex_ptr, &v_off);
            auto *i_buf = (MTL::Buffer *) jit_metal_lookup_buffer(
                index_ptr, &i_off);

            jit_var_dec_ref(vbuf_idx);
            jit_var_dec_ref(ibuf_idx);

            (void) vertex_count;

            if (!v_buf || !i_buf)
                Throw("scene_metal: could not find Metal buffers for mesh "
                      "\"%s\".", std::string(mesh->id()).c_str());

            // Create triangle geometry descriptor
            auto *geom_desc =
                MTL::AccelerationStructureTriangleGeometryDescriptor::alloc()->init();
            geom_desc->setVertexBuffer(v_buf);
            geom_desc->setVertexBufferOffset(v_off);
            geom_desc->setVertexStride(sizeof(float) * 3);
            geom_desc->setVertexFormat(MTL::AttributeFormatFloat3);
            geom_desc->setIndexBuffer(i_buf);
            geom_desc->setIndexBufferOffset(i_off);
            geom_desc->setIndexType(MTL::IndexTypeUInt32);
            geom_desc->setTriangleCount(face_count);

            NS::Array *geom_array = NS::Array::array(
                (const NS::Object *const *) &geom_desc, 1);

            // Create BLAS descriptor
            auto *blas_desc =
                MTL::PrimitiveAccelerationStructureDescriptor::alloc()->init();
            blas_desc->setGeometryDescriptors(geom_array);

            // Get BLAS sizes
            auto sizes = device->accelerationStructureSizes(blas_desc);

            // Allocate BLAS
            auto *blas = device->newAccelerationStructure(sizes.accelerationStructureSize);

            // Allocate scratch buffer
            auto *scratch = device->newBuffer(sizes.buildScratchBufferSize,
                                               MTL::ResourceStorageModePrivate);

            // Build BLAS
            auto *cb = queue->commandBuffer();
            auto *enc = cb->accelerationStructureCommandEncoder();
            enc->buildAccelerationStructure(blas, blas_desc, scratch, 0);
            enc->endEncoding();
            cb->commit();
            cb->waitUntilCompleted();

            scratch->release();
            geom_desc->release();
            blas_desc->release();

            blas_vec.push_back(blas);
            state->blas_list.push_back(blas);
            state->resources.push_back(v_buf);
            state->resources.push_back(i_buf);
            state->resources.push_back(blas);

            // Create instance descriptor.
            // For triangle meshes, m_to_world is already baked into vertex
            // positions at load time (see obj.cpp:237, ply.cpp:279,
            // serialized.cpp:362), so we use identity here. This matches
            // OptiX's behavior (scene_optix.inl:467 also passes identity).
            MTL::AccelerationStructureInstanceDescriptor inst = {};
            inst.transformationMatrix             = mtl_identity_4x3();
            inst.options                          = MTL::AccelerationStructureInstanceOptionOpaque;
            inst.mask                             = 0xFF;
            inst.intersectionFunctionTableOffset  = 0;
            inst.accelerationStructureIndex       = instance_id;
            instances.push_back(inst);
        }

        if (instances.empty()) {
            Log(Warn, "scene_metal: no meshes found, ray tracing will not work.");
            jit_metal_configure_rt(nullptr, nullptr, 0);
            return;
        }

        // ----------------------------------------------------------------
        //  2. Build TLAS (instance acceleration structure)
        // ----------------------------------------------------------------
        auto *inst_buf = device->newBuffer(
            instances.data(),
            instances.size() * sizeof(MTL::AccelerationStructureInstanceDescriptor),
            MTL::ResourceStorageModeShared);

        // Create BLAS reference array
        NS::Array *blas_array = NS::Array::array(
            (const NS::Object *const *) blas_vec.data(), blas_vec.size());

        auto *tlas_desc =
            MTL::InstanceAccelerationStructureDescriptor::alloc()->init();
        tlas_desc->setInstancedAccelerationStructures(blas_array);
        tlas_desc->setInstanceDescriptorBuffer(inst_buf);
        tlas_desc->setInstanceCount(instances.size());

        auto sizes = device->accelerationStructureSizes(tlas_desc);
        auto *tlas = device->newAccelerationStructure(sizes.accelerationStructureSize);
        auto *scratch = device->newBuffer(sizes.buildScratchBufferSize,
                                           MTL::ResourceStorageModePrivate);

        auto *cb = queue->commandBuffer();
        auto *enc = cb->accelerationStructureCommandEncoder();
        enc->buildAccelerationStructure(tlas, tlas_desc, scratch, 0);
        enc->endEncoding();
        cb->commit();
        cb->waitUntilCompleted();

        scratch->release();
        tlas_desc->release();
        inst_buf->release();

        state->tlas = tlas;
        state->resources.push_back(tlas);

        // ----------------------------------------------------------------
        //  3. Compile the MSL intersection-function library (if needed)
        //     and bind everything to drjit for ray tracing.
        // ----------------------------------------------------------------
        // Geometry-types mask: bit 0 = triangle, bit 1 = bounding_box,
        // bit 2 = curves. drjit uses this to choose the intersector<...>
        // template tags.
        uint32_t geom_mask = 0x1u;                  // triangle always present
        if (any_custom)  geom_mask |= 0x2u;         // bounding_box (custom)
        if (any_curves)  geom_mask |= 0x4u;         // native curves

        if (any_custom) {
            // Compile our embedded MSL source into an MTL::Library. drjit will
            // link the named [[intersection(...)]] functions into ray-tracing
            // pipelines via MTLLinkedFunctions.
            NS::String *src = NS::String::string(
                metal_shape::kIntersectionFunctionsMSL,
                NS::StringEncoding::UTF8StringEncoding);
            auto *opts = MTL::CompileOptions::alloc()->init();
            // The [[primitive_data]] attribute on intersection-function
            // parameters requires MSL 3.0; curve_data tags require 3.1+.
            opts->setLanguageVersion(MTL::LanguageVersion3_2);

            NS::Error *err = nullptr;
            auto *library = device->newLibrary(src, opts, &err);
            opts->release();

            if (!library) {
                std::string desc = "no error info";
                if (err && err->localizedDescription())
                    desc = err->localizedDescription()->utf8String();
                Throw("scene_metal: failed to compile intersection function "
                      "library: %s", desc.c_str());
            }

            state->intersection_library = library;

            // Build C-string array for the API.
            std::vector<const char *> name_ptrs;
            name_ptrs.reserve(ift_function_names.size());
            for (auto &s : ift_function_names)
                name_ptrs.push_back(s.c_str());

            jit_metal_configure_rt_ex(
                tlas,
                state->resources.data(),
                (uint32_t) state->resources.size(),
                library,
                (uint32_t) ift_function_names.size(),
                name_ptrs.data(),
                ift_data_buffers.data(),
                ift_buffer_slots.data(),
                geom_mask);
        } else if (any_curves) {
            // Curves but no custom shapes — use base RT setup but signal
            // curve presence so drjit-core emits the right intersector tags.
            jit_metal_configure_rt_ex(
                tlas,
                state->resources.data(),
                (uint32_t) state->resources.size(),
                /* library = */ nullptr,
                /* n_ift_entries = */ 0,
                /* names = */ nullptr,
                /* buffers = */ nullptr,
                /* slots = */ nullptr,
                geom_mask);
        } else {
            jit_metal_configure_rt(
                tlas,
                state->resources.data(),
                (uint32_t) state->resources.size());
        }

        size_t n_custom = ift_function_names.size();
        Log(Info, "scene_metal: built acceleration structure (%zu meshes, "
                  "%zu custom shapes, %zu instances)",
            blas_vec.size() - n_custom, n_custom,
            instances.size());
    }
}

MI_VARIANT void Scene<Float, Spectrum>::accel_release_metal() {
    if constexpr (dr::is_metal_v<Float>) {
        if (m_accel) {
            auto *state = (MetalAccelState<UInt32> *) m_accel;
            jit_metal_configure_rt(nullptr, nullptr, 0);

            // Release TLAS
            if (state->tlas)
                ((MTL::AccelerationStructure *) state->tlas)->release();

            // Release BLAS
            for (auto *blas : state->blas_list)
                ((MTL::AccelerationStructure *) blas)->release();

            // Release per-shape buffers WE allocated via device->newBuffer
            // (AABB + primitive-data buffers for custom shapes). The other
            // entries in `state->resources` (mesh vertex/index buffers, BLAS
            // handles, etc.) are managed elsewhere and must not be released
            // here.
            for (auto *buf : state->owned_buffers)
                ((MTL::Buffer *) buf)->release();

            // Release intersection-function library
            if (state->intersection_library)
                ((MTL::Library *) state->intersection_library)->release();

            delete state;
            m_accel = nullptr;
        }
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_metal(const Ray3f &ray,
                                                        Mask active) const {
    if constexpr (dr::is_metal_v<Float>) {
        using Single = dr::float32_array_t<Float>;

        dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_tmin(0.f), ray_tmax(ray.maxt);

        uint32_t args[8] = {
            ray_o.x().index(), ray_o.y().index(), ray_o.z().index(),
            ray_d.x().index(), ray_d.y().index(), ray_d.z().index(),
            ray_tmin.index(), ray_tmax.index()
        };

        uint32_t out[7];
        jit_metal_ray_trace(8, args, active.index(), out, 7);

        // out: [valid, distance, bary_u, bary_v, instance_id, primitive_id, geometry_id]
        Mask valid = Mask::steal(out[0]);

        PreliminaryIntersection3f pi;
        pi.t          = Float(Single::steal(out[1]));
        pi.prim_uv    = Point2f(Float(Single::steal(out[2])),
                                Float(Single::steal(out[3])));
        pi.prim_index = UInt32::steal(out[5]);
        pi.shape_index = UInt32::steal(out[4]); // instance_id → shape index

        // Resolve shape pointer from instance_id via registry
        auto *state = (MetalAccelState<UInt32> *) m_accel;
        ShapePtr shape = dr::gather<UInt32>(state->shapes_registry_ids,
                                           pi.shape_index, valid);
        pi.shape    = shape & valid;
        pi.instance = nullptr;

        // Inactive lanes get infinity
        pi.t[!valid]          = dr::Infinity<Float>;
        pi.prim_uv[!valid]    = dr::zeros<Point2f>();
        pi.prim_index[!valid] = 0;
        pi.shape[!valid]      = nullptr;

        return pi;
    } else {
        Throw("ray_intersect_preliminary_metal() should only be called in Metal mode.");
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_metal(const Ray3f &ray, uint32_t ray_flags,
                                             Mask active) const {
    if constexpr (dr::is_metal_v<Float>) {
        PreliminaryIntersection3f pi = ray_intersect_preliminary_metal(ray, active);
        return pi.compute_surface_interaction(ray, ray_flags, active);
    } else {
        Throw("ray_intersect_metal() should only be called in Metal mode.");
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_metal(const Ray3f &ray, Mask active) const {
    if constexpr (dr::is_metal_v<Float>) {
        using Single = dr::float32_array_t<Float>;

        dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_tmin(0.f), ray_tmax(ray.maxt);

        uint32_t args[8] = {
            ray_o.x().index(), ray_o.y().index(), ray_o.z().index(),
            ray_d.x().index(), ray_d.y().index(), ray_d.z().index(),
            ray_tmin.index(), ray_tmax.index()
        };

        uint32_t out[7];
        jit_metal_ray_trace(8, args, active.index(), out, 7);

        Mask valid = Mask::steal(out[0]);

        // Release unused outputs
        for (int i = 1; i < 7; ++i)
            jit_var_dec_ref(out[i]);

        return valid;
    } else {
        Throw("ray_test_metal() should only be called in Metal mode.");
    }
}

// Stubs for acceleration parameter change (rebuild)
MI_VARIANT void Scene<Float, Spectrum>::accel_parameters_changed_metal() {
    if constexpr (dr::is_metal_v<Float>) {
        accel_release_metal();
        Properties props;
        accel_init_metal(props);
    }
}

MI_VARIANT void Scene<Float, Spectrum>::static_accel_initialization_metal() { }
MI_VARIANT void Scene<Float, Spectrum>::static_accel_shutdown_metal() { }

NAMESPACE_END(mitsuba)
