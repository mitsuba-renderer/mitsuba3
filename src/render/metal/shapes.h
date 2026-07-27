/*
    metal/shapes.h -- Metal intersection-function indices for custom shapes.
    The per-primitive POD layouts live in <mitsuba/render/shapedata.h>.
*/

#pragma once

#if defined(MI_ENABLE_METAL)

#include <mitsuba/core/platform.h>
#include <mitsuba/render/shapedata.h>
#include <mitsuba/render/fwd.h>
#include <cstddef>
#include <cstdint>

NAMESPACE_BEGIN(mitsuba)

/// Indices of intersection functions in the linked Metal library.
/// Must match the order used to build the MTLIntersectionFunctionTable, and the
/// names in ``metal_isect_fn_names`` (src/render/metal_accel.mm).
enum MetalIntersectionFn : uint32_t {
    METAL_ISECT_FN_SPHERE     = 0,
    METAL_ISECT_FN_DISK       = 1,
    METAL_ISECT_FN_CYLINDER   = 2,
    METAL_ISECT_FN_ELLIPSOIDS = 3,
    METAL_ISECT_FN_SDFGRID    = 4,
    METAL_ISECT_FN_COUNT      = 5
};

/// Map a custom shape's \ref ShapeType to its Metal intersection-function
/// index, which also selects the per-type primitive-data ``[[buffer(..)]]``
/// slot. Returns \c METAL_ISECT_FN_COUNT for unsupported types.
inline uint32_t metal_fn_index(ShapeType type) {
    switch (type) {
        case ShapeType::Sphere:     return METAL_ISECT_FN_SPHERE;
        case ShapeType::Disk:       return METAL_ISECT_FN_DISK;
        case ShapeType::Cylinder:   return METAL_ISECT_FN_CYLINDER;
        case ShapeType::Ellipsoids: return METAL_ISECT_FN_ELLIPSOIDS;
        case ShapeType::SDFGrid:    return METAL_ISECT_FN_SDFGRID;
        default:                    return METAL_ISECT_FN_COUNT;
    }
}

NAMESPACE_END(mitsuba)

#endif // MI_ENABLE_METAL
