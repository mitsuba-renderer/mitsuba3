#pragma once

#include <math.h>
#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/optix/math.cuh>

struct OptixSphereData {
    optix::BoundingBox3f bbox;
    optix::Vector3f center;
    float radius;
};

#ifdef __CUDACC__

extern "C" __global__ void __intersection__sphere() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData*) optixGetSbtDataPointer();
    OptixSphereData *sphere = (OptixSphereData *)sbt_data->data;

    // Ray in instance-space
    Ray3f ray = get_ray();

    Vector3f o = ray.o - sphere->center;
    Vector3f d = ray.d;

    float A = squared_norm(d);
    float B = 2.f * dot(o, d);
    float C = squared_norm(o) - sqr(sphere->radius);

    float near_t, far_t;
    bool solution_found = solve_quadratic(A, B, C, near_t, far_t);

    // Sphere doesn't intersect with the segment on the ray
    bool out_bounds = !(near_t <= ray.maxt && far_t >= 0.f); // NaN-aware conditionals

    // Sphere fully contains the segment of the ray
    bool in_bounds = near_t < 0.f && far_t > ray.maxt;

    float t = (near_t < 0.f ? far_t: near_t);

    if (solution_found && !out_bounds && !in_bounds)
        optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
}

extern "C" __global__ void __closesthit__sphere() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
    set_preliminary_intersection_to_payload(
        optixGetRayTmax(), Vector2f(), 0, sbt_data->shape_registry_id);
}
#endif