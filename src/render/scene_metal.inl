/*
    scene_metal.inl -- Metal GPU ray tracing via Metal acceleration structures

    This file contains the templated half of the Metal backend: it gathers
    per-shape geometry into the backend-agnostic description consumed by the
    acceleration structure builder (src/render/metal_accel.mm) and implements
    ray dispatch through jit_metal_ray_trace().

    The JIT-tracked state and entry-point declarations live in
    include/mitsuba/render/accel_metal.h (MetalAccel<Float, Spectrum>, the
    by-value Scene::m_accel member for metal_* variants).

    Copyright (c) 2026 Wenzel Jakob

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE file.
*/

#include <drjit-core/metal.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/metal/accel.h>
#include <mitsuba/render/metal/shapes.h>
#include <mitsuba/render/scene.h>
#include <unordered_map>

NAMESPACE_BEGIN(mitsuba)

/* Sentinel stored in leaf_idx_offsets[i] for TLAS instances that don't
   index into leaf_idx_table (top-level shapes, single-shape Instances).
   Out-of-bounds by construction so that a stray gather surfaces in debug
   mode rather than silently returning garbage. */
static constexpr uint32_t METAL_LEAF_IDX_NONE = (uint32_t) -1;

/**
 * Gather the scene's shapes into the description consumed by the Metal
 * acceleration structure builder (src/render/metal_accel.mm), and (re)build
 * the per-TLAS-instance lookup tables on \c state. Returns a description
 * with no instances when the scene contains no shapes.
 */
template <typename Float, typename Spectrum>
static metal_accel::SceneDesc metal_build_scene_desc(
    std::vector<ref<Shape<Float, Spectrum>>> &shapes,
    MetalAccel<Float, Spectrum> *state) {
    using Shape  = mitsuba::Shape<Float, Spectrum>;
    using Mesh   = mitsuba::Mesh<Float, Spectrum>;
    using UInt32 = dr::uint32_array_t<Float>;

    metal_accel::SceneDesc desc;

    // Per-TLAS-instance bookkeeping, built in lock-step with
    // desc.instances. ``instance_id'' values reported by Metal index
    // into these arrays.
    std::vector<uint32_t> tlas_owner_shape_idx;
    std::vector<uint32_t> tlas_leaf_offset;
    std::vector<uint32_t> leaf_idx_table_host;

    /* Whether a TLAS instance is an Instance hit (vs. a top-level shape) is
       encoded by its leaf offset: a real offset into ``leaf_idx_table`` for
       Instances (incl. collapsed single-shape Instances, which get a one-entry
       table), ``METAL_LEAF_IDX_NONE`` for top-level shapes. */
    auto push_instance = [&](const metal_accel::InstanceDesc &inst,
                             uint32_t owner_shape_idx, uint32_t leaf_offset) {
        desc.instances.push_back(inst);
        tlas_owner_shape_idx.push_back(owner_shape_idx);
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
            metal_accel::GeometryDesc g = make_geometry(children[leaf].get());
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

    for (size_t shape_idx = 0; shape_idx < shapes.size(); ++shape_idx) {
        Shape *shape = shapes[shape_idx].get();

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
                push_instance(inst, (uint32_t) shape_idx, leaf_offset);
            }
            continue;
        }

        // Top-level shape or collapsed single-shape Instance
        metal_accel::BlasDesc blas;
        blas.geoms.push_back(make_geometry(effective_shape));
        inst.blas_idx = (uint32_t) desc.blases.size();
        desc.blases.push_back(std::move(blas));
        if (is_instance) {
            // Collapsed single-shape Instance: a one-entry leaf table (leaf 0)
            // so ``offset != NONE`` reports an instance hit and the leaf lookup
            // resolves to the group's single child.
            uint32_t leaf_offset = (uint32_t) leaf_idx_table_host.size();
            leaf_idx_table_host.push_back(0u);
            push_instance(inst, (uint32_t) shape_idx, leaf_offset);
        } else {
            push_instance(inst, (uint32_t) shape_idx, METAL_LEAF_IDX_NONE);
        }
    }

    if (desc.instances.empty())
        return desc;

    // Per-TLAS-instance lookup tables used at hit-decode time
    {
        size_t n_tlas = desc.instances.size();
        /* Per-instance userID = owning shape's registry id. The trace returns
           it as user_instance_id, so the owning ShapePtr is recovered directly
           (no host-side registry gather, mirroring OptiX). */
        for (size_t i = 0; i < n_tlas; ++i)
            desc.instances[i].user_id =
                jit_registry_id(shapes[tlas_owner_shape_idx[i]]);
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

    return desc;
}

// -----------------------------------------------------------------------
//  MetalAccel<Float, Spectrum> -- lifecycle
// -----------------------------------------------------------------------

template <typename Float, typename Spectrum>
void MetalAccel<Float, Spectrum>::init(Scene<Float, Spectrum> *scene,
                                       const Properties & /*props*/) {
    metal_accel::SceneDesc desc = metal_build_scene_desc(scene->m_shapes, this);

    if (desc.instances.empty())
        /* Leave scene_index = 0; the ray-tracing entry points below
           synthesize an all-miss result in that case. */
        Log(Debug, "accel_init_metal(): scene contains no shapes.");
    else {
        std::tie(accel, scene_index) = metal_accel::build(desc);
        /* Surface the scene as a rebindable frozen-function input (F3): the
           handle's data pointer is the MetalScene owner, which the recorder
           matches against this scene's ray-trace resource parameters. */
        accel_handle = UInt64::steal(jit_metal_scene_owner_handle(scene_index));
    }

    /* Mark all shapes as clean so that Scene::parameters_changed() only
       rebuilds the acceleration structure when geometry actually changed
       (matches the Embree/OptiX backends). */
    scene->clear_shapes_dirty();
}

template <typename Float, typename Spectrum>
void MetalAccel<Float, Spectrum>::parameters_changed(
    Scene<Float, Spectrum> *scene) {
    /* Build-and-swap: a geometry edit drops the previous scene and registers a
       fresh one (with a new scene_index and accel_handle). Frozen functions
       follow the change because the handle is a traversed input -- the next
       replay rebinds the new owner -- so no in-place scene mutation is needed. */
    release();
    Properties props;
    init(scene, props);
}

template <typename Float, typename Spectrum>
void MetalAccel<Float, Spectrum>::release() {
    if (!accel && scene_index == 0)
        return; // Empty scene or already released
    accel_handle = 0;
    metal_accel::release(accel, scene_index);
    accel = nullptr;
    scene_index = 0;
}

// -----------------------------------------------------------------------
//  MetalAccel<Float, Spectrum> -- ray queries
// -----------------------------------------------------------------------

template <typename Float, typename Spectrum>
typename MetalAccel<Float, Spectrum>::PreliminaryIntersection3f
MetalAccel<Float, Spectrum>::ray_intersect_preliminary(
    const Scene<Float, Spectrum> * /*scene*/, const Ray3f &ray, Mask /*coh*/,
    bool /*reorder*/, UInt32 /*reorder_hint*/, uint32_t /*reorder_hint_bits*/,
    Mask active) const {
    using Single = dr::float32_array_t<Float>;

    PreliminaryIntersection3f pi = dr::zeros<PreliminaryIntersection3f>();
    if (scene_index == 0) // Empty scene: every ray misses
        return pi;

    dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
    Single ray_tmin(0.f), ray_tmax(ray.maxt);

    uint32_t args[8] = {
        ray_o.x().index(), ray_o.y().index(), ray_o.z().index(),
        ray_d.x().index(), ray_d.y().index(), ray_d.z().index(),
        ray_tmin.index(), ray_tmax.index()
    };

    // out: [valid, distance, bary_u, bary_v, instance_id, primitive_id,
    //       geometry_id, user_instance_id]
    uint32_t out[8];
    jit_metal_ray_trace(8, args, active.index(), out, 8, scene_index);

    Mask valid = Mask::steal(out[0]);

    pi.t          = Float(Single::steal(out[1]));
    pi.prim_uv    = Point2f(Float(Single::steal(out[2])),
                            Float(Single::steal(out[3])));
    pi.prim_index = UInt32::steal(out[5]);

    UInt32 instance_id = UInt32::steal(out[4]);
    UInt32 geometry_id = UInt32::steal(out[6]);
    UInt32 user_id     = UInt32::steal(out[7]);

    // Recover the owning shape directly from the per-instance userID (the
    // owner's registry id) -- no host-side gather. Several consecutive TLAS
    // instances may map to the same Mitsuba Instance shape (one per per-kind
    // sub-BLAS of its ShapeGroup); they share a userID, which is correct.
    ShapePtr shape = dr::reinterpret_array<ShapePtr, UInt32>(user_id);

    /* The is-instance predicate is folded into the leaf offset: a top-level
       shape stores METAL_LEAF_IDX_NONE, an Instance a real offset. For Instance
       hits, pi.shape_index identifies the leaf within ShapeGroup::m_shapes via
       the per-instance leaf table; otherwise it is instance_id (downstream code
       does not consume it). */
    UInt32 leaf_offset = dr::gather<UInt32>(leaf_idx_offsets,
                                            instance_id, valid);
    Mask is_instance = valid && (leaf_offset != METAL_LEAF_IDX_NONE);
    UInt32 leaf_idx = dr::gather<UInt32>(leaf_idx_table,
                                         leaf_offset + geometry_id,
                                         is_instance);
    pi.shape_index = dr::select(is_instance, leaf_idx, instance_id);

    pi.shape    = shape & valid;
    pi.instance = dr::select(is_instance, shape, dr::zeros<ShapePtr>());

    pi.t[!valid]          = dr::Infinity<Float>;
    pi.prim_uv[!valid]    = dr::zeros<Point2f>();
    pi.prim_index[!valid] = 0;

    return pi;
}

template <typename Float, typename Spectrum>
typename MetalAccel<Float, Spectrum>::SurfaceInteraction3f
MetalAccel<Float, Spectrum>::ray_intersect(
    const Scene<Float, Spectrum> *scene, const Ray3f &ray, uint32_t ray_flags,
    Mask coherent, bool reorder, UInt32 reorder_hint,
    uint32_t reorder_hint_bits, Mask active) const {
    PreliminaryIntersection3f pi = ray_intersect_preliminary(
        scene, ray, coherent, reorder, reorder_hint, reorder_hint_bits, active);
    return pi.compute_surface_interaction(ray, ray_flags, active);
}

template <typename Float, typename Spectrum>
typename MetalAccel<Float, Spectrum>::Mask
MetalAccel<Float, Spectrum>::ray_test(const Scene<Float, Spectrum> * /*scene*/,
                                      const Ray3f &ray, Mask /*coherent*/,
                                      Mask active) const {
    using Single = dr::float32_array_t<Float>;

    if (scene_index == 0) // Empty scene: no occluders
        return dr::zeros<Mask>(dr::width(ray.o));

    dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
    Single ray_tmin(0.f), ray_tmax(ray.maxt);

    uint32_t args[8] = {
        ray_o.x().index(), ray_o.y().index(), ray_o.z().index(),
        ray_d.x().index(), ray_d.y().index(), ray_d.z().index(),
        ray_tmin.index(), ray_tmax.index()
    };

    uint32_t out[8];
    jit_metal_ray_trace(8, args, active.index(), out, 8, scene_index);

    Mask valid = Mask::steal(out[0]);
    for (int i = 1; i < 8; ++i) // Only the hit flag is needed
        jit_var_dec_ref(out[i]);

    return valid;
}

template <typename Float, typename Spectrum>
typename MetalAccel<Float, Spectrum>::SurfaceInteraction3f
MetalAccel<Float, Spectrum>::ray_intersect_naive(
    const Scene<Float, Spectrum> * /*scene*/, const Ray3f & /*ray*/,
    Mask /*active*/) const {
    NotImplementedError("ray_intersect_naive");
}

NAMESPACE_END(mitsuba)
