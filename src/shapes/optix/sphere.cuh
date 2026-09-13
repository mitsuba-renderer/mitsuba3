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

    // Shift the ray origin to the point closest to the center (see sphere.cpp)
    Vector3f l = ray.o - center;
    Vector3f d = ray.d;

    float A = squared_norm(d);
    float t_offset = -dot(l, d) / A;
    Vector3f o = fmaf(t_offset, d, l);

    float B = 2.f * dot(o, d);
    float C = squared_norm(o) - sqr(radius);

    float near_t, far_t;
    bool solution_found = solve_quadratic(A, B, C, near_t, far_t);

    // Undo the origin shift
    near_t += t_offset;
    far_t += t_offset;

    bool near_ok = near_t >= 0.f && near_t <= ray.maxt,
         far_ok  = far_t  >= 0.f && far_t  <= ray.maxt;

    if (solution_found && (near_ok || far_ok))
        optixReportIntersection(near_ok ? near_t : far_t,
                                OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
}
#endif
