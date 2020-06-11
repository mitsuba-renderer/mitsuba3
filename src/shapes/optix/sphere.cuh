#pragma once

#include <math.h>
#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/optix/math.cuh>

struct OptixSphereData {
    optix::BoundingBox3f bbox;
    optix::Transform4f to_world;
    optix::Transform4f to_object;
    optix::Vector3f center;
    float radius;
    bool flip_normals;
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
    bool out_bounds = !(near_t <= ray.maxt && far_t >= ray.mint); // NaN-aware conditionals

    // Sphere fully contains the segment of the ray
    bool in_bounds = near_t < ray.mint && far_t > ray.maxt;

    float t = (near_t < ray.mint ? far_t: near_t);

    if (solution_found && !out_bounds && !in_bounds)
        optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
}


extern "C" __global__ void __closesthit__sphere() {
    unsigned int launch_index = calculate_launch_index();

    if (params.is_ray_test()) {
        params.out_hit[launch_index] = true;
    } else {
        const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
        OptixSphereData *sphere = (OptixSphereData *)sbt_data->data;

        // Ray in instance-space
        Ray3f ray = get_ray();

        // Early return for ray_intersect_preliminary call
        if (params.is_ray_intersect_preliminary()) {
            write_output_pi_params(params, launch_index, sbt_data->shape_ptr, 0, Vector2f(), ray.maxt);
            return;
        }

        /* Compute and store information describing the intersection. This is
           very similar to Sphere::compute_surface_interaction() */

        Vector3f ns = normalize(ray(ray.maxt) - sphere->center);

        if (sphere->flip_normals)
            ns = -ns;

        Vector3f ng = ns;

        // Re-project onto the sphere to improve accuracy
        Vector3f p = fmaf(sphere->radius, ns, sphere->center);

        Vector2f uv;
        Vector3f dp_du, dp_dv;
        if (params.has_uv()) {
            Vector3f local = sphere->to_object.transform_point(p);

            float rd_2  = sqr(local.x()) + sqr(local.y()),
                  theta = acos(local.z()),
                  phi   = atan2(local.y(), local.x());

            if (phi < 0.f)
                phi += TwoPi;

            uv = Vector2f(phi * InvTwoPi, theta * InvPi);

            if (params.has_dp_duv()) {
                dp_du = Vector3f(-local.y(), local.x(), 0.f);

                float rd      = sqrt(rd_2),
                      inv_rd  = 1.f / rd,
                      cos_phi = local.x() * inv_rd,
                      sin_phi = local.y() * inv_rd;

                dp_dv = Vector3f(local.z() * cos_phi,
                                local.z() * sin_phi,
                                -rd);

                // Check for singularity
                if (rd == 0.f)
                    dp_dv = Vector3f(1.f, 0.f, 0.f);

                dp_du = sphere->to_world.transform_vector(dp_du) * TwoPi;
                dp_dv = sphere->to_world.transform_vector(dp_dv) * Pi;
            }
        }

        float inv_radius = (sphere->flip_normals ? -1.f : 1.f) / sphere->radius;
        Vector3f dn_du = dp_du * inv_radius;
        Vector3f dn_dv = dp_dv * inv_radius;

        write_output_si_params(params, launch_index, sbt_data->shape_ptr,
                               0, p, uv, ns, ng, dp_du, dp_dv, dn_du, dn_dv, ray.maxt);
    }
}
#endif