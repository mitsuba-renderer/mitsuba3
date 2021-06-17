#pragma once

#include <math.h>
#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/optix/math.cuh>

struct OptixCylinderData {
    optix::BoundingBox3f bbox;
    optix::Transform4f to_object;
    float length;
    float radius;
};

#ifdef __CUDACC__
extern "C" __global__ void __intersection__cylinder() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData*) optixGetSbtDataPointer();
    OptixCylinderData *cylinder = (OptixCylinderData *)sbt_data->data;

    // Ray in intance-space
    Ray3f ray = get_ray();
    // Ray in object-space
    ray = cylinder->to_object.transform_ray(ray);

    float ox = ray.o.x(),
          oy = ray.o.y(),
          oz = ray.o.z(),
          dx = ray.d.x(),
          dy = ray.d.y(),
          dz = ray.d.z();

    float A = sqr(dx) + sqr(dy),
          B = 2.0 * (dx * ox + dy * oy),
          C = sqr(ox) + sqr(oy) - sqr(cylinder->radius);

    float near_t, far_t;
    bool solution_found = solve_quadratic(A, B, C, near_t, far_t);

    // Cylinder doesn't intersect with the segment on the ray
    bool out_bounds = !(near_t <= ray.maxt && far_t >= 0.f); // NaN-aware conditionals

    float z_pos_near = oz + dz * near_t,
          z_pos_far  = oz + dz * far_t;

    // Cylinder fully contains the segment of the ray
    bool in_bounds = near_t < 0.f && far_t > ray.maxt;

    bool valid_intersection =
        solution_found && !out_bounds && !in_bounds &&
        ((z_pos_near >= 0.f && z_pos_near <= cylinder->length && near_t >= 0.f) ||
         (z_pos_far  >= 0.f && z_pos_far  <= cylinder->length && far_t <= ray.maxt));

    float t = (z_pos_near >= 0 && z_pos_near <= cylinder->length && near_t >= 0.f ? near_t : far_t);

    if (valid_intersection)
        optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
}

extern "C" __global__ void __closesthit__cylinder() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
    set_preliminary_intersection_to_payload(optixGetRayTmax(), Vector2f(), 0,
                                            sbt_data->shape_registry_id);
}
#endif
