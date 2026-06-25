/*
    shapedata.h -- Cross-toolchain POD layouts for custom-shape intersection
    data (sphere / disk / cylinder / ellipsoid).
*/

#pragma once

#if defined(__METAL_VERSION__)
    typedef float4 mi_float4;
    typedef uint   mi_uint;
#elif defined(__CUDACC__) || defined(__CUDA_ARCH__)
    typedef float4       mi_float4;
    typedef unsigned int mi_uint;
#else
#   include <cstddef>
#   include <cstdint>
    struct alignas(16) mi_float4 { float x, y, z, w; };
    typedef uint32_t mi_uint;
#endif

namespace shapedata {

#if !defined(__METAL_VERSION__) && !defined(__CUDACC__) && !defined(__CUDA_ARCH__)
/// Host-side helper: pack the upper three rows of an affine matrix into the
/// ``mi_float4[3]`` layout the GPU intersection functions read.
template <typename Matrix>
inline void fill_affine3x4(const Matrix &M, mi_float4 out[3]) {
    for (size_t i = 0; i < 3; ++i)
        out[i] = { (float) M(i, 0), (float) M(i, 1),
                   (float) M(i, 2), (float) M(i, 3) };
}
#endif

/// Per-primitive data for a sphere (world space).
struct alignas(16) SphereData {
    /// xyz = center, w = radius
    mi_float4 center_radius;
};

/// Per-primitive data for a disk (object space: z=0 plane, unit radius).
struct alignas(16) DiskData {
    /// Affine world -> object transformation
    mi_float4 to_object[3];
};

/// Per-primitive data for a cylinder (object space: z-axis, [0,length], radius).
struct alignas(16) CylinderData {
    /// Affine world -> object transformation
    mi_float4 to_object[3];
    /// x = length, y = radius (z, w unused).
    mi_float4 params;
};

/// Per-primitive data for an ellipsoid (object space: unit sphere).
struct alignas(16) EllipsoidData {
    /// Affine world -> object (unit-sphere) transformation
    mi_float4 to_object[3];
};

static_assert(sizeof(SphereData)    == 16, "SphereData layout");
static_assert(sizeof(DiskData)      == 48, "DiskData layout");
static_assert(sizeof(CylinderData)  == 64, "CylinderData layout");
static_assert(sizeof(EllipsoidData) == 48, "EllipsoidData layout");

} // namespace shapedata
