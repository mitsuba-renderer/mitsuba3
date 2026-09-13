#pragma once

#include <math.h>
#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/optix/math.cuh>
#include <mitsuba/render/shapedata.h>

#ifdef __CUDACC__
extern "C" __global__ void __intersection__cylinder() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData*) optixGetSbtDataPointer();
    shapedata::CylinderData *cylinder = (shapedata::CylinderData *)sbt_data->data;
    float length = cylinder->params.x, radius = cylinder->params.y;

    // Ray in intance-space
    Ray3f ray = get_ray();
    // Ray in object-space
    ray = apply_affine_ray(cylinder->to_object, ray);

    // Shift the ray origin to the point closest to the axis (see sphere.cpp)
    float lx = ray.o.x(), ly = ray.o.y(),
          dx = ray.d.x(), dy = ray.d.y();

    float A = sqr(dx) + sqr(dy);
    float t_offset = A != 0.f ? -(lx * dx + ly * dy) / A : 0.f;
    float ox = fmaf(t_offset, dx, lx),
          oy = fmaf(t_offset, dy, ly);

    float B = 2.f * (ox * dx + oy * dy),
          C = sqr(ox) + sqr(oy) - sqr(radius);

    float near_t, far_t;
    bool solution_found = solve_quadratic(A, B, C, near_t, far_t);
    near_t += t_offset;
    far_t += t_offset;

    float z_near = fmaf(ray.d.z(), near_t, ray.o.z()),
          z_far  = fmaf(ray.d.z(), far_t,  ray.o.z());

    bool near_ok = near_t >= 0.f && near_t <= ray.maxt &&
                   z_near >= 0.f && z_near <= length,
         far_ok  = far_t  >= 0.f && far_t  <= ray.maxt &&
                   z_far  >= 0.f && z_far  <= length;

    if (solution_found && (near_ok || far_ok))
        optixReportIntersection(near_ok ? near_t : far_t,
                                OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
}
#endif
