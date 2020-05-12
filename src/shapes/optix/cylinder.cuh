#pragma once

#include <math.h>
#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/optix/math.cuh>

struct OptixCylinderData {
    optix::BoundingBox3f bbox;
    optix::Transform4f to_world;
    optix::Transform4f to_object;
    float length;
    float radius;
    bool flip_normals;
};

#ifdef __CUDACC__
extern "C" __global__ void __intersection__cylinder() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData*) optixGetSbtDataPointer();
    OptixCylinderData *cylinder = (OptixCylinderData *)sbt_data->data;

    float mint = optixGetRayTmin();
    float maxt = optixGetRayTmax();
    Vector3f ray_o = make_vector3f(optixGetWorldRayOrigin());
    Vector3f ray_d = make_vector3f(optixGetWorldRayDirection());

    ray_o = cylinder->to_object.transform_point(ray_o);
    ray_d = cylinder->to_object.transform_vector(ray_d);

    float ox = ray_o.x(),
          oy = ray_o.y(),
          oz = ray_o.z(),
          dx = ray_d.x(),
          dy = ray_d.y(),
          dz = ray_d.z();

    float A = sqr(dx) + sqr(dy),
          B = 2.0 * (dx * ox + dy * oy),
          C = sqr(ox) + sqr(oy) - sqr(cylinder->radius);

    float near_t, far_t;
    bool solution_found = solve_quadratic(A, B, C, near_t, far_t);

    // Cylinder doesn't intersect with the segment on the ray
    bool out_bounds = !(near_t <= maxt && far_t >= mint); // NaN-aware conditionals

    float z_pos_near = oz + dz * near_t,
          z_pos_far  = oz + dz * far_t;

    // Cylinder fully contains the segment of the ray
    bool in_bounds = near_t < mint && far_t > maxt;

    bool valid_intersection =
        solution_found && !out_bounds && !in_bounds &&
        ((z_pos_near >= 0.f && z_pos_near <= cylinder->length && near_t >= mint) ||
         (z_pos_far  >= 0.f && z_pos_far  <= cylinder->length && far_t <= maxt));

    float t = (z_pos_near >= 0 && z_pos_near <= cylinder->length && near_t >= mint ? near_t : far_t);

    if (valid_intersection)
        optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
}

extern "C" __global__ void __closesthit__cylinder() {
    unsigned int launch_index = calculate_launch_index();

    if (params.out_hit != nullptr) {
        params.out_hit[launch_index] = true;
    } else {
        const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
        OptixCylinderData *cylinder = (OptixCylinderData *)sbt_data->data;

        /* Compute and store information describing the intersection. This is
           very similar to Cylinder::fill_surface_interaction() */

        Vector3f ray_o = make_vector3f(optixGetWorldRayOrigin());
        Vector3f ray_d = make_vector3f(optixGetWorldRayDirection());
        float t = optixGetRayTmax();

        Vector3f p = fmaf(t, ray_d, ray_o);

        Vector3f local = cylinder->to_object.transform_point(p);

        float phi = atan2(local.y(), local.x());
        if (phi < 0.f)
            phi += 2.f * M_PI;

        Vector2f uv = Vector2f(phi / (2.f * M_PI), local.z() / cylinder->length);

        Vector3f dp_du = 2.f * M_PI * Vector3f(-local.y(), local.x(), 0.f);
        Vector3f dp_dv = Vector3f(0.f, 0.f, cylinder->length);
        dp_du = cylinder->to_world.transform_vector(dp_du);
        dp_dv = cylinder->to_world.transform_vector(dp_dv);
        Vector3f ns = Vector3f(normalize(cross(dp_du, dp_dv)));

        /* Mitigate roundoff error issues by a normal shift of the computed
           intersection point */
        p += ns * (cylinder->radius - norm(Vector2f(local.x(), local.y())));

        if (cylinder->flip_normals)
            ns *= -1.f;

        Vector3f ng = ns;

        write_output_params(params, launch_index,
                            sbt_data->shape_ptr,
                            optixGetPrimitiveIndex(),
                            p, uv, ns, ng, dp_du, dp_dv, t);
    }
}
#endif
