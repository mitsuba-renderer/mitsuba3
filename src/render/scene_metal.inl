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
#include <unordered_map>
#include <unordered_set>

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

// Pack a 12-float column-major buffer (as produced by
// `Shape::metal_instance_to_world`) into Metal's PackedFloat4x3 layout.
static inline MTL::PackedFloat4x3 mtl_pack_4x3_from_floats(const float t[12]) {
    MTL::PackedFloat4x3 r;
    for (size_t col = 0; col < 4; ++col)
        r.columns[col] = MTL::PackedFloat3{
            t[col * 3 + 0], t[col * 3 + 1], t[col * 3 + 2]
        };
    return r;
}

// Per-shapegroup cached BLAS + IFT bookkeeping. We build the BLAS once per
// unique ShapeGroup (multiple Instances referencing the same group share
// it). The IFT entries for the group's children are also allocated once and
// reused: ``ift_base`` is the first entry's index in the scene-wide IFT
// table, and the per-geometry ``setIntersectionFunctionTableOffset(local)``
// inside the BLAS adds the within-group offset.
// One entry per per-type BLAS that we built for a shape-group. A group
// with mixed shape types (triangles + bounding-boxes + curves) gets one
// entry per non-empty type, since Apple Metal forbids mixing geometry
// types within a single primitive acceleration structure.
struct MetalShapeGroupSubBlas {
    void *blas = nullptr;       // MTL::AccelerationStructure*
    uint32_t blas_idx = 0;      // index in the per-scene blas_vec
    uint32_t ift_base = 0;      // base IFT entry for this BLAS (UINT_MAX if no IFT)
    // For each geometry inside this BLAS, the corresponding leaf index in
    // the ShapeGroup's m_shapes list. Used at ray-intersect time to set
    // pi.shape_index = leaf_idx[geometry_id], which lets ShapeGroup's
    // compute_surface_interaction gather the correct leaf shape.
    std::vector<uint32_t> leaf_idx;
};

struct MetalShapeGroupCache {
    std::vector<MetalShapeGroupSubBlas> sub_blases;
};

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
    /// Per-TLAS-instance → owning shape's registry ID (m_shapes index for
    /// top-level shapes; the Instance's m_shapes index for shape-group
    /// instances, including the multiple sub-BLAS TLAS instances we now
    /// emit per Instance).
    DynamicBuffer<UInt32> shapes_registry_ids;
    /// Per-TLAS-instance bool mask: 1 if the owning shape is an Instance.
    DynamicBuffer<UInt32> is_instance_mask;
    /// Per-TLAS-instance offset into ``leaf_idx_table``. For top-level /
    /// single-shape Instance: offset = (sentinel) and the leaf table is
    /// not consulted. For multi-shape Instance: offset points to a
    /// length = (BLAS geometry count) sub-array that maps geometry_id →
    /// leaf shape index in the ShapeGroup's m_shapes.
    DynamicBuffer<UInt32> leaf_idx_offsets;
    /// Flat lookup table; see ``leaf_idx_offsets``.
    DynamicBuffer<UInt32> leaf_idx_table;
    void *intersection_library = nullptr;  // MTL::Library* with intersection fns
    /// Drjit-core JIT-variable index returned by jit_metal_configure_scene.
    /// Threaded through jit_metal_ray_trace per call to identify *this*
    /// scene at codegen + launch. Released (dec_ref) in accel_release_metal.
    uint32_t scene_index = 0;
    /// Cache: ShapeGroup pointer → per-type BLASes (built lazily, shared
    /// across multiple Instances of the same group).
    std::unordered_map<const void *, MetalShapeGroupCache> shapegroup_cache;
};

// Sentinel value stored in leaf_idx_offsets[i] for TLAS instances that
// don't index into leaf_idx_table (top-level shapes, single-shape
// Instances). Picked to be > any plausible real offset so a stray gather
// would land out-of-bounds and surface as a debug failure rather than
// silently return garbage.
constexpr uint32_t METAL_LEAF_IDX_NONE = (uint32_t) -1;

MI_VARIANT void Scene<Float, Spectrum>::accel_init_metal(const Properties &/*props*/) {
    if constexpr (dr::is_metal_v<Float>) {
        auto *device = (MTL::Device *) jit_metal_context();
        auto *queue  = (MTL::CommandQueue *) jit_metal_command_queue();

        auto *state = new MetalAccelState<UInt32>();
        m_accel = state;

        // Per-TLAS-instance bookkeeping built up alongside ``instances``
        // below. Each entry records which m_shapes index "owns" the TLAS
        // instance, whether that owner is an Instance, and (for multi-
        // shape Instances) the start offset into ``leaf_idx_table_host``.
        // The dr arrays on ``state`` are loaded once at the end of the
        // build, after we know the exact TLAS-instance count.
        std::vector<uint32_t> tlas_owner_shape_idx;
        std::vector<uint32_t> tlas_is_instance;
        std::vector<uint32_t> tlas_leaf_offset;
        std::vector<uint32_t> leaf_idx_table_host;

        // ----------------------------------------------------------------
        //  1. Build BLAS for each mesh
        // ----------------------------------------------------------------
        std::vector<MTL::AccelerationStructure *> blas_vec;
        std::vector<MTL::AccelerationStructureUserIDInstanceDescriptor> instances;

        // Per-IFT-entry tracking for custom-shape BLAS (one entry per BLAS).
        std::vector<std::string> ift_function_names;
        std::vector<void *> ift_data_buffers;
        std::vector<uint32_t> ift_buffer_slots;
        // Per-IFT-entry byte offset into ``ift_data_buffers[i]`` (forwarded
        // to ``setBuffer``'s offset arg). Zero for top-level shapes (the MSL
        // function picks its slice via ``[[user_instance_id]]``); non-zero
        // for shape-group children, which share a single TLAS instance and
        // therefore can't disambiguate via userID.
        std::vector<uint64_t> ift_buffer_offsets;

        // We build TLAS instances in m_shapes order. For top-level / single-
        // shape Instance shapes we still emit ONE TLAS instance per
        // m_shape, preserving the original ``instance_id == m_shapes_index``
        // invariant for those entries. Multi-shape Instances emit MULTIPLE
        // TLAS instances (one per per-type sub-BLAS); the parallel
        // ``tlas_owner_shape_idx`` / ``tlas_leaf_offset`` arrays let us
        // recover the owning shape and leaf index at hit time.
        bool any_custom = false;
        bool any_curves = false;

        // Helper: append a TLAS-instance descriptor and its parallel
        // bookkeeping in lock-step. Always use this rather than pushing
        // to ``instances`` directly so the per-TLAS-instance arrays stay
        // aligned with what Metal returns as ``_hit.instance_id``.
        auto push_tlas_instance =
            [&](const MTL::AccelerationStructureUserIDInstanceDescriptor &inst,
                uint32_t owner_shape_idx, bool is_instance,
                uint32_t leaf_offset) {
                instances.push_back(inst);
                tlas_owner_shape_idx.push_back(owner_shape_idx);
                tlas_is_instance.push_back(is_instance ? 1u : 0u);
                tlas_leaf_offset.push_back(leaf_offset);
            };

        // ----------------------------------------------------------------
        //  Pre-pass: compute per-type total element counts so we can
        //  allocate one combined data buffer per non-SDFGrid custom shape
        //  type. Each TLAS instance later carries a userID equal to its
        //  data offset (in elements) within its type's combined buffer,
        //  which the MSL intersection function reads via [[user_instance_id]]
        //  and uses to index `combined_buf[user_instance_id + primitive_id]`.
        //  This sidesteps Metal's "last write wins" semantics on shared IFT
        //  buffer slots without changing the instance_id ↔ m_shapes_index
        //  invariant.
        // ----------------------------------------------------------------
        size_t per_type_total[metal_shape::INTERSECTION_FN_COUNT] = { 0 };
        size_t per_type_elem_size[metal_shape::INTERSECTION_FN_COUNT] = { 0 };
        // Track ShapeGroup pointers we've already counted so multiple
        // Instances of the same group don't double-add their children.
        std::unordered_set<const void *> seen_groups_count;
        auto count_shape = [&](Shape *eff) {
            uint32_t fn_idx = eff->metal_intersection_function_index();
            if (fn_idx < metal_shape::INTERSECTION_FN_COUNT &&
                fn_idx != metal_shape::INTERSECTION_FN_SDFGRID) {
                size_t pdata_size = eff->metal_primitive_data_size();
                size_t aabb_count = eff->metal_aabb_count();
                if (pdata_size > 0 && aabb_count > 0) {
                    per_type_total[fn_idx] += aabb_count;
                    per_type_elem_size[fn_idx] = pdata_size;
                }
            }
        };
        for (size_t shape_idx = 0; shape_idx < m_shapes.size(); ++shape_idx) {
            auto &shape = m_shapes[shape_idx];
            auto inst_children = shape->metal_instance_children();
            if (inst_children.size() > 1) {
                // Multi-shape shape-group Instance: count each unique
                // group's children exactly once.
                const void *sg_id = shape->metal_shapegroup_id();
                if (sg_id && seen_groups_count.count(sg_id))
                    continue;
                if (sg_id)
                    seen_groups_count.insert(sg_id);
                for (auto &child : inst_children)
                    count_shape(child.get());
            } else {
                // Top-level shape OR single-shape Instance — use the
                // effective leaf shape (current behaviour).
                Shape *eff = shape.get();
                if (inst_children.size() == 1)
                    eff = inst_children[0].get();
                count_shape(eff);
            }
        }
        MTL::Buffer *combined_buf[metal_shape::INTERSECTION_FN_COUNT] = { nullptr };
        size_t per_type_cursor[metal_shape::INTERSECTION_FN_COUNT] = { 0 };
        for (uint32_t t = 0; t < metal_shape::INTERSECTION_FN_COUNT; ++t) {
            if (per_type_total[t] > 0) {
                combined_buf[t] = device->newBuffer(
                    per_type_total[t] * per_type_elem_size[t],
                    MTL::ResourceStorageModeShared);
                state->owned_buffers.push_back(combined_buf[t]);
                state->resources.push_back(combined_buf[t]);
            }
        }

        // SDFGrid is the only type that still binds a per-shape buffer at
        // its IFT slot (its data is shape-level, not per-primitive). A
        // second SDFGrid shape would silently overwrite the first via the
        // IFT's "last write wins" semantics, so we warn in that case.
        bool seen_sdfgrid = false;

        for (size_t shape_idx = 0; shape_idx < m_shapes.size(); ++shape_idx) {
            uint32_t instance_id = (uint32_t) shape_idx;
            auto &shape = m_shapes[shape_idx];

            // For Instance shapes:
            //   * Single-shape shapegroup: collapse the Instance to its
            //     single child and treat the rest of the loop as if that
            //     child were the top-level shape. The instance's
            //     ``to_world`` is captured into ``tlas_transform`` below
            //     and applied at the TLAS instance descriptor.
            //   * Multi-shape shapegroup: build a single multi-geometry
            //     BLAS for the group (cached so multiple Instances of the
            //     same group share it), allocate one IFT entry per
            //     custom-shape child (with a per-entry buffer offset that
            //     points at the child's slice of the per-type combined
            //     buffer, since Metal's [[user_instance_id]] is per
            //     instance and can't disambiguate children of the same
            //     BLAS). Then add the TLAS instance descriptor and
            //     ``continue`` to skip the per-shape branches below.
            Shape *effective_shape = shape.get();
            MTL::PackedFloat4x3 tlas_transform = mtl_identity_4x3();
            auto inst_children = shape->metal_instance_children();
            if (inst_children.size() == 1) {
                effective_shape = inst_children[0].get();
                float to_world[12];
                shape->metal_instance_to_world(to_world);
                tlas_transform = mtl_pack_4x3_from_floats(to_world);
            } else if (inst_children.size() > 1) {
                // -------- Multi-shape shape-group path --------
                //
                // Apple's Metal forbids mixing geometry types within a
                // single primitive acceleration structure (triangle vs
                // bounding-box vs curve). We therefore build a SEPARATE
                // sub-BLAS per non-empty type and emit one TLAS instance
                // per sub-BLAS, all sharing the Instance's transformation.
                // The cache keeps these sub-BLASes per ShapeGroup so that
                // multiple Instances of the same group reuse them.
                Log(Debug, "scene_metal: multi-shape shape-group BLAS for "
                           "Instance \"%s\" (%zu children)",
                    std::string(shape->id()).c_str(), inst_children.size());
                float to_world[12];
                shape->metal_instance_to_world(to_world);
                tlas_transform = mtl_pack_4x3_from_floats(to_world);

                const void *sg_id = shape->metal_shapegroup_id();
                auto cache_it = sg_id ? state->shapegroup_cache.find(sg_id)
                                       : state->shapegroup_cache.end();

                MetalShapeGroupCache local_cache;
                MetalShapeGroupCache *group_cache;
                if (cache_it != state->shapegroup_cache.end()) {
                    group_cache = &cache_it->second;
                } else {
                    // First Instance referencing this group — build per-type
                    // sub-BLASes from scratch.

                    // Group children by geometry kind. Each per-type group
                    // becomes its own sub-BLAS. We also record the leaf
                    // index (position in inst_children, == position in
                    // ShapeGroup::m_shapes) for each geometry inside its
                    // sub-BLAS so the kernel can recover ``pi.shape_index``
                    // from ``_hit.geometry_id`` at intersect time.
                    enum SubKind { ST_TRIANGLE = 0, ST_BB = 1, ST_CURVE = 2 };
                    std::vector<MTL::AccelerationStructureGeometryDescriptor *>
                        per_type_geoms[3];
                    std::vector<uint32_t> per_type_leaf[3];

                    // Per-IFT entries that we'll allocate for the BB
                    // sub-BLAS. Keyed by intersection function index, value
                    // = local IFT index within the sub-BLAS.
                    uint32_t bb_local_ift_idx = 0;
                    std::vector<std::string> bb_ift_names;
                    std::vector<void *>      bb_ift_buffers;
                    std::vector<uint32_t>    bb_ift_slots;
                    std::vector<uint64_t>    bb_ift_offsets;

                    for (size_t child_idx = 0;
                         child_idx < inst_children.size(); ++child_idx) {
                        Shape *child = inst_children[child_idx].get();
                        auto *child_mesh =
                            dynamic_cast<mitsuba::Mesh<Float, Spectrum> *>(child);

                        if (child_mesh) {
                            dr::eval(child_mesh->vertex_positions_buffer(),
                                     child_mesh->faces_buffer());
                            dr::sync_thread();

                            void *vp = nullptr, *ip = nullptr;
                            uint32_t vbuf_idx = jit_var_data(
                                child_mesh->vertex_positions_buffer().index(), &vp);
                            uint32_t ibuf_idx = jit_var_data(
                                child_mesh->faces_buffer().index(), &ip);

                            size_t v_off = 0, i_off = 0;
                            auto *v_buf = (MTL::Buffer *)
                                jit_metal_lookup_buffer(vp, &v_off);
                            auto *i_buf = (MTL::Buffer *)
                                jit_metal_lookup_buffer(ip, &i_off);

                            jit_var_dec_ref(vbuf_idx);
                            jit_var_dec_ref(ibuf_idx);

                            if (!v_buf || !i_buf)
                                Throw("scene_metal: could not find Metal "
                                      "buffers for mesh shape inside "
                                      "shape-group of Instance \"%s\".",
                                      std::string(shape->id()).c_str());

                            auto *gd = MTL::AccelerationStructureTriangleGeometryDescriptor
                                ::alloc()->init();
                            gd->setVertexBuffer(v_buf);
                            gd->setVertexBufferOffset(v_off);
                            gd->setVertexStride(sizeof(float) * 3);
                            gd->setVertexFormat(MTL::AttributeFormatFloat3);
                            gd->setIndexBuffer(i_buf);
                            gd->setIndexBufferOffset(i_off);
                            gd->setIndexType(MTL::IndexTypeUInt32);
                            gd->setTriangleCount(child_mesh->face_count());
                            per_type_geoms[ST_TRIANGLE].push_back(gd);
                            per_type_leaf[ST_TRIANGLE].push_back(
                                (uint32_t) child_idx);
                            state->resources.push_back(v_buf);
                            state->resources.push_back(i_buf);
                            continue;
                        }

                        int curve_kind = child->metal_curve_kind();
                        if (curve_kind != 0) {
                            uint32_t cp_var = 0, idx_var = 0;
                            size_t cp_count = 0, seg_count = 0;
                            child->metal_get_curve_data(
                                &cp_var, &cp_count, &idx_var, &seg_count);
                            if (cp_count == 0 || seg_count == 0)
                                Throw("scene_metal: curve shape inside "
                                      "shape-group of \"%s\" reports zero "
                                      "control points / segments.",
                                      std::string(shape->id()).c_str());

                            jit_var_eval(cp_var);
                            jit_var_eval(idx_var);
                            dr::sync_thread();

                            void *cp_ptr = nullptr, *idx_ptr = nullptr;
                            uint32_t cp_data_idx  = jit_var_data(cp_var, &cp_ptr);
                            uint32_t idx_data_idx = jit_var_data(idx_var, &idx_ptr);

                            size_t cp_off = 0, idx_off = 0;
                            auto *cp_buf = (MTL::Buffer *)
                                jit_metal_lookup_buffer(cp_ptr, &cp_off);
                            auto *ix_buf = (MTL::Buffer *)
                                jit_metal_lookup_buffer(idx_ptr, &idx_off);

                            jit_var_dec_ref(cp_data_idx);
                            jit_var_dec_ref(idx_data_idx);

                            if (!cp_buf || !ix_buf)
                                Throw("scene_metal: could not find Metal "
                                      "buffers for curve shape inside "
                                      "shape-group of \"%s\".",
                                      std::string(shape->id()).c_str());
                            any_curves = true;

                            auto *gd = MTL::AccelerationStructureCurveGeometryDescriptor
                                ::alloc()->init();
                            gd->setControlPointBuffer(cp_buf);
                            gd->setControlPointBufferOffset(cp_off);
                            gd->setControlPointFormat(MTL::AttributeFormatFloat3);
                            gd->setControlPointStride(4 * sizeof(float));
                            gd->setControlPointCount(cp_count);
                            gd->setRadiusBuffer(cp_buf);
                            gd->setRadiusBufferOffset(cp_off + 3 * sizeof(float));
                            gd->setRadiusFormat(MTL::AttributeFormatFloat);
                            gd->setRadiusStride(4 * sizeof(float));
                            gd->setIndexBuffer(ix_buf);
                            gd->setIndexBufferOffset(idx_off);
                            gd->setIndexType(MTL::IndexTypeUInt32);
                            gd->setSegmentCount(seg_count);
                            gd->setSegmentControlPointCount(curve_kind == 2 ? 4 : 2);
                            gd->setCurveBasis(
                                curve_kind == 2 ? MTL::CurveBasisBSpline
                                                : MTL::CurveBasisLinear);
                            gd->setCurveType(MTL::CurveTypeRound);
                            gd->setCurveEndCaps(MTL::CurveEndCapsSphere);
                            per_type_geoms[ST_CURVE].push_back(gd);
                            per_type_leaf[ST_CURVE].push_back(
                                (uint32_t) child_idx);
                            state->resources.push_back(cp_buf);
                            state->resources.push_back(ix_buf);
                            continue;
                        }

                        // ---- Custom (bounding-box) child ----
                        size_t n_aabb = child->metal_aabb_count();
                        size_t pdata_size = child->metal_primitive_data_size();
                        size_t total_data_size = child->metal_total_data_size();
                        uint32_t fn_idx = child->metal_intersection_function_index();
                        (void) pdata_size;
                        if (n_aabb == 0 || total_data_size == 0)
                            Throw("scene_metal: custom shape inside shape-"
                                  "group of \"%s\" reports zero AABBs / "
                                  "no data.",
                                  std::string(shape->id()).c_str());
                        any_custom = true;

                        auto *aabb_buf = device->newBuffer(
                            n_aabb * 6 * sizeof(float),
                            MTL::ResourceStorageModeShared);
                        child->metal_fill_aabb_data(aabb_buf->contents());

                        MTL::Buffer *child_pdata_buf = nullptr;
                        uint64_t child_byte_offset = 0;
                        uint32_t child_inst_user_id = 0;
                        if (fn_idx == metal_shape::INTERSECTION_FN_SDFGRID) {
                            child_pdata_buf = device->newBuffer(
                                total_data_size, MTL::ResourceStorageModeShared);
                            child->metal_fill_primitive_data(
                                child_pdata_buf->contents());
                        } else if (fn_idx < metal_shape::INTERSECTION_FN_COUNT &&
                                   combined_buf[fn_idx]) {
                            child_inst_user_id =
                                (uint32_t) per_type_cursor[fn_idx];
                            uint8_t *dst =
                                (uint8_t *) combined_buf[fn_idx]->contents()
                                + child_inst_user_id * per_type_elem_size[fn_idx];
                            child->metal_fill_primitive_data(dst);
                            per_type_cursor[fn_idx] += n_aabb;
                            child_byte_offset =
                                (uint64_t) child_inst_user_id *
                                (uint64_t) per_type_elem_size[fn_idx];
                        }

                        auto *gd = MTL::AccelerationStructureBoundingBoxGeometryDescriptor
                            ::alloc()->init();
                        gd->setBoundingBoxBuffer(aabb_buf);
                        gd->setBoundingBoxBufferOffset(0);
                        gd->setBoundingBoxStride(6 * sizeof(float));
                        gd->setBoundingBoxCount(n_aabb);
                        // Local IFT index within the BB sub-BLAS — sequential
                        // among BB children.
                        gd->setIntersectionFunctionTableOffset(bb_local_ift_idx);
                        per_type_geoms[ST_BB].push_back(gd);
                        per_type_leaf[ST_BB].push_back((uint32_t) child_idx);

                        // IFT entry for this BB child. We collect them in
                        // ``bb_ift_*`` and prepend to the scene-wide
                        // ``ift_*`` arrays once we know the BB sub-BLAS's
                        // base IFT slot (assigned below).
                        bb_ift_names.emplace_back(
                            metal_shape::kFunctionNames[fn_idx]);
                        if (fn_idx == metal_shape::INTERSECTION_FN_SDFGRID) {
                            bb_ift_buffers.push_back(child_pdata_buf);
                            bb_ift_slots.push_back(fn_idx);
                            bb_ift_offsets.push_back(0ull);
                            state->resources.push_back(child_pdata_buf);
                            state->owned_buffers.push_back(child_pdata_buf);
                        } else if (combined_buf[fn_idx]) {
                            bb_ift_buffers.push_back(combined_buf[fn_idx]);
                            bb_ift_slots.push_back(fn_idx);
                            bb_ift_offsets.push_back(child_byte_offset);
                        } else {
                            bb_ift_buffers.push_back(nullptr);
                            bb_ift_slots.push_back(0u);
                            bb_ift_offsets.push_back(0ull);
                        }
                        ++bb_local_ift_idx;

                        state->resources.push_back(aabb_buf);
                        state->owned_buffers.push_back(aabb_buf);
                    }

                    // Build sub-BLASes per non-empty type.
                    auto build_sub_blas = [&](SubKind kind, uint32_t ift_base) {
                        auto &geoms = per_type_geoms[(int) kind];
                        if (geoms.empty()) return;

                        NS::Array *garr = NS::Array::array(
                            (const NS::Object *const *) geoms.data(),
                            geoms.size());
                        auto *bdesc =
                            MTL::PrimitiveAccelerationStructureDescriptor::alloc()->init();
                        bdesc->setGeometryDescriptors(garr);
                        auto sizes = device->accelerationStructureSizes(bdesc);
                        auto *blas = device->newAccelerationStructure(
                            sizes.accelerationStructureSize);
                        auto *scratch = device->newBuffer(
                            sizes.buildScratchBufferSize,
                            MTL::ResourceStorageModePrivate);
                        auto *cb = queue->commandBuffer();
                        auto *enc = cb->accelerationStructureCommandEncoder();
                        enc->buildAccelerationStructure(blas, bdesc, scratch, 0);
                        enc->endEncoding();
                        cb->commit();
                        cb->waitUntilCompleted();
                        scratch->release();
                        bdesc->release();
                        for (auto *gd : geoms)
                            gd->release();

                        uint32_t blas_idx = (uint32_t) blas_vec.size();
                        blas_vec.push_back(blas);
                        state->blas_list.push_back(blas);
                        state->resources.push_back(blas);

                        local_cache.sub_blases.push_back(
                            { blas, blas_idx, ift_base,
                              per_type_leaf[(int) kind] });
                    };

                    // Triangle / curve sub-BLASes use IFT base = 0 (no
                    // custom IFT). Allocate the BB sub-BLAS's IFT entries
                    // contiguously starting at the current scene-wide
                    // ift_function_names size.
                    build_sub_blas(ST_TRIANGLE, 0);
                    build_sub_blas(ST_CURVE,    0);

                    if (!per_type_geoms[ST_BB].empty()) {
                        uint32_t bb_ift_base =
                            (uint32_t) ift_function_names.size();
                        for (size_t i = 0; i < bb_ift_names.size(); ++i) {
                            ift_function_names.push_back(bb_ift_names[i]);
                            ift_data_buffers.push_back(bb_ift_buffers[i]);
                            ift_buffer_slots.push_back(bb_ift_slots[i]);
                            ift_buffer_offsets.push_back(bb_ift_offsets[i]);
                        }
                        build_sub_blas(ST_BB, bb_ift_base);
                    }

                    if (sg_id)
                        state->shapegroup_cache[sg_id] = local_cache;
                    group_cache = sg_id
                        ? &state->shapegroup_cache[sg_id]
                        : &local_cache;
                }

                // Emit one TLAS instance per sub-BLAS. Each gets the same
                // ``transformationMatrix``; ``leaf_offset`` indexes into
                // ``leaf_idx_table_host`` so the hit decoder can recover
                // pi.shape_index from geometry_id.
                for (auto &sub : group_cache->sub_blases) {
                    uint32_t leaf_offset =
                        (uint32_t) leaf_idx_table_host.size();
                    leaf_idx_table_host.insert(leaf_idx_table_host.end(),
                                               sub.leaf_idx.begin(),
                                               sub.leaf_idx.end());

                    MTL::AccelerationStructureUserIDInstanceDescriptor inst = {};
                    inst.transformationMatrix             = tlas_transform;
                    inst.options                          = MTL::AccelerationStructureInstanceOptionOpaque;
                    inst.mask                             = 0xFF;
                    inst.intersectionFunctionTableOffset  = sub.ift_base;
                    inst.accelerationStructureIndex       = sub.blas_idx;
                    inst.userID                           = 0;
                    push_tlas_instance(
                        inst, (uint32_t) shape_idx, /*is_instance=*/true,
                        leaf_offset);
                }
                continue;
            }

            auto *mesh = dynamic_cast<mitsuba::Mesh<Float, Spectrum> *>(effective_shape);

            if (!mesh) {
                // ============================================================
                //  Native curve BLAS (Metal HW curves, macOS 14+)
                // ============================================================
                int curve_kind = effective_shape->metal_curve_kind();
                if (curve_kind != 0) {
                    Shape *shape_ptr = effective_shape;
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

                    MTL::AccelerationStructureUserIDInstanceDescriptor inst = {};
                    inst.transformationMatrix             = tlas_transform;
                    inst.options                          = MTL::AccelerationStructureInstanceOptionOpaque;
                    inst.mask                             = 0xFF;
                    inst.intersectionFunctionTableOffset  = 0;
                    inst.accelerationStructureIndex       = (uint32_t) (blas_vec.size() - 1);
                    inst.userID                           = 0;
                    push_tlas_instance(
                        inst, (uint32_t) shape_idx,
                        /*is_instance=*/!inst_children.empty(),
                        METAL_LEAF_IDX_NONE);

                    continue;
                }

                // ------------------------------------------------------------
                //  Custom (bounding-box) shape BLAS
                // ------------------------------------------------------------
                Shape *shape_ptr = effective_shape;
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

                if (fn_idx == metal_shape::INTERSECTION_FN_SDFGRID) {
                    if (seen_sdfgrid)
                        Log(Warn, "scene_metal: scene has multiple SDFGrid "
                                  "shapes. SDFGrid still binds its data at "
                                  "IFT buffer slot %u (its data is shape-"
                                  "level, not per-primitive), and Metal's "
                                  "IFT buffer bindings are global to the "
                                  "table, so all SDFGrids will read from "
                                  "the LATER shape's data. Use a single "
                                  "SDFGrid for now; per-primitive shape-"
                                  "pointer support is a TODO.",
                                  metal_shape::INTERSECTION_FN_SDFGRID);
                    seen_sdfgrid = true;
                }

                auto *aabb_buf = device->newBuffer(
                    n_aabb * 6 * sizeof(float),
                    MTL::ResourceStorageModeShared);
                shape_ptr->metal_fill_aabb_data(aabb_buf->contents());

                // Per-shape pdata_buf is only used for the SDFGrid path
                // (which still binds it at the IFT slot). For the four
                // [[user_instance_id]]-driven types, the per-shape data is
                // copied into the type's combined buffer instead, and we
                // record the per-instance offset (in elements) for the
                // TLAS userID below.
                MTL::Buffer *pdata_buf = nullptr;
                uint32_t inst_user_id = 0;
                if (fn_idx == metal_shape::INTERSECTION_FN_SDFGRID) {
                    pdata_buf = device->newBuffer(
                        total_data_size, MTL::ResourceStorageModeShared);
                    shape_ptr->metal_fill_primitive_data(pdata_buf->contents());
                } else if (fn_idx < metal_shape::INTERSECTION_FN_COUNT &&
                           combined_buf[fn_idx]) {
                    inst_user_id = (uint32_t) per_type_cursor[fn_idx];
                    uint8_t *dst =
                        (uint8_t *) combined_buf[fn_idx]->contents()
                        + inst_user_id * per_type_elem_size[fn_idx];
                    shape_ptr->metal_fill_primitive_data(dst);
                    per_type_cursor[fn_idx] += n_aabb;
                }

                auto *geom_desc =
                    MTL::AccelerationStructureBoundingBoxGeometryDescriptor::alloc()->init();
                geom_desc->setBoundingBoxBuffer(aabb_buf);
                geom_desc->setBoundingBoxBufferOffset(0);
                geom_desc->setBoundingBoxStride(6 * sizeof(float));
                geom_desc->setBoundingBoxCount(n_aabb);
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
                state->resources.push_back(blas);
                state->owned_buffers.push_back(aabb_buf);
                if (pdata_buf) {
                    // SDFGrid path only: we allocated a per-shape pdata_buf
                    // above. Track it for release + IFT binding.
                    state->resources.push_back(pdata_buf);
                    state->owned_buffers.push_back(pdata_buf);
                }

                uint32_t ift_entry_idx = (uint32_t) ift_function_names.size();
                ift_function_names.emplace_back(
                    metal_shape::kFunctionNames[fn_idx]);

                // Per-IFT-entry buffer binding:
                //   * SDFGrid: per-shape buffer at slot 4 (still has the
                //     last-write-wins limitation if multiple SDFGrids exist).
                //   * Other types: the type's combined buffer at the type's
                //     slot. The MSL function picks its region via
                //     [[user_instance_id]] (set on the TLAS descriptor).
                if (fn_idx == metal_shape::INTERSECTION_FN_SDFGRID) {
                    ift_data_buffers.push_back(pdata_buf);
                    ift_buffer_slots.push_back(fn_idx);
                    ift_buffer_offsets.push_back(0ull);
                } else if (combined_buf[fn_idx]) {
                    ift_data_buffers.push_back(combined_buf[fn_idx]);
                    ift_buffer_slots.push_back(fn_idx);
                    ift_buffer_offsets.push_back(0ull);
                } else {
                    ift_data_buffers.push_back(nullptr);
                    ift_buffer_slots.push_back(0u);
                    ift_buffer_offsets.push_back(0ull);
                }

                MTL::AccelerationStructureUserIDInstanceDescriptor inst = {};
                inst.transformationMatrix             = tlas_transform;
                inst.options                          = MTL::AccelerationStructureInstanceOptionOpaque;
                inst.mask                             = 0xFF;
                inst.intersectionFunctionTableOffset  = ift_entry_idx;
                inst.accelerationStructureIndex       = (uint32_t) (blas_vec.size() - 1);
                inst.userID                           = inst_user_id;
                push_tlas_instance(
                    inst, (uint32_t) shape_idx,
                    /*is_instance=*/!inst_children.empty(),
                    METAL_LEAF_IDX_NONE);

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
            MTL::AccelerationStructureUserIDInstanceDescriptor inst = {};
            inst.transformationMatrix             = tlas_transform;
            inst.options                          = MTL::AccelerationStructureInstanceOptionOpaque;
            inst.mask                             = 0xFF;
            inst.intersectionFunctionTableOffset  = 0;
            inst.accelerationStructureIndex       = (uint32_t) (blas_vec.size() - 1);
            inst.userID                           = 0;  // unused by triangle BLASes
            push_tlas_instance(
                inst, (uint32_t) shape_idx,
                /*is_instance=*/!inst_children.empty(),
                METAL_LEAF_IDX_NONE);
        }

        if (instances.empty()) {
            Log(Warn, "scene_metal: no meshes found, ray tracing will not work.");
            // No scene to configure; leave state->scene_index = 0 so any
            // subsequent ray-trace calls fail loudly rather than silently
            // returning stale results from another scene's kernel.
            return;
        }

        // ----------------------------------------------------------------
        //  Build per-TLAS-instance lookup arrays from the metadata we
        //  collected alongside ``instances``. These are gathered in
        //  ray_intersect_preliminary_metal to (a) resolve the owning
        //  shape via the registry, (b) detect Instance hits, and (c)
        //  recover pi.shape_index via leaf_idx_table for multi-shape
        //  Instances.
        // ----------------------------------------------------------------
        {
            size_t n_tlas = instances.size();
            std::unique_ptr<uint32_t[]> reg_ids(new uint32_t[n_tlas]);
            for (size_t i = 0; i < n_tlas; ++i)
                reg_ids[i] = jit_registry_id(
                    m_shapes[tlas_owner_shape_idx[i]]);
            state->shapes_registry_ids =
                dr::load<DynamicBuffer<UInt32>>(reg_ids.get(), n_tlas);
            state->is_instance_mask =
                dr::load<DynamicBuffer<UInt32>>(tlas_is_instance.data(),
                                                n_tlas);
            state->leaf_idx_offsets =
                dr::load<DynamicBuffer<UInt32>>(tlas_leaf_offset.data(),
                                                n_tlas);
            if (!leaf_idx_table_host.empty())
                state->leaf_idx_table =
                    dr::load<DynamicBuffer<UInt32>>(
                        leaf_idx_table_host.data(),
                        leaf_idx_table_host.size());
            else
                // Allocate a 1-element placeholder so dr::gather doesn't
                // see an empty array (the gather is masked to instance
                // hits anyway, but the array must be non-empty for the
                // handle to be valid).
                state->leaf_idx_table =
                    dr::zeros<DynamicBuffer<UInt32>>(1);
        }

        // ----------------------------------------------------------------
        //  2. Build TLAS (instance acceleration structure)
        // ----------------------------------------------------------------
        auto *inst_buf = device->newBuffer(
            instances.data(),
            instances.size() * sizeof(MTL::AccelerationStructureUserIDInstanceDescriptor),
            MTL::ResourceStorageModeShared);

        // Create BLAS reference array
        NS::Array *blas_array = NS::Array::array(
            (const NS::Object *const *) blas_vec.data(), blas_vec.size());

        auto *tlas_desc =
            MTL::InstanceAccelerationStructureDescriptor::alloc()->init();
        tlas_desc->setInstancedAccelerationStructures(blas_array);
        tlas_desc->setInstanceDescriptorBuffer(inst_buf);
        tlas_desc->setInstanceCount(instances.size());
        // We pass UserID-extended descriptors so each instance can carry a
        // ``userID`` that the MSL intersection function reads as
        // [[user_instance_id]] (its data offset within the type's combined
        // buffer — see the pre-pass at the top of accel_init_metal).
        tlas_desc->setInstanceDescriptorType(
            MTL::AccelerationStructureInstanceDescriptorTypeUserID);

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
        //     and register a per-scene drjit handle that subsequent
        //     ray-trace calls thread through to identify *this* scene at
        //     codegen + launch time.
        // ----------------------------------------------------------------
        // Geometry-types mask: bit 0 = triangle, bit 1 = bounding_box,
        // bit 2 = curves. drjit uses this to choose the intersector<...>
        // template tags.
        uint32_t geom_mask = 0x1u;                  // triangle always present
        if (any_custom)  geom_mask |= 0x2u;         // bounding_box (custom)
        if (any_curves)  geom_mask |= 0x4u;         // native curves

        MTL::Library *library = nullptr;
        std::vector<const char *> name_ptrs;
        if (any_custom) {
            // Compile our embedded MSL source into an MTL::Library. drjit
            // will link the named [[intersection(...)]] functions into
            // ray-tracing pipelines via MTLLinkedFunctions.
            NS::String *src = NS::String::string(
                metal_shape::kIntersectionFunctionsMSL,
                NS::StringEncoding::UTF8StringEncoding);
            auto *opts = MTL::CompileOptions::alloc()->init();
            // The [[primitive_data]] attribute on intersection-function
            // parameters requires MSL 3.0; curve_data tags require 3.1+.
            opts->setLanguageVersion(MTL::LanguageVersion3_2);

            NS::Error *err = nullptr;
            library = device->newLibrary(src, opts, &err);
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
            name_ptrs.reserve(ift_function_names.size());
            for (auto &s : ift_function_names)
                name_ptrs.push_back(s.c_str());
        }

        state->scene_index = jit_metal_configure_scene(
            tlas,
            state->resources.data(),
            (uint32_t) state->resources.size(),
            library,
            (uint32_t) ift_function_names.size(),
            name_ptrs.empty() ? nullptr : name_ptrs.data(),
            ift_data_buffers.empty() ? nullptr : ift_data_buffers.data(),
            ift_buffer_slots.empty() ? nullptr : ift_buffer_slots.data(),
            ift_buffer_offsets.empty() ? nullptr : ift_buffer_offsets.data(),
            geom_mask);

        size_t n_custom = ift_function_names.size();
        Log(Info, "scene_metal: built acceleration structure (%zu meshes, "
                  "%zu custom shapes, %zu instances)",
            blas_vec.size() - n_custom, n_custom,
            instances.size());

        // Mark all shapes as clean now that they've been incorporated into
        // the freshly-built acceleration structure. Without this, every
        // call to Scene::parameters_changed() would see dirty shapes and
        // trigger a full TLAS rebuild — even when only a BSDF parameter
        // changed. (Embree, Native, and OptiX backends all do this.)
        clear_shapes_dirty();
    }
}

MI_VARIANT void Scene<Float, Spectrum>::accel_release_metal() {
    if constexpr (dr::is_metal_v<Float>) {
        if (m_accel) {
            auto *state = (MetalAccelState<UInt32> *) m_accel;

            // Pre-emptively null out the MetalScene's TLAS pointer. This
            // is a no-op for the common case (the dec_ref below is the
            // last ref → the MetalScene is freed). However, frozen-
            // function recordings retain references on ``scene_index``
            // (via captured TraceRay nodes' dep[1]), keeping the
            // MetalScene alive past this point. Without this null-out
            // its ``tlas`` field would be a dangling pointer to a
            // released MTL::AccelerationStructure, causing a use-after-
            // free in ``setAccelerationStructure`` at the next
            // frozen-function replay.
            if (state->scene_index)
                jit_metal_invalidate_scene_tlas(state->scene_index);

            // Release the per-scene drjit handle. This drops drjit's
            // last (Mitsuba-side) reference on the MetalScene; its
            // destruction callback releases any cached IFTs and our
            // retain on the intersection-function library. Mitsuba
            // then releases the raw Metal objects (TLAS / BLAS / owned
            // buffers) below.
            if (state->scene_index) {
                jit_var_dec_ref(state->scene_index);
                state->scene_index = 0;
            }

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

        auto *state = (MetalAccelState<UInt32> *) m_accel;

        // Empty scene: nothing to intersect. Return an all-miss
        // PreliminaryIntersection. Embree behaves this way silently for
        // shape-less scenes; the Metal accel-build skips TLAS construction
        // when there are no meshes (state->scene_index == 0), so we have to
        // synthesize the miss result here. Without this short-circuit
        // ``jit_metal_ray_trace`` raises because scene == 0.
        if (state->scene_index == 0) {
            PreliminaryIntersection3f pi;
            pi.t           = dr::Infinity<Float>;
            pi.prim_uv     = dr::zeros<Point2f>();
            pi.prim_index  = 0;
            pi.shape_index = 0;
            pi.shape       = nullptr;
            pi.instance    = nullptr;
            return pi;
        }

        dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_tmin(0.f), ray_tmax(ray.maxt);

        uint32_t args[8] = {
            ray_o.x().index(), ray_o.y().index(), ray_o.z().index(),
            ray_d.x().index(), ray_d.y().index(), ray_d.z().index(),
            ray_tmin.index(), ray_tmax.index()
        };

        uint32_t out[7];
        jit_metal_ray_trace(8, args, active.index(), out, 7,
                            state->scene_index);

        // out: [valid, distance, bary_u, bary_v, instance_id, primitive_id, geometry_id]
        Mask valid = Mask::steal(out[0]);

        PreliminaryIntersection3f pi;
        pi.t          = Float(Single::steal(out[1]));
        pi.prim_uv    = Point2f(Float(Single::steal(out[2])),
                                Float(Single::steal(out[3])));
        pi.prim_index = UInt32::steal(out[5]);

        UInt32 instance_id = UInt32::steal(out[4]);
        UInt32 geometry_id = UInt32::steal(out[6]);

        // Resolve shape pointer from instance_id via registry. After the
        // multi-shape shape-group rework ``instance_id`` indexes per-TLAS-
        // instance arrays (which can have multiple consecutive entries
        // pointing at the same Mitsuba Instance shape — one per per-type
        // sub-BLAS).
        ShapePtr shape = dr::gather<UInt32>(state->shapes_registry_ids,
                                           instance_id, valid);
        UInt32 is_inst_u32 = dr::gather<UInt32>(state->is_instance_mask,
                                                instance_id, valid);
        Mask is_instance = (is_inst_u32 != 0u) & valid;

        // For Instance hits, ``pi.shape_index`` must be the leaf's index
        // inside ShapeGroup::m_shapes. For single-geometry sub-BLASes the
        // table simply records that one leaf index; for multi-geometry
        // sub-BLASes we add ``geometry_id`` to land on the right entry.
        // For non-Instance hits the registry already returned the right
        // shape and ``shape_index`` is unused by the downstream code, but
        // we still set it to ``instance_id`` for consistency with the
        // (pre-multi-shape) behaviour.
        UInt32 leaf_offset = dr::gather<UInt32>(state->leaf_idx_offsets,
                                                instance_id, valid);
        UInt32 leaf_idx = dr::gather<UInt32>(
            state->leaf_idx_table, leaf_offset + geometry_id,
            is_instance);
        pi.shape_index = dr::select(is_instance, leaf_idx, instance_id);

        pi.shape    = shape & valid;
        pi.instance = dr::select(is_instance, shape,
                                 dr::zeros<ShapePtr>());

        // Inactive lanes get infinity
        pi.t[!valid]          = dr::Infinity<Float>;
        pi.prim_uv[!valid]    = dr::zeros<Point2f>();
        pi.prim_index[!valid] = 0;
        pi.shape[!valid]      = nullptr;
        pi.instance[!valid]   = nullptr;

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

        auto *state = (MetalAccelState<UInt32> *) m_accel;

        // Empty scene: no occluders, every ray is unobstructed.
        if (state->scene_index == 0)
            return dr::zeros<Mask>(dr::width(ray.o));

        dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_tmin(0.f), ray_tmax(ray.maxt);

        uint32_t args[8] = {
            ray_o.x().index(), ray_o.y().index(), ray_o.z().index(),
            ray_d.x().index(), ray_d.y().index(), ray_d.z().index(),
            ray_tmin.index(), ray_tmax.index()
        };

        uint32_t out[7];
        jit_metal_ray_trace(8, args, active.index(), out, 7,
                            state->scene_index);

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

MI_VARIANT void Scene<Float, Spectrum>::traverse_1_cb_ro_metal(
    void *payload, drjit::detail::traverse_callback_ro fn) const {
    if constexpr (dr::is_metal_v<Float>) {
        if (!m_accel)
            return;
        auto &s = *(MetalAccelState<UInt32> *) m_accel;
        // Traverse the JIT-tracked DynamicBuffer fields. The raw Metal
        // pointers (TLAS / BLAS / library handles) and ``scene_index``
        // (a single drjit-core variable index, kept alive via ref-count)
        // are owned outside the JIT graph and intentionally skipped.
        drjit::traverse_1_fn_ro(s.shapes_registry_ids, payload, fn);
        drjit::traverse_1_fn_ro(s.is_instance_mask,    payload, fn);
        drjit::traverse_1_fn_ro(s.leaf_idx_offsets,    payload, fn);
        drjit::traverse_1_fn_ro(s.leaf_idx_table,      payload, fn);
    }
}

MI_VARIANT void Scene<Float, Spectrum>::traverse_1_cb_rw_metal(
    void *payload, drjit::detail::traverse_callback_rw fn) {
    if constexpr (dr::is_metal_v<Float>) {
        if (!m_accel)
            return;
        auto &s = *(MetalAccelState<UInt32> *) m_accel;
        drjit::traverse_1_fn_rw(s.shapes_registry_ids, payload, fn);
        drjit::traverse_1_fn_rw(s.is_instance_mask,    payload, fn);
        drjit::traverse_1_fn_rw(s.leaf_idx_offsets,    payload, fn);
        drjit::traverse_1_fn_rw(s.leaf_idx_table,      payload, fn);
    }
}

NAMESPACE_END(mitsuba)
