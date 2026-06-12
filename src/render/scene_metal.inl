/*
    scene_metal.inl -- Metal GPU ray tracing via Metal acceleration structures

    This file contains the templated half of the Metal backend: it gathers
    per-shape geometry into the backend-agnostic description consumed by the
    acceleration structure builder (src/render/metal_accel.mm) and implements
    ray dispatch through jit_metal_ray_trace().

    Copyright (c) 2026 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE file.
*/

#include <drjit-core/metal.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/metal/accel.h>
#include <mitsuba/render/metal/shapes.h>
#include <unordered_map>

NAMESPACE_BEGIN(mitsuba)

/// Metal acceleration structure state, stored in Scene::m_accel
template <typename UInt32>
struct MetalAccelState {
    /// Opaque handle owning the Metal objects (TLAS/BLAS/buffers/library)
    metal_accel::Accel *accel = nullptr;
    /// Dr.Jit variable index returned by jit_metal_configure_scene().
    /// Threaded through jit_metal_ray_trace() to identify *this* scene at
    /// codegen + launch time. Zero for empty scenes.
    uint32_t scene_index = 0;
    /// Per-TLAS-instance registry ID of the owning shape (the Instance
    /// shape for shape-group geometry, the shape itself otherwise)
    DynamicBuffer<UInt32> shapes_registry_ids;
    /// Per-TLAS-instance flag: 1 if the owning shape is an Instance
    DynamicBuffer<UInt32> is_instance_mask;
    /// Per-TLAS-instance offset into ``leaf_idx_table`` (see below)
    DynamicBuffer<UInt32> leaf_idx_offsets;
    /// Flat table mapping (leaf_idx_offsets[instance_id] + geometry_id) to
    /// the leaf shape's index within its ShapeGroup. Only consulted for
    /// instance hits.
    DynamicBuffer<UInt32> leaf_idx_table;
};

/* Sentinel stored in leaf_idx_offsets[i] for TLAS instances that don't
   index into leaf_idx_table (top-level shapes, single-shape Instances).
   Out-of-bounds by construction so that a stray gather surfaces in debug
   mode rather than silently returning garbage. */
static constexpr uint32_t METAL_LEAF_IDX_NONE = (uint32_t) -1;

MI_VARIANT void Scene<Float, Spectrum>::accel_init_metal(const Properties &/*props*/) {
    if constexpr (dr::is_metal_v<Float>) {
        using Mesh = mitsuba::Mesh<Float, Spectrum>;

        auto *state = new MetalAccelState<UInt32>();
        m_accel = state;

        metal_accel::SceneDesc desc;

        // Per-TLAS-instance bookkeeping, built in lock-step with
        // desc.instances. ``instance_id'' values reported by Metal index
        // into these arrays.
        std::vector<uint32_t> tlas_owner_shape_idx;
        std::vector<uint32_t> tlas_is_instance;
        std::vector<uint32_t> tlas_leaf_offset;
        std::vector<uint32_t> leaf_idx_table_host;

        auto push_instance = [&](const metal_accel::InstanceDesc &inst,
                                 uint32_t owner_shape_idx, bool is_instance,
                                 uint32_t leaf_offset) {
            desc.instances.push_back(inst);
            tlas_owner_shape_idx.push_back(owner_shape_idx);
            tlas_is_instance.push_back(is_instance ? 1u : 0u);
            tlas_leaf_offset.push_back(leaf_offset);
        };

        // Describe one shape as a geometry
        auto make_geometry = [&](Shape *shape) -> metal_accel::GeometryDesc {
            metal_accel::GeometryDesc g;

            if (auto *mesh = dynamic_cast<Mesh *>(shape); mesh != nullptr) {
                dr::eval(mesh->vertex_positions_buffer(), mesh->faces_buffer());
                dr::sync_thread();

                /* jit_var_data() returns a new reference to the (evaluated)
                   variable; release it right away — the underlying buffer
                   stays alive through the mesh's own arrays. */
                void *vertex_ptr = nullptr, *index_ptr = nullptr;
                uint32_t v_idx = jit_var_data(
                    mesh->vertex_positions_buffer().index(), &vertex_ptr);
                uint32_t i_idx = jit_var_data(
                    mesh->faces_buffer().index(), &index_ptr);
                jit_var_dec_ref(v_idx);
                jit_var_dec_ref(i_idx);

                g.kind = metal_accel::GeometryDesc::Triangle;
                g.vertex_ptr = vertex_ptr;
                g.index_ptr = index_ptr;
                g.triangle_count = mesh->face_count();
                return g;
            }

            if (int curve_kind = shape->metal_curve_kind(); curve_kind != 0) {
                uint32_t cp_var = 0, idx_var = 0;
                size_t cp_count = 0, seg_count = 0;
                shape->metal_get_curve_data(&cp_var, &cp_count,
                                            &idx_var, &seg_count);
                if (cp_count == 0 || seg_count == 0)
                    Throw("accel_init_metal(): curve shape \"%s\" reports "
                          "zero control points or segments.",
                          std::string(shape->id()).c_str());

                jit_var_eval(cp_var);
                jit_var_eval(idx_var);
                dr::sync_thread();

                void *cp_ptr = nullptr, *idx_ptr = nullptr;
                uint32_t cp_idx = jit_var_data(cp_var, &cp_ptr);
                uint32_t ix_idx = jit_var_data(idx_var, &idx_ptr);
                jit_var_dec_ref(cp_idx);
                jit_var_dec_ref(ix_idx);

                g.kind = metal_accel::GeometryDesc::Curve;
                g.cp_ptr = cp_ptr;
                g.cp_count = cp_count;
                g.seg_index_ptr = idx_ptr;
                g.segment_count = seg_count;
                g.curve_kind = curve_kind;
                return g;
            }

            // Custom (bounding-box) shape
            g.kind = metal_accel::GeometryDesc::BoundingBox;
            g.aabb_count = shape->metal_aabb_count();
            g.pdata_size = shape->metal_primitive_data_size();
            g.total_data_size = shape->metal_total_data_size();
            g.fn_idx = shape->metal_intersection_function_index();
            if (g.aabb_count == 0 || g.total_data_size == 0)
                Throw("accel_init_metal(): custom shape \"%s\" reports zero "
                      "AABBs or no primitive data.",
                      std::string(shape->id()).c_str());
            // The shape outlives the build, so capturing it by pointer is safe
            g.fill_aabbs = [shape](void *out) {
                shape->metal_fill_aabb_data(out);
            };
            g.fill_pdata = [shape](void *out) {
                shape->metal_fill_primitive_data(out);
            };
            return g;
        };

        /* Per-geometry-kind sub-BLAS set for a multi-shape ShapeGroup,
           cached so that several Instances of the same group share it.
           Metal forbids mixing geometry kinds within one primitive
           acceleration structure, hence one BLAS per non-empty kind. */
        struct SubBlas {
            uint32_t blas_idx;
            std::vector<uint32_t> leaf_idx; // geometry_id -> ShapeGroup leaf
        };
        std::unordered_map<const void *, std::vector<SubBlas>> group_cache;

        auto build_group_blases =
            [&](std::vector<ref<Shape>> &children) -> std::vector<SubBlas> {
            metal_accel::BlasDesc per_kind[3];
            std::vector<uint32_t> per_kind_leaf[3];

            for (size_t leaf = 0; leaf < children.size(); ++leaf) {
                metal_accel::GeometryDesc g =
                    make_geometry(children[leaf].get());
                per_kind[(int) g.kind].geoms.push_back(std::move(g));
                per_kind_leaf[(int) g.kind].push_back((uint32_t) leaf);
            }

            std::vector<SubBlas> result;
            for (int kind = 0; kind < 3; ++kind) {
                if (per_kind[kind].geoms.empty())
                    continue;
                result.push_back({ (uint32_t) desc.blases.size(),
                                   std::move(per_kind_leaf[kind]) });
                desc.blases.push_back(std::move(per_kind[kind]));
            }
            return result;
        };

        for (size_t shape_idx = 0; shape_idx < m_shapes.size(); ++shape_idx) {
            Shape *shape = m_shapes[shape_idx].get();

            metal_accel::InstanceDesc inst;
            Shape *effective_shape = shape;
            auto inst_children = shape->metal_instance_children();
            bool is_instance = !inst_children.empty();

            if (is_instance)
                shape->metal_instance_to_world(inst.transform);

            if (inst_children.size() == 1) {
                // Single-shape group: collapse the Instance to its child
                effective_shape = inst_children[0].get();
            } else if (inst_children.size() > 1) {
                // Multi-shape group: one TLAS instance per per-kind sub-BLAS
                const void *sg_id = shape->metal_shapegroup_id();
                auto it = sg_id ? group_cache.find(sg_id) : group_cache.end();
                if (it == group_cache.end())
                    it = group_cache.emplace(
                        sg_id, build_group_blases(inst_children)).first;

                for (const SubBlas &sub : it->second) {
                    uint32_t leaf_offset = (uint32_t) leaf_idx_table_host.size();
                    leaf_idx_table_host.insert(leaf_idx_table_host.end(),
                                               sub.leaf_idx.begin(),
                                               sub.leaf_idx.end());
                    inst.blas_idx = sub.blas_idx;
                    push_instance(inst, (uint32_t) shape_idx,
                                  /*is_instance=*/true, leaf_offset);
                }
                continue;
            }

            // Top-level shape or collapsed single-shape Instance
            metal_accel::BlasDesc blas;
            blas.geoms.push_back(make_geometry(effective_shape));
            inst.blas_idx = (uint32_t) desc.blases.size();
            desc.blases.push_back(std::move(blas));
            push_instance(inst, (uint32_t) shape_idx, is_instance,
                          METAL_LEAF_IDX_NONE);
        }

        if (desc.instances.empty()) {
            /* Leave state->scene_index = 0; the ray-tracing entry points
               below synthesize an all-miss result in that case. */
            Log(Debug, "accel_init_metal(): scene contains no shapes.");
            clear_shapes_dirty();
            return;
        }

        // Per-TLAS-instance lookup tables used at hit-decode time
        {
            size_t n_tlas = desc.instances.size();
            std::unique_ptr<uint32_t[]> reg_ids(new uint32_t[n_tlas]);
            for (size_t i = 0; i < n_tlas; ++i)
                reg_ids[i] = jit_registry_id(m_shapes[tlas_owner_shape_idx[i]]);
            state->shapes_registry_ids =
                dr::load<DynamicBuffer<UInt32>>(reg_ids.get(), n_tlas);
            state->is_instance_mask =
                dr::load<DynamicBuffer<UInt32>>(tlas_is_instance.data(), n_tlas);
            state->leaf_idx_offsets =
                dr::load<DynamicBuffer<UInt32>>(tlas_leaf_offset.data(), n_tlas);
            if (!leaf_idx_table_host.empty())
                state->leaf_idx_table = dr::load<DynamicBuffer<UInt32>>(
                    leaf_idx_table_host.data(), leaf_idx_table_host.size());
            else
                /* Non-empty placeholder: the gather below is masked to
                   instance hits, but the array handle must be valid. */
                state->leaf_idx_table = dr::zeros<DynamicBuffer<UInt32>>(1);
        }

        std::tie(state->accel, state->scene_index) = metal_accel::build(desc);

        /* Mark all shapes as clean so that Scene::parameters_changed() only
           rebuilds the acceleration structure when geometry actually
           changed (matches the Embree/OptiX backends). */
        clear_shapes_dirty();
    }
}

MI_VARIANT void Scene<Float, Spectrum>::accel_parameters_changed_metal() {
    if constexpr (dr::is_metal_v<Float>) {
        accel_release_metal();
        Properties props;
        accel_init_metal(props);
    }
}

MI_VARIANT void Scene<Float, Spectrum>::accel_release_metal() {
    if constexpr (dr::is_metal_v<Float>) {
        if (!m_accel)
            return;
        auto *state = (MetalAccelState<UInt32> *) m_accel;
        metal_accel::release(state->accel, state->scene_index);
        delete state;
        m_accel = nullptr;
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_metal(const Ray3f &ray,
                                                        Mask active) const {
    if constexpr (dr::is_metal_v<Float>) {
        using Single = dr::float32_array_t<Float>;

        auto *state = (MetalAccelState<UInt32> *) m_accel;

        PreliminaryIntersection3f pi = dr::zeros<PreliminaryIntersection3f>();
        if (state->scene_index == 0) // Empty scene: every ray misses
            return pi;

        dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_tmin(0.f), ray_tmax(ray.maxt);

        uint32_t args[8] = {
            ray_o.x().index(), ray_o.y().index(), ray_o.z().index(),
            ray_d.x().index(), ray_d.y().index(), ray_d.z().index(),
            ray_tmin.index(), ray_tmax.index()
        };

        // out: [valid, distance, bary_u, bary_v,
        //       instance_id, primitive_id, geometry_id]
        uint32_t out[7];
        jit_metal_ray_trace(8, args, active.index(), out, 7,
                            state->scene_index);

        Mask valid = Mask::steal(out[0]);

        pi.t          = Float(Single::steal(out[1]));
        pi.prim_uv    = Point2f(Float(Single::steal(out[2])),
                                Float(Single::steal(out[3])));
        pi.prim_index = UInt32::steal(out[5]);

        UInt32 instance_id = UInt32::steal(out[4]);
        UInt32 geometry_id = UInt32::steal(out[6]);

        // Resolve the owning shape via the registry. Several consecutive
        // TLAS instances may map to the same Mitsuba Instance shape (one
        // per per-kind sub-BLAS of its ShapeGroup).
        ShapePtr shape = dr::gather<UInt32>(state->shapes_registry_ids,
                                            instance_id, valid);
        UInt32 is_inst_u32 = dr::gather<UInt32>(state->is_instance_mask,
                                                instance_id, valid);
        Mask is_instance = (is_inst_u32 != 0u) && valid;

        /* For Instance hits, pi.shape_index must identify the leaf within
           ShapeGroup::m_shapes; the per-instance leaf table provides that
           mapping per BLAS geometry. For other hits, set it to instance_id
           for consistency (downstream code does not consume it). */
        UInt32 leaf_offset = dr::gather<UInt32>(state->leaf_idx_offsets,
                                                instance_id, valid);
        UInt32 leaf_idx = dr::gather<UInt32>(state->leaf_idx_table,
                                             leaf_offset + geometry_id,
                                             is_instance);
        pi.shape_index = dr::select(is_instance, leaf_idx, instance_id);

        pi.shape    = shape & valid;
        pi.instance = dr::select(is_instance, shape, dr::zeros<ShapePtr>());

        pi.t[!valid]          = dr::Infinity<Float>;
        pi.prim_uv[!valid]    = dr::zeros<Point2f>();
        pi.prim_index[!valid] = 0;

        return pi;
    } else {
        DRJIT_MARK_USED(ray); DRJIT_MARK_USED(active);
        Throw("ray_intersect_preliminary_metal() should only be called in "
              "Metal mode.");
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_metal(const Ray3f &ray,
                                            uint32_t ray_flags,
                                            Mask active) const {
    if constexpr (dr::is_metal_v<Float>) {
        PreliminaryIntersection3f pi =
            ray_intersect_preliminary_metal(ray, active);
        return pi.compute_surface_interaction(ray, ray_flags, active);
    } else {
        DRJIT_MARK_USED(ray); DRJIT_MARK_USED(ray_flags);
        DRJIT_MARK_USED(active);
        Throw("ray_intersect_metal() should only be called in Metal mode.");
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_metal(const Ray3f &ray, Mask active) const {
    if constexpr (dr::is_metal_v<Float>) {
        using Single = dr::float32_array_t<Float>;

        auto *state = (MetalAccelState<UInt32> *) m_accel;

        if (state->scene_index == 0) // Empty scene: no occluders
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
        for (int i = 1; i < 7; ++i) // Only the hit flag is needed
            jit_var_dec_ref(out[i]);

        return valid;
    } else {
        DRJIT_MARK_USED(ray); DRJIT_MARK_USED(active);
        Throw("ray_test_metal() should only be called in Metal mode.");
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
        /* Traverse the JIT-tracked lookup tables. The raw Metal handles and
           ``scene_index'' (kept alive via its own reference count) live
           outside the JIT graph and are intentionally skipped. */
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
