/*
    metal/shapes.h -- Shape data layouts for Metal custom-intersection shapes.

    These POD structs match the layout used by the MSL intersection functions
    in metal/intersection_functions.h. Mitsuba uses them on the host to fill
    per-primitive data buffers attached to BoundingBoxGeometryDescriptor BLAS,
    which the MSL code reads via the [[primitive_data]] attribute.

    Layout is intentionally simple (no padding, no `alignas(16)`) to avoid
    the surprises that come with the OptiX BoundingBox3f/Vector3f types.
*/

#pragma once

#if defined(MI_ENABLE_METAL)

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(metal_shape)

/// Axis-aligned bounding box in world space (float[6])
struct alignas(4) BBox3f {
    float min[3];
    float max[3];
};
static_assert(sizeof(BBox3f) == 24, "BBox3f must be 24 bytes");

/// Per-primitive data for a single sphere
struct alignas(4) SphereData {
    BBox3f bbox;
    float center[3];
    float radius;
};
static_assert(sizeof(SphereData) == 40, "SphereData must be 40 bytes");

/// Per-primitive data for a disk (z=0 unit disk in object space)
struct alignas(4) DiskData {
    BBox3f bbox;
    /// Row-major 4x4 affine: world -> object. Last row is implicitly (0,0,0,1).
    float to_object[16];
};
static_assert(sizeof(DiskData) == 24 + 64, "DiskData must be 88 bytes");

/// Per-primitive data for a cylinder (z-axis aligned, [0,length] in object space)
struct alignas(4) CylinderData {
    BBox3f bbox;
    /// Row-major 4x4 affine: world -> object.
    float to_object[16];
    float length;
    float radius;
};
static_assert(sizeof(CylinderData) == 24 + 64 + 8, "CylinderData must be 96 bytes");

/// Per-primitive data for an ellipsoid in a multi-ellipsoid shape.
struct alignas(4) EllipsoidData {
    BBox3f bbox;
    /// Row-major 4x4 affine: world -> object. Object space is the unit sphere.
    float to_object[16];
};
static_assert(sizeof(EllipsoidData) == 24 + 64, "EllipsoidData must be 88 bytes");

/// Indices of intersection functions in the linked Metal library.
/// Must match the order used to build the MTLIntersectionFunctionTable.
enum IntersectionFunctionIndex : uint32_t {
    INTERSECTION_FN_SPHERE     = 0,
    INTERSECTION_FN_DISK       = 1,
    INTERSECTION_FN_CYLINDER   = 2,
    INTERSECTION_FN_ELLIPSOIDS = 3,
    INTERSECTION_FN_SDFGRID    = 4,
    INTERSECTION_FN_COUNT      = 5
};

/// Names of the MSL functions, matching IntersectionFunctionIndex order.
static constexpr const char *kFunctionNames[INTERSECTION_FN_COUNT] = {
    "intersection_sphere",
    "intersection_disk",
    "intersection_cylinder",
    "intersection_ellipsoids",
    "intersection_sdfgrid"
};

NAMESPACE_END(metal_shape)
NAMESPACE_END(mitsuba)

#endif // MI_ENABLE_METAL
