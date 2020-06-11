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
    bool out_bounds = !(near_t <= ray.maxt && far_t >= ray.mint); // NaN-aware conditionals

    float z_pos_near = oz + dz * near_t,
          z_pos_far  = oz + dz * far_t;

    // Cylinder fully contains the segment of the ray
    bool in_bounds = near_t < ray.mint && far_t > ray.maxt;

    bool valid_intersection =
        solution_found && !out_bounds && !in_bounds &&
        ((z_pos_near >= 0.f && z_pos_near <= cylinder->length && near_t >= ray.mint) ||
         (z_pos_far  >= 0.f && z_pos_far  <= cylinder->length && far_t <= ray.maxt));

    float t = (z_pos_near >= 0 && z_pos_near <= cylinder->length && near_t >= ray.mint ? near_t : far_t);

    if (valid_intersection)
        optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
}

extern "C" __global__ void __closesthit__cylinder() {
    unsigned int launch_index = calculate_launch_index();

    if (params.is_ray_test()) {
        params.out_hit[launch_index] = true;
    } else {
        const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
        OptixCylinderData *cylinder = (OptixCylinderData *)sbt_data->data;

        // Ray in intance-space
        Ray3f ray = get_ray();

        // Early return for ray_intersect_preliminary call
        if (params.is_ray_intersect_preliminary()) {
            write_output_pi_params(params, launch_index, sbt_data->shape_ptr, 0, Vector2f(), ray.maxt);
            return;
        }

        /* Compute and store information describing the intersection. This is
           very similar to Cylinder::compute_surface_interaction() */

        Vector3f p = ray(ray.maxt);

        Vector3f local = cylinder->to_object.transform_point(p);

        float phi = atan2(local.y(), local.x());
        if (phi < 0.f)
            phi += TwoPi;

        Vector2f uv = Vector2f(phi * InvTwoPi, local.z() / cylinder->length);

        Vector3f ng, ns, dp_du, dp_dv, dn_du, dn_dv;

        dp_du = TwoPi * Vector3f(-local.y(), local.x(), 0.f);
        dp_dv = Vector3f(0.f, 0.f, cylinder->length);
        dp_du = cylinder->to_world.transform_vector(dp_du);
        dp_dv = cylinder->to_world.transform_vector(dp_dv);
        ns = Vector3f(normalize(cross(dp_du, dp_dv)));

        /* Mitigate roundoff error issues by a normal shift of the computed
        intersection point */
        p += ns * (cylinder->radius - norm(Vector2f(local.x(), local.y())));

        if (cylinder->flip_normals)
            ns *= -1.f;

        ng = ns;

        dn_du = dp_du / (cylinder->radius * (cylinder->flip_normals ? -1.f : 1.f));
        dn_dv = Vector3f(0.f);

        write_output_si_params(params, launch_index, sbt_data->shape_ptr,
                               0, p, uv, ns, ng, dp_du, dp_dv, dn_du, dn_dv, ray.maxt);
    }
}
#endif
