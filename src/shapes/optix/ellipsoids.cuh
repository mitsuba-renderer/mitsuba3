#pragma once

#include <math.h>
#include <math.h>
#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/optix/math.cuh>

struct OptixEllipsoidsData {
    optix::BoundingBox3f bbox;
    float* extents;
    float* data;

#ifdef __CUDACC__
    DEVICE Vector3f center(unsigned int index) {
        return Vector3f::load(&data[index * 10]);
    }

    DEVICE Vector3f scale(unsigned int index) {
        return Vector3f::load(&data[index * 10 + 3]);
    }

    DEVICE Matrix3f rotation(unsigned int index) {
        auto q = Vector4f::load(&data[index * 10 + 6]);
        q = q * SqrtTwo;
        float xx = q.x() * q.x(), yy = q.y() * q.y(), zz = q.z() * q.z(),
              xy = q.x() * q.y(), xz = q.x() * q.z(), yz = q.y() * q.z(),
              xw = q.x() * q.w(), yw = q.y() * q.w(), zw = q.z() * q.w();
        auto m = Matrix3f(); // Already transposed
        m[0] = Vector3f(1.f - (yy + zz), xy - zw, xz + yw);
        m[1] = Vector3f(xy + zw, 1.f - (xx + zz), yz - xw);
        m[2] = Vector3f(xz - yw,  yz + xw, 1.f - (xx + yy));
        return m;
    }
#endif
};

#ifdef __CUDACC__
extern "C" __global__ void __intersection__ellipsoids() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData*) optixGetSbtDataPointer();
    OptixEllipsoidsData &ellipsoid = *((OptixEllipsoidsData *) sbt_data->data);

    unsigned int prim_index = optixGetPrimitiveIndex();
    Vector3f center   = ellipsoid.center(prim_index);
    Vector3f scale    = ellipsoid.scale(prim_index);
    Matrix3f rotation = ellipsoid.rotation(prim_index);
    scale *= ellipsoid.extents[prim_index];

    // Ray in instance-space
    Ray3f ray = get_ray();
    Vector3f o = rotation.prod_inv(ray.o - center);
    Vector3f d = rotation.prod_inv(ray.d);
    o /= scale;
    d /= scale;

    float near_t, far_t;

#if 0
    float A = squared_norm(d);
    float B = 2.f * dot(o, d);
    float C = squared_norm(o) - 1.f;
    bool solution_found = solve_quadratic(A, B, C, near_t, far_t);
#else
    // Taken from Ray Tracing Gems 2
    float A = squared_norm(d);
    float B = -dot(o, d);
    float C = squared_norm(o) - 1.f;
    float discr = 1.f - squared_norm(o + (B / A) * d);
    bool solution_found = improved_solve_quadratic(A, B, C, discr, near_t, far_t);
#endif

    // Ellipsoid doesn't intersect with the segment on the ray
    bool out_bounds = !(near_t <= ray.maxt && far_t >= 0.f); // NaN-aware conditionals

    // Ellipsoid fully contains the segment of the ray
    bool in_bounds = near_t < 0.f && far_t > ray.maxt;

    // Ellipsoid is backfacing
    bool backfacing = near_t < 0.f;

    bool active = solution_found && !out_bounds && !in_bounds && !backfacing;

    float t = (near_t < 0.f ? far_t: near_t);

    if (active) {
        optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
    }
}
#endif
