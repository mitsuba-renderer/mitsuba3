#pragma once

#include <math.h>
#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/optix/math.cuh>
#include <mitsuba/render/shapedata.h>

#ifdef __CUDACC__

extern "C" __global__ void __intersection__sphere() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData*) optixGetSbtDataPointer();
    shapedata::SphereData *sphere = (shapedata::SphereData*) sbt_data->data;
    Vector3f center(sphere->center_radius.x, sphere->center_radius.y,
                    sphere->center_radius.z);
    float radius = sphere->center_radius.w;

    // Ray in instance-space
    Ray3f ray = get_ray();

    // We define a plane which is perpendicular to the ray direction and
    // contains the sphere center and intersect it. We then solve the ray-sphere
    // intersection as if the ray origin was this new intersection point. This
    // additional step makes the whole intersection routine numerically more
    // robust.

    Vector3f l = ray.o - center;
    Vector3f d = ray.d;
    float plane_t = dot(-l, d) / norm(d);
    Vector3f plane_p = ray(plane_t);

    // Ray is perpendicular to the origin-center segment,
    // and intersection with plane is outside of the sphere
    if (plane_t == 0.f && norm(plane_p - center) > radius)
        return;

    Vector3f o = plane_p - center;

    float A = squared_norm(d);
    float B = 2.f * dot(o, d);
    float C = squared_norm(o) - sqr(radius);

    float near_t, far_t;
    bool solution_found = solve_quadratic(A, B, C, near_t, far_t);

    // Adjust distances for plane intersection
    near_t += plane_t;
    far_t += plane_t;

    // Sphere doesn't intersect with the segment on the ray
    bool out_bounds = !(near_t <= ray.maxt && far_t >= 0.f); // NaN-aware conditionals

    // Sphere fully contains the segment of the ray
    bool in_bounds = near_t < ray.mint && far_t > ray.maxt;

    float t = (near_t < 0.f ? far_t: near_t);

    if (solution_found && !out_bounds && !in_bounds)
        optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
}
#endif
