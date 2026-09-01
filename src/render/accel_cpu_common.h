/*
    accel_cpu_common.h -- Shared helpers for CPU ray-tracing backends.
*/

#pragma once

#include <mitsuba/render/fwd.h>
#include <memory>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/**
 * Build the per-shape registry-id buffer used by the CPU backends to
 * recover a `ShapePtr` from a hit's geometry / instance index.
 */
template <typename Float, typename Spectrum>
DynamicBuffer<dr::uint32_array_t<Float>>
build_registry_ids(const std::vector<ref<Shape<Float, Spectrum>>> &shapes) {
    using UInt32 = dr::uint32_array_t<Float>;
    // The scene requires this list to be nonempty. Return a dummy entry
    // when there are no shapes.
    if (shapes.empty())
        return dr::zeros<DynamicBuffer<UInt32>>(1);
    std::unique_ptr<uint32_t[]> data(new uint32_t[shapes.size()]);
    for (size_t i = 0; i < shapes.size(); ++i)
        data[i] = jit_registry_id(shapes[i]);
    return dr::load<DynamicBuffer<UInt32>>(data.get(), shapes.size());
}

/**
 * Decode the closest-hit result of a kd-tree (LLVM) ray trace into
 * a `PreliminaryIntersection`.
 *
 * The trace writes ``out`` = { valid, t, u, v, prim_index, shape_index,
 * unused, unused }. The owning `ShapePtr` comes from ``registry_ids``,
 * gathered by the top-level shape index.
 */
template <typename Float, typename Spectrum, typename RayScalar>
auto decode_cpu_llvm_pi(
        const uint32_t out[8],
        const DynamicBuffer<dr::uint32_array_t<Float>> &registry_ids) {
    MI_IMPORT_TYPES(Shape, ShapePtr)

    PreliminaryIntersection3f pi;

    pi.valid       = Mask::steal(out[0]);
    pi.t           = Float(RayScalar::steal(out[1]));
    pi.prim_uv     = Vector2f(RayScalar::steal(out[2]), RayScalar::steal(out[3]));
    pi.prim_index  = UInt32::steal(out[4]);
    pi.shape       = dr::gather<UInt32>(registry_ids, UInt32::steal(out[5]),
                                        pi.valid);
    UInt32::steal(out[6]);
    Mask::steal(out[7]);

    return pi;
}

/**
 * Assemble the standard 14-element input vector for a CPU (LLVM) ray
 * trace and invoke ``jit_llvm_ray_trace``.
 *
 * With ``shadow_ray``, this is an occlusion query and ``out`` holds one boolean
 * result variable index. Otherwise it holds eight. Ray components are supplied
 * in the precision the backend traces in (float32 for Embree, ``Float`` for the
 * native kd-tree).
 */
template <typename Float, typename RayScalar, typename Mask>
void cpu_llvm_ray_trace(void *func_ptr, uint32_t func_handle_index,
                        void *scene_ptr, uint32_t accel_handle_index,
                        const dr::Array<RayScalar, 3> &ray_o,
                        const dr::Array<RayScalar, 3> &ray_d, RayScalar ray_time,
                        RayScalar ray_maxt, Mask coherent, Mask active,
                        const dr::uint32_array_t<Float> &visibility_mask,
                        int shadow_ray, uint32_t *out) {
    using UInt32 = dr::uint32_array_t<Float>;
    using UInt64 = dr::uint64_array_t<Float>;

    UInt64 func_v  = UInt64::steal(jit_var_pointer(
               JitBackend::LLVM, func_ptr, func_handle_index, 0)),
           scene_v = UInt64::steal(jit_var_pointer(
               JitBackend::LLVM, scene_ptr, accel_handle_index, 0));

    RayScalar ray_mint = dr::zeros<RayScalar>();
    UInt32 zero = dr::zeros<UInt32>();

    uint32_t in[14] = { coherent.index(),  active.index(),
                        ray_o.x().index(), ray_o.y().index(),
                        ray_o.z().index(), ray_mint.index(),
                        ray_d.x().index(), ray_d.y().index(),
                        ray_d.z().index(), ray_time.index(),
                        ray_maxt.index(),  visibility_mask.index(),
                        zero.index(),      zero.index() };

    jit_llvm_ray_trace(func_v.index(), scene_v.index(), shadow_ray, in, out);
}

/**
 * Map a CPU backend's JIT handle to its native object and cleanup hook.
 */
template <typename UInt64>
void init_mapped_handle(UInt64 &handle, void *ptr,
                        void (*callback)(uint32_t, int, void *),
                        void *payload) {
    handle = UInt64::map_(ptr, 1, false);
    jit_var_set_callback(handle.index(), callback, payload);
}

/**
 * Map the width-specialized intersect / occlude entry points to
 * freeze-visible JIT handles (shared by both CPU backends).
 */
template <typename UInt64>
void map_func_handles(UInt64 &func_handle, UInt64 &occlude_handle,
                      void *func_ptr, void *occlude_func_ptr) {
    func_handle    = UInt64::map_(func_ptr, 1, false);
    occlude_handle = UInt64::map_(occlude_func_ptr, 1, false);
}

NAMESPACE_END(mitsuba)
