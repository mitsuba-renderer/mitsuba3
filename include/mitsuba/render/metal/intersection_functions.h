/*
    metal/intersection_functions.h -- MSL source for custom intersection
    functions used by Metal ray tracing.

    The MSL source is embedded as a C++ raw string literal. Mitsuba creates
    an MTL::Library from this source via device->newLibrary(...) and links
    the resulting MTLFunctions into the compute pipeline together with the
    drjit-generated ray-tracing kernels.

    Per-type combined buffer layout (sphere / disk / cylinder / ellipsoids):
    Mitsuba concatenates the per-primitive data of every shape of a given
    type into ONE buffer per type, bound at the type's IFT slot
    ([[buffer(N)]] where N = INTERSECTION_FN_<type>). Each TLAS instance
    carries its data offset (in elements) inside this combined buffer via
    ``MTLAccelerationStructureUserIDInstanceDescriptor::userID``, which the
    intersection function reads as ``[[user_instance_id]]``. The MSL
    function then indexes the buffer at ``user_instance_id + primitive_id``.

    This layout sidesteps Metal's "last write wins" semantics on shared IFT
    buffer slots (the IFT only has one binding per slot, but each slot now
    carries the entire concatenated dataset, and per-instance offsets pick
    the right region).

    The ``world_space_data`` intersection tag is required by MSL to make
    [[user_instance_id]] available in bounding_box intersection functions.

    SDFGrid is the exception: its data is shape-level (header + voxel
    indices + sample grid), not per-primitive, so concatenating is harder
    and SDFGrid keeps the original per-shape buffer scheme. Multiple
    SDFGrid shapes in one scene therefore still trigger a warning in
    scene_metal.inl until per-primitive shape pointers are wired up.

    The math here is a 1:1 port of the OptiX __intersection__sphere /
    __intersection__disk / __intersection__cylinder programs in
    src/shapes/optix/{sphere,disk,cylinder}.cuh.
*/

#pragma once

#if defined(MI_ENABLE_METAL)

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(metal_shape)

static constexpr const char *kIntersectionFunctionsMSL = R"MSL(
#include <metal_stdlib>
#include <metal_raytracing>
using namespace metal;
using namespace metal::raytracing;

// ---------------------------------------------------------------------------
//  Per-primitive data layouts (must match include/mitsuba/render/metal/shapes.h)
// ---------------------------------------------------------------------------

struct BBox3f {
    float min_[3];
    float max_[3];
};

struct SphereData {
    BBox3f bbox;
    float center[3];
    float radius;
};

struct DiskData {
    BBox3f bbox;
    float to_object[16]; // row-major 4x4 (last row implicit)
};

struct CylinderData {
    BBox3f bbox;
    float to_object[16];
    float length;
    float radius;
};

struct EllipsoidData {
    BBox3f bbox;
    float to_object[16];
};

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------

// Numerically stable quadratic solver. Mirrors optix/math.cuh::solve_quadratic
// (returns ordered roots x0 <= x1).
//
// Branchless GPU style: we pre-compute `x0 = x1 = -c / b` (the linear-case
// root) at the top so both the quadratic and linear branches share one
// division. When a == 0 AND b == 0 we return false up front, so this
// division is always defined; when a != 0 (the quadratic case) `b` may be 0
// and the pre-set value would be NaN/Inf, but it is overwritten further
// down by `x0 = x0m; x1 = x1m;` before being read by callers.
static inline bool solve_quadratic(float a, float b, float c,
                                   thread float &x0, thread float &x1) {
    bool linear_case = (a == 0.0f);
    if (linear_case && b == 0.0f)
        return false;

    x0 = x1 = -c / b;
    float discrim = fma(b, b, -4.0f * a * c);

    if (!linear_case && discrim < 0.0f)
        return false;

    float temp = -0.5f * (b + copysign(sqrt(max(discrim, 0.0f)), b));
    float x0p = temp / a;
    float x1p = c / temp;

    float x0m = min(x0p, x1p);
    float x1m = max(x0p, x1p);

    x0 = (linear_case ? x0 : x0m);
    x1 = (linear_case ? x0 : x1m);
    return true;
}

// Apply a row-major 4x4 affine to a point.
static inline float3 apply_affine_point(thread const float *m, float3 p) {
    return float3(
        m[0]  * p.x + m[1]  * p.y + m[2]  * p.z + m[3],
        m[4]  * p.x + m[5]  * p.y + m[6]  * p.z + m[7],
        m[8]  * p.x + m[9]  * p.y + m[10] * p.z + m[11]);
}

// Apply a row-major 4x4 affine to a vector (no translation).
static inline float3 apply_affine_vector(thread const float *m, float3 v) {
    return float3(
        m[0] * v.x + m[1] * v.y + m[2]  * v.z,
        m[4] * v.x + m[5] * v.y + m[6]  * v.z,
        m[8] * v.x + m[9] * v.y + m[10] * v.z);
}

struct BoundingBoxIntersection {
    bool  accept   [[accept_intersection]];
    float distance [[distance]];
};

// ---------------------------------------------------------------------------
//  Sphere
// ---------------------------------------------------------------------------

[[intersection(bounding_box, instancing, world_space_data)]]
BoundingBoxIntersection intersection_sphere(
    float3 origin                       [[origin]],
    float3 direction                    [[direction]],
    float  min_distance                 [[min_distance]],
    float  max_distance                 [[max_distance]],
    uint   prim_id                      [[primitive_id]],
    uint   inst_off                     [[user_instance_id]],
    device const SphereData *spheres    [[buffer(0)]])
{
    SphereData s = spheres[inst_off + prim_id];
    float3 center = float3(s.center[0], s.center[1], s.center[2]);

    // See sphere.cuh — perpendicular plane projection for numerical stability.
    // Note: `length(d)` could in principle be zero for a degenerate ray with
    // d == 0; in practice Mitsuba never generates such rays (the integrator
    // always normalizes), and Metal's IEEE-754 division by zero would yield
    // ±inf rather than crash. The downstream comparisons all evaluate to
    // false in that case, so we'd return r.accept == false (a "miss"), which
    // is the right behavior.
    float3 l = origin - center;
    float3 d = direction;
    float plane_t = dot(-l, d) / length(d);
    float3 plane_p = origin + plane_t * d;

    BoundingBoxIntersection r{false, 0.0f};

    if (plane_t == 0.0f && length(plane_p - center) > s.radius)
        return r;

    float3 o = plane_p - center;

    float A = dot(d, d);
    float B = 2.0f * dot(o, d);
    float C = dot(o, o) - s.radius * s.radius;

    float near_t, far_t;
    bool ok = solve_quadratic(A, B, C, near_t, far_t);

    near_t += plane_t;
    far_t  += plane_t;

    bool out_bounds = !(near_t <= max_distance && far_t >= 0.0f);
    bool in_bounds  = near_t < min_distance && far_t > max_distance;

    float t = (near_t < 0.0f ? far_t : near_t);

    if (ok && !out_bounds && !in_bounds && t >= min_distance && t <= max_distance) {
        r.accept   = true;
        r.distance = t;
    }
    return r;
}

// ---------------------------------------------------------------------------
//  Disk (object-space: z=0 plane, unit radius)
// ---------------------------------------------------------------------------

[[intersection(bounding_box, instancing, world_space_data)]]
BoundingBoxIntersection intersection_disk(
    float3 origin                       [[origin]],
    float3 direction                    [[direction]],
    float  min_distance                 [[min_distance]],
    float  max_distance                 [[max_distance]],
    uint   prim_id                      [[primitive_id]],
    uint   inst_off                     [[user_instance_id]],
    device const DiskData *disks        [[buffer(1)]])
{
    DiskData d = disks[inst_off + prim_id];

    float3 ro = apply_affine_point(d.to_object, origin);
    float3 rd = apply_affine_vector(d.to_object, direction);

    float t = -ro.z / rd.z;
    float3 local = ro + t * rd;

    BoundingBoxIntersection r{false, 0.0f};
    if (local.x * local.x + local.y * local.y <= 1.0f &&
        t > min_distance && t < max_distance) {
        r.accept   = true;
        r.distance = t;
    }
    return r;
}

// ---------------------------------------------------------------------------
//  Cylinder (object-space: z-axis, [0, length], radius)
// ---------------------------------------------------------------------------

[[intersection(bounding_box, instancing, world_space_data)]]
BoundingBoxIntersection intersection_cylinder(
    float3 origin                       [[origin]],
    float3 direction                    [[direction]],
    float  min_distance                 [[min_distance]],
    float  max_distance                 [[max_distance]],
    uint   prim_id                      [[primitive_id]],
    uint   inst_off                     [[user_instance_id]],
    device const CylinderData *cyls     [[buffer(2)]])
{
    CylinderData c = cyls[inst_off + prim_id];

    float3 ro = apply_affine_point(c.to_object, origin);
    float3 rd = apply_affine_vector(c.to_object, direction);

    float A = rd.x * rd.x + rd.y * rd.y;
    float B = 2.0f * (rd.x * ro.x + rd.y * ro.y);
    float C = ro.x * ro.x + ro.y * ro.y - c.radius * c.radius;

    float near_t, far_t;
    bool ok = solve_quadratic(A, B, C, near_t, far_t);

    bool out_bounds = !(near_t <= max_distance && far_t >= min_distance);

    float z_pos_near = ro.z + rd.z * near_t;
    float z_pos_far  = ro.z + rd.z * far_t;

    bool in_bounds = near_t < min_distance && far_t > max_distance;

    bool valid =
        ok && !out_bounds && !in_bounds &&
        ((z_pos_near >= 0.0f && z_pos_near <= c.length && near_t > min_distance) ||
         (z_pos_far  >= 0.0f && z_pos_far  <= c.length && far_t  < max_distance));

    float t = (z_pos_near >= 0.0f && z_pos_near <= c.length && near_t >= 0.0f)
              ? near_t : far_t;

    BoundingBoxIntersection r{false, 0.0f};
    if (valid && t >= min_distance && t <= max_distance) {
        r.accept   = true;
        r.distance = t;
    }
    return r;
}

// ---------------------------------------------------------------------------
//  Ellipsoids (one ellipsoid per primitive — object space is unit sphere)
// ---------------------------------------------------------------------------

[[intersection(bounding_box, instancing, world_space_data)]]
BoundingBoxIntersection intersection_ellipsoids(
    float3 origin                       [[origin]],
    float3 direction                    [[direction]],
    float  min_distance                 [[min_distance]],
    float  max_distance                 [[max_distance]],
    uint   prim_id                      [[primitive_id]],
    uint   inst_off                     [[user_instance_id]],
    device const EllipsoidData *ellis   [[buffer(3)]])
{
    EllipsoidData e = ellis[inst_off + prim_id];

    // World -> object (unit-sphere) space ray.
    float3 ro = apply_affine_point(e.to_object, origin);
    float3 rd = apply_affine_vector(e.to_object, direction);

    BoundingBoxIntersection r{false, 0.0f};

    // Perpendicular-plane projection trick (matches src/shapes/optix/
    // ellipsoids.cuh:62-72): we shift the ray origin to the foot of the
    // perpendicular dropped from the ellipsoid center onto the ray, which
    // significantly improves the conditioning of the discriminant for
    // distant rays / grazing angles.
    float plane_t  = dot(-ro, rd) / length(rd);
    float3 plane_p = ro + plane_t * rd;

    // Ray is perpendicular to the origin-center segment AND its closest
    // point on the ray is outside the unit sphere -> definite miss.
    if (plane_t == 0.0f && length(plane_p) > 1.0f)
        return r;

    // Ray-vs-unit-sphere using the shifted origin `plane_p`.
    float A = dot(rd, rd);
    float B = 2.0f * dot(plane_p, rd);
    float C = dot(plane_p, plane_p) - 1.0f;

    float near_t, far_t;
    bool ok = solve_quadratic(A, B, C, near_t, far_t);

    // Re-anchor the parametric distances to the original ray origin.
    near_t += plane_t;
    far_t  += plane_t;

    // Match OptiX semantics (ellipsoids.cuh:82-94):
    //   out_bounds: ellipsoid does not intersect [0, ray.maxt]
    //   in_bounds : ellipsoid fully contains the ray segment
    //   backfacing: ray origin is INSIDE the ellipsoid (rejected)
    bool out_bounds = !(near_t <= max_distance && far_t >= 0.0f);
    bool in_bounds  = near_t < 0.0f && far_t > max_distance;
    bool backfacing = near_t < 0.0f;

    if (ok && !out_bounds && !in_bounds && !backfacing &&
        near_t >= min_distance && near_t <= max_distance) {
        r.accept   = true;
        r.distance = near_t;
    }
    return r;
}

// ---------------------------------------------------------------------------
//  SDF grid (one AABB per filled voxel)
// ---------------------------------------------------------------------------
//
// Layout of the per-shape buffer at [[buffer(4)]]:
//   bytes [ 0.. 4): res_x  (uint)
//   bytes [ 4.. 8): res_y
//   bytes [ 8..12): res_z
//   bytes [12..16): n_voxels
//   bytes [16..28): voxel_size[3] (float)
//   bytes [28..32): padding
//   bytes [32..96): to_object[16] (row-major 4x4 affine)
//   bytes [96..96 + 4*n_voxels):       voxel_indices (uint per filled voxel)
//   bytes [...end): grid_data (float per grid cell, total = res_x*res_y*res_z)

struct SDFGridHeader {
    uint res_x;
    uint res_y;
    uint res_z;
    uint n_voxels;
    float voxel_size[3];
    float pad;
    float to_object[16];
};

static inline bool sdf_solve_quadratic(float a, float b, float c,
                                       thread float &x0, thread float &x1) {
    return solve_quadratic(a, b, c, x0, x1);
}

static inline bool sdf_intersect_aabb(float3 ro, float3 rd,
                                       float3 bbmin, float3 bbmax,
                                       thread float &t_min, thread float &t_max) {
    float3 inv_d = 1.0f / rd;
    float3 t1 = (bbmin - ro) * inv_d;
    float3 t2 = (bbmax - ro) * inv_d;
    float3 t_lo = min(t1, t2);
    float3 t_hi = max(t1, t2);
    t_min = max(max(t_lo.x, t_lo.y), t_lo.z);
    t_max = min(min(t_hi.x, t_hi.y), t_hi.z);
    return t_max >= max(t_min, 0.0f);
}

[[intersection(bounding_box, instancing)]]
BoundingBoxIntersection intersection_sdfgrid(
    float3 origin                       [[origin]],
    float3 direction                    [[direction]],
    float  min_distance                 [[min_distance]],
    float  max_distance                 [[max_distance]],
    uint   prim_id                      [[primitive_id]],
    device const uchar *buf             [[buffer(4)]])
{
    const device SDFGridHeader &h =
        *(const device SDFGridHeader *) buf;
    const device uint *voxel_indices =
        (const device uint *) (buf + sizeof(SDFGridHeader));
    const device float *grid =
        (const device float *) (voxel_indices + h.n_voxels);

    BoundingBoxIntersection r{false, 0.0f};

    uint vi = voxel_indices[prim_id];
    uint x_len = h.res_x - 1;
    uint y_len = h.res_y - 1;
    // Voxel index encoding: vi = vx + vy * x_len + vz * x_len * y_len
    // (see SDFGrid::build_bboxes() / sdfgrid.cpp). Decoding therefore
    // divides by x_len for vy and by x_len*y_len for vz.
    uint vx = vi % x_len;
    uint vy = ((vi - vx) / x_len) % y_len;
    uint vz = (vi - vx - vy * x_len) / (x_len * y_len);

    // World -> object space ray. Copy matrix to thread storage first so it
    // matches the helpers' address-space signature.
    float to_object[16];
    for (int i = 0; i < 16; ++i)
        to_object[i] = h.to_object[i];
    float3 ro = apply_affine_point(to_object, origin);
    float3 rd = apply_affine_vector(to_object, direction);

    // Voxel AABB in object space.
    float3 vox = float3((float) vx, (float) vy, (float) vz);
    float3 vsize = float3(h.voxel_size[0], h.voxel_size[1], h.voxel_size[2]);
    float3 bbmin = vox * vsize;
    float3 bbmax = bbmin + vsize;

    float t_bbox_beg = 0.0f, t_bbox_end = 0.0f;
    if (!sdf_intersect_aabb(ro, rd, bbmin, bbmax, t_bbox_beg, t_bbox_end))
        return r;

    t_bbox_beg = max(t_bbox_beg, 0.0f);
    if (t_bbox_end < t_bbox_beg)
        return r;

    // Ray in voxel-local [0, 1]^3 space:
    //   p_voxel = (p_object / voxel_size) - voxel_position
    float3 inv_vs = 1.0f / vsize;
    float3 ro_v = ro * inv_vs - vox;
    float3 rd_v = rd * inv_vs;

    // Read 8 SDF corner values for this voxel.
    uint sx = h.res_x, sy = h.res_y;
    auto sample_at = [&](uint x, uint y, uint z) -> float {
        return grid[z * sy * sx + y * sx + x];
    };
    float s000 = sample_at(vx,     vy,     vz);
    float s100 = sample_at(vx + 1, vy,     vz);
    float s010 = sample_at(vx,     vy + 1, vz);
    float s110 = sample_at(vx + 1, vy + 1, vz);
    float s001 = sample_at(vx,     vy,     vz + 1);
    float s101 = sample_at(vx + 1, vy,     vz + 1);
    float s011 = sample_at(vx,     vy + 1, vz + 1);
    float s111 = sample_at(vx + 1, vy + 1, vz + 1);

    // Cubic polynomial coefficients for the trilinearly-interpolated SDF
    // along the ray. (Same algebra as Hansson-Söderlund et al. 2022 / the
    // OptiX __intersection__sdfgrid function.)
    float3 p0 = ro_v + t_bbox_beg * rd_v;
    float o_x = p0.x, o_y = p0.y, o_z = p0.z;
    float d_x = rd_v.x, d_y = rd_v.y, d_z = rd_v.z;

    float a  = s101 - s001;
    float k0 = s000;
    float k1 = s100 - s000;
    float k2 = s010 - s000;
    float k3 = s110 - s010 - k1;
    float k4 = k0 - s001;
    float k5 = k1 - a;
    float k6 = k2 - (s011 - s001);
    float k7 = k3 - (s111 - s011 - a);
    float m0 = o_x * o_y;
    float m1 = d_x * d_y;
    float m2 = fma(o_x, d_y, o_y * d_x);
    float m3 = fma(k5, o_z, -k1);
    float m4 = fma(k6, o_z, -k2);
    float m5 = fma(k7, o_z, -k3);

    float c0 = fma(k4, o_z, -k0) + fma(o_x, m3, fma(o_y, m4, m0 * m5));
    float c1 = fma(d_x, m3, d_y * m4) + m2 * m5 +
               d_z * (k4 + fma(k5, o_x, fma(k6, o_y, k7 * m0)));
    float c2 = fma(m1, m5, d_z * (fma(k5, d_x, fma(k6, d_y, k7 * m2))));
    float c3 = k7 * m1 * d_z;

    float t_beg = 0.0f;
    float t_end = t_bbox_end - t_bbox_beg;

    auto eval_sdf = [&](float t) -> float {
        return fma(fma(fma(c3, t, c2), t, c1), t, c0);
    };

    auto numerical_solve = [&](float t_near, float t_far,
                               float f_near, float f_far) -> float {
        const int max_iter = 50;
        const float eps = 1e-5f;
        float t = 0.0f;
        for (int i = 0; i < max_iter; ++i) {
            t = t_near + (t_far - t_near) * (-f_near / (f_far - f_near));
            float f_t = eval_sdf(t);
            if (f_t * f_near <= 0.0f) {
                t_far = t;
                f_far = f_t;
            } else {
                t_near = t;
                f_near = f_t;
            }
            if (abs(t_far - t_near) < eps)
                break;
        }
        return t;
    };

    if (c3 != 0.0f) {
        // Cubic — bracket then bisect.
        float root_0 = 0.0f, root_1 = 0.0f;
        bool has_drv = sdf_solve_quadratic(c3 * 3.0f, c2 * 2.0f, c1, root_0, root_1);
        float t_near = t_beg, t_far = t_end;
        float f_near = eval_sdf(t_near);
        float f_far  = eval_sdf(t_far);

        if (has_drv) {
            if (t_near <= root_0 && root_0 <= t_far) {
                float fr = eval_sdf(root_0);
                if (f_near * fr <= 0.0f) { t_far = root_0; f_far = fr; }
                else                     { t_near = root_0; f_near = fr; }
            }
            if (t_near <= root_1 && root_1 <= t_far) {
                float fr = eval_sdf(root_1);
                if (f_near * fr <= 0.0f) { t_far = root_1; f_far = fr; }
                else                     { t_near = root_1; f_near = fr; }
            }
        }
        if (f_near * f_far > 0.0f)
            return r;
        float t = numerical_solve(t_near, t_far, f_near, f_far);
        float t_world = t_bbox_beg + t;
        if (t_beg <= t && t <= t_end &&
            t_world >= min_distance && t_world < max_distance) {
            r.accept   = true;
            r.distance = t_world;
        }
    } else {
        // Quadratic / linear.
        float r0 = 0.0f, r1 = 0.0f;
        bool ok = sdf_solve_quadratic(c2, c1, c0, r0, r1);
        if (ok) {
            float t_world = t_bbox_beg + r0;
            if (t_beg <= r0 && r0 <= t_end &&
                t_world >= min_distance && t_world < max_distance) {
                r.accept   = true;
                r.distance = t_world;
            } else {
                t_world = t_bbox_beg + r1;
                if (t_beg <= r1 && r1 <= t_end &&
                    t_world >= min_distance && t_world < max_distance) {
                    r.accept   = true;
                    r.distance = t_world;
                }
            }
        }
    }
    return r;
}

)MSL";

NAMESPACE_END(metal_shape)
NAMESPACE_END(mitsuba)

#endif // MI_ENABLE_METAL
