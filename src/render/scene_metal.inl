/*
    scene_metal.inl -- Templated half of the Metal backend: lowers the scene for
    the acceleration structure builder (src/render/metal_accel.mm) and dispatches
    rays through jit_metal_ray_trace(). State and declarations live in
    include/mitsuba/render/accel_metal.h.
*/

#include <drjit-core/metal.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/scene_ir.h>
#include "metal/accel.h"
#include "metal/shapes.h"

NAMESPACE_BEGIN(mitsuba)

/// Build the raw recovery tables that resolve \c pi.shape from a Metal hit.
/// The trace reports raw instance and geometry indices. The offset table maps
/// the pair into the shape-id table.
static void build_recovery_table_data(const SceneIR &sd,
                                      std::vector<uint32_t> &offsets,
                                      std::vector<uint32_t> &table) {
    offsets.clear();
    table.clear();
    offsets.reserve(sd.instances.size());
    for (const InstanceEntry &inst : sd.instances) {
        offsets.push_back((uint32_t) table.size());
        for (const ShapeIR &g : sd.blases[inst.blas_index].geoms)
            table.push_back(jit_registry_id(g.ctx));
    }
}

// -----------------------------------------------------------------------
//  MetalAccel<Float, Spectrum> -- lifecycle
// -----------------------------------------------------------------------

template <typename Float, typename Spectrum>
void MetalAccel<Float, Spectrum>::init(Scene<Float, Spectrum> *scene,
                                       const Properties & /*props*/) {
    SceneIR sd = SceneIRBuilder<Float, Spectrum>::build(scene);

    if (sd.instances.empty()) {
        Log(Debug, "accel_init_metal(): scene contains no shapes.");
        return;
    }

    std::vector<uint32_t> offsets, table;
    build_recovery_table_data(sd, offsets, table);
    using UInt32 = dr::uint32_array_t<Float>;
    geom_shape_offsets =
        dr::load<DynamicBuffer<UInt32>>(offsets.data(), offsets.size());
    geom_shape_table =
        dr::load<DynamicBuffer<UInt32>>(table.data(), table.size());

    std::tie(accel, scene_index) =
        build_metal_accel(sd, scene->m_compact_accel);

    accel_handle = UInt64::steal(jit_metal_scene_owner_handle(scene_index));
}

template <typename Float, typename Spectrum>
void MetalAccel<Float, Spectrum>::rebuild(
    Scene<Float, Spectrum> *scene) {
    // Build-and-swap: a geometry edit drops the previous scene and registers a
    // fresh one. Frozen functions follow the traversed handle on the next
    // replay, so no in-place scene mutation is needed.
    release();
    Properties props;
    init(scene, props);
}

template <typename Float, typename Spectrum>
void MetalAccel<Float, Spectrum>::release() {
    if (!accel && scene_index == 0)
        return; // Empty scene or already released
    accel_handle = 0;
    release_metal_accel(accel, scene_index);
    accel = nullptr;
    scene_index = 0;
}

// -----------------------------------------------------------------------
//  MetalAccel<Float, Spectrum> -- ray queries
// -----------------------------------------------------------------------

template <typename Float, typename Spectrum>
void MetalAccel<Float, Spectrum>::trace(const Ray3f &ray, Mask active,
                                        uint32_t out[8], bool shadow) const {
    using Single = dr::float32_array_t<Float>;
    dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
    Single ray_tmin(0.f), ray_tmax(ray.maxt);

    // Be careful with 'ray.maxt' in double precision variants
    if constexpr (!std::is_same_v<Single, Float>)
        ray_tmax = dr::minimum(ray_tmax, dr::Largest<Single>);

    uint32_t args[8] = {
        ray_o.x().index(), ray_o.y().index(), ray_o.z().index(),
        ray_d.x().index(), ray_d.y().index(), ray_d.z().index(),
        ray_tmin.index(), ray_tmax.index()
    };

    jit_metal_ray_trace(8, args, active.index(), out, 8, scene_index, shadow);
}

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

    // out: [valid, distance, bary_u, bary_v, instance_id, primitive_id,
    //       geometry_id, user_instance_id]
    uint32_t out[8];
    trace(ray, active, out, /* shadow = */ false);

    Mask valid = Mask::steal(out[0]);

    pi.valid      = valid;
    pi.t          = Float(Single::steal(out[1]));
    pi.prim_uv    = Point2f(Float(Single::steal(out[2])),
                            Float(Single::steal(out[3])));
    pi.prim_index = UInt32::steal(out[5]);

    UInt32 instance_id = UInt32::steal(out[4]);
    UInt32 geometry_id = UInt32::steal(out[6]);

    // The hit shape's registry id is the (instance, geometry) entry of the
    // recovery table, so pi.shape names the actual child for every hit. The
    // gathers are masked, so missed lanes read 0 (a null shape).
    UInt32 off      = dr::gather<UInt32>(geom_shape_offsets, instance_id, valid);
    UInt32 shape_id = dr::gather<UInt32>(geom_shape_table, off + geometry_id,
                                         valid);
    pi.shape = dr::reinterpret_array<ShapePtr, UInt32>(shape_id);

    // userID is the owning Instance's registry id, or 0 (a null shape) for a
    // top-level hit. Missed lanes read 0 too. Consecutive TLAS instances of one
    // ShapeGroup (one per BLAS) share a userID, which is correct.
    UInt32 user_id = dr::select(valid, UInt32::steal(out[7]), dr::zeros<UInt32>());
    pi.instance = dr::reinterpret_array<ShapePtr, UInt32>(user_id);

    return pi;
}

template <typename Float, typename Spectrum>
typename MetalAccel<Float, Spectrum>::Mask
MetalAccel<Float, Spectrum>::ray_test(const Scene<Float, Spectrum> * /*scene*/,
                                      const Ray3f &ray, Mask /*coherent*/,
                                      Mask active) const {
    if (scene_index == 0) // Empty scene: no occluders
        return dr::zeros<Mask>(dr::width(ray.o));

    uint32_t out[8];
    trace(ray, active, out, /* shadow = */ true);

    // A shadow trace computes only out[0] (the hit flag). out[1..7] are left
    // untouched (see jit_metal_ray_trace), so steal just the hit flag.
    return Mask::steal(out[0]);
}

template <typename Float, typename Spectrum>
typename MetalAccel<Float, Spectrum>::SurfaceInteraction3f
MetalAccel<Float, Spectrum>::ray_intersect_naive(
    const Scene<Float, Spectrum> *scene, const Ray3f &ray, Mask active) const {
    // Metal has no brute-force path; route through the accelerated query.
    return scene->ray_intersect(ray, active);
}

NAMESPACE_END(mitsuba)
