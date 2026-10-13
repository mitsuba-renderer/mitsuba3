/*
    metal/accel.h -- Plain-C++ interface to the Objective-C++ Metal acceleration
    structure builder (src/render/metal_accel.mm). It consumes the lowered scene
    (the \ref BlasEntry / \ref InstanceEntry descriptors from scene_ir.h), turns
    it into Metal objects, and registers them with Dr.Jit via
    jit_metal_configure_scene().
*/

#pragma once

#if defined(MI_ENABLE_METAL)

#include <mitsuba/core/platform.h>
#include <mitsuba/render/scene_ir.h>
#include <utility>

NAMESPACE_BEGIN(mitsuba)

/// Opaque handle owning the Metal objects of a built scene
struct MetalAccelData;

/// Build the lowered scene's acceleration structures (same-kind BLASes plus the
/// flattened TLAS instances) and register them with Dr.Jit. \c compact shrinks
/// the BLASes after building. Returns the owning \ref MetalAccelData and the
/// scene's JIT variable index (caller-owned), to pass to jit_metal_ray_trace().
extern MI_EXPORT_LIB std::pair<MetalAccelData *, uint32_t>
build_metal_accel(const SceneIR &sd, bool compact);

/// Release a scene built with \ref build_metal_accel(): drop the JIT variable
/// reference and free the Metal objects (deferred until no kernel/recording
/// references it).
extern MI_EXPORT_LIB void release_metal_accel(MetalAccelData *accel,
                                              uint32_t scene_index);

NAMESPACE_END(mitsuba)

#endif // MI_ENABLE_METAL
