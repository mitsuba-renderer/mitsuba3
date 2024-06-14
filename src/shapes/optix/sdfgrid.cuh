#pragma once

#include <math.h>
#include <mitsuba/render/optix/common.h>

struct OptixSDFGridData {
    uint32_t* voxel_indices;
    uint32_t res_x;
    uint32_t res_y;
    uint32_t res_z;
    float voxel_size_x;
    float voxel_size_y;
    float voxel_size_z;
    float* grid_data;
    optix::Transform4f to_object;
};

#ifdef __CUDACC__

__device__ unsigned int vec_to_index(const Vector3u &vec,
                                     const OptixSDFGridData &sdf) {
    return vec[2] * sdf.res_y * sdf.res_x + vec[1] * sdf.res_x + vec[0];
}

__device__ Vector3u index_to_vec(unsigned int index,
                                 const OptixSDFGridData &sdf) {
    uint32_t x_len = sdf.res_x - 1;
    uint32_t y_len = sdf.res_y - 1;
    unsigned int x = index % x_len;
    unsigned int y = ((index - x) / y_len) % y_len;
    unsigned int z = (index - x - y * x_len) / (x_len * y_len);

    return Vector3u(x, y, z);
}

__device__ bool intersect_aabb(const Ray3f &ray,
                               const optix::BoundingBox3f &bbox, float &t_min,
                               float &t_max) {
    /**
      An Efficient and Robust Ray–Box Intersection Algorithm. Amy Williams et al. 2004.
    */
    const Vector3f d_rcp = frcp(ray.d);

    t_min = ((d_rcp.x() >= 0 ? bbox.min[0] : bbox.max[0]) - ray.o.x()) * d_rcp.x();
    t_max = ((d_rcp.x() >= 0 ? bbox.max[0] : bbox.min[0]) - ray.o.x()) * d_rcp.x();

    const float t_y_min = ((d_rcp.y() >= 0 ? bbox.min[1] : bbox.max[1]) - ray.o.y()) * d_rcp.y();
    const float t_y_max = ((d_rcp.y() >= 0 ? bbox.max[1] : bbox.min[1]) - ray.o.y()) * d_rcp.y();

    if ((t_min > t_y_max) || (t_y_min > t_max))
        return false;

    if (t_y_min > t_min || !isfinite(t_min))
        t_min = t_y_min;
    if (t_y_max < t_max || !isfinite(t_max))
        t_max = t_y_max;

    const float t_z_min = ((d_rcp.z() >= 0 ? bbox.min[2] : bbox.max[2]) - ray.o.z()) * d_rcp.z();
    const float t_z_max = ((d_rcp.z() >= 0 ? bbox.max[2] : bbox.min[2]) - ray.o.z()) * d_rcp.z();

    if ((t_min > t_z_max) || (t_z_min > t_max))
        return false;

    if (t_z_min > t_min || !isfinite(t_min))
        t_min = t_z_min;
    if (t_z_max < t_max || !isfinite(t_max))
        t_max = t_z_max;

    return true;
}

__device__ bool sdf_solve_cubic(float t_beg, float t_end, float c3, float c2,
                                float c1, float c0, float &t) {
    /**
      M ARMITT, G., K LEER , A., WALD , I., AND F RIEDRICH , H.
      2004. Fast and accurate ray-voxel intersection techniques for
      iso-surface ray tracing.
    */

    float root_0 = 0;
    float root_1 = 0;
    bool has_derivative_roots =
        solve_quadratic(c3 * 3, c2 * 2, c1, root_0, root_1);

    auto eval_sdf_t = [&](float t_) -> float {
        return fmaf(fmaf(fmaf(c3, t_, c2), t_, c1), t_, c0);
    };

    auto numerical_solve = [&](float t_near, float t_far, float f_near,
                               float f_far) -> float {
#define NUM_SOLVE_MAX_ITER 50
#define NUM_SOLVE_EPSILON 1e-5f

        float t = 0.f;
        float f_t = 0.f;

        unsigned int i = 0;
        bool done = false;
        while (!done) {
            t = t_near + (t_far - t_near) * (-f_near / (f_far - f_near));
            f_t = eval_sdf_t(t);
            if (f_t * f_near <= 0) { // Different signs
                t_far = t;
                f_far = f_t;
            } else {
                t_near = t;
                f_near = f_t;
            }
            done = (abs(t_far - t_near) < NUM_SOLVE_EPSILON) || (NUM_SOLVE_MAX_ITER < ++i);
        }

        return t;
    };

    float f_t_beg  = eval_sdf_t(t_beg);
    float f_t_end  = eval_sdf_t(t_end);

    float t_near = t_beg;
    float f_near = f_t_beg;
    float t_far = t_end;
    float f_far = f_t_end;

    if (has_derivative_roots) {
        if (t_near <= root_0 && root_0 <= t_far){
            float f_root_0 = eval_sdf_t(root_0);
            if (f_near * f_root_0 <= 0) { // Different signs
                t_far = root_0;
                f_far = f_root_0;
            } else {
                t_near = root_0;
                f_near = f_root_0;
            }
        }
        if (t_near <= root_1 && root_1 <= t_far){
            float f_root_1 = eval_sdf_t(root_1);
            if (f_near * f_root_1 <= 0) { // Different signs
                t_far = root_1;
                f_far = f_root_1;
            } else {
                t_near = root_1;
                f_near = f_root_1;
            }
        }
    }

    if (f_near * f_far > 0) // Same sign: no roots in the interval
        return false;

    t = numerical_solve(t_near, t_far, f_near, f_far);

    return true;
}

extern "C" __global__ void __intersection__sdfgrid() {
    const OptixHitGroupData *sbt_data =
        (OptixHitGroupData *) optixGetSbtDataPointer();
    OptixSDFGridData &sdf = *((OptixSDFGridData *) sbt_data->data);
    unsigned int voxel_index = sdf.voxel_indices[optixGetPrimitiveIndex()];

    Ray3f ray = get_ray();
    ray = sdf.to_object.transform_ray(ray); // object-space

    Vector3f bbox_min = index_to_vec(voxel_index, sdf);
    Vector3f bbox_max = bbox_min + Vector3f(1.f, 1.f, 1.f);
    Vector3f voxel_size(sdf.voxel_size_x, sdf.voxel_size_y, sdf.voxel_size_z);
    bbox_min *= voxel_size;
    bbox_max *= voxel_size;

    BoundingBox3f bbox_local;
    bbox_local.min[0] = bbox_min[0];
    bbox_local.min[1] = bbox_min[1];
    bbox_local.min[2] = bbox_min[2];
    bbox_local.max[0] = bbox_max[0];
    bbox_local.max[1] = bbox_max[1];
    bbox_local.max[2] = bbox_max[2];

    float t_bbox_beg = 0;
    float t_bbox_end = 0;
    bool bbox_its = intersect_aabb(ray, bbox_local, t_bbox_beg, t_bbox_end);
    // This should theoretically always hit, but OptiX might be a bit
    // less/more tight numerically hence some rays will miss
    if (!bbox_its) {
        return;
    }

    t_bbox_beg = max(t_bbox_beg, 0.f);
    if (t_bbox_end < t_bbox_beg) {
        return;
    }

    optix::Transform4f to_voxel;
    to_voxel.matrix[0][0] = (float) (sdf.res_x - 1);
    to_voxel.matrix[1][0] = 0.f;
    to_voxel.matrix[2][0] = 0.f;
    to_voxel.matrix[3][0] = 0.f;

    to_voxel.matrix[0][1] = 0.f;
    to_voxel.matrix[1][1] = (float) (sdf.res_y - 1);
    to_voxel.matrix[2][1] = 0.f;
    to_voxel.matrix[3][1] = 0.f;

    to_voxel.matrix[0][2] = 0.f;
    to_voxel.matrix[1][2] = 0.f;
    to_voxel.matrix[2][2] = (float) (sdf.res_z - 1);
    to_voxel.matrix[3][2] = 0.f;

    Vector3u voxel_position = index_to_vec(voxel_index, sdf);
    to_voxel.matrix[0][3] = -1.f * ((float) voxel_position.x());
    to_voxel.matrix[1][3] = -1.f * ((float) voxel_position.y());
    to_voxel.matrix[2][3] = -1.f * ((float) voxel_position.z());
    to_voxel.matrix[3][3] = 1.f;

    ray = to_voxel.transform_ray(ray); // voxel-space [0, 1] x [0, 1] x [0, 1]

    /**
       Herman Hansson-Söderlund, Alex Evans, and Tomas Akenine-Möller, Ray
       Tracing of Signed Distance Function Grids, Journal of Computer Graphics
       Techniques (JCGT), vol. 11, no. 3, 94-113, 2022
    */
    Vector3u v000 = voxel_position;
    Vector3u v100 = v000 + Vector3u(1, 0, 0);
    Vector3u v010 = v000 + Vector3u(0, 1, 0);
    Vector3u v110 = v000 + Vector3u(1, 1, 0);
    Vector3u v001 = v000 + Vector3u(0, 0, 1);
    Vector3u v101 = v000 + Vector3u(1, 0, 1);
    Vector3u v011 = v000 + Vector3u(0, 1, 1);
    Vector3u v111 = v000 + Vector3u(1, 1, 1);

    float s000 = sdf.grid_data[vec_to_index(v000, sdf)];
    float s100 = sdf.grid_data[vec_to_index(v100, sdf)];
    float s010 = sdf.grid_data[vec_to_index(v010, sdf)];
    float s110 = sdf.grid_data[vec_to_index(v110, sdf)];
    float s001 = sdf.grid_data[vec_to_index(v001, sdf)];
    float s101 = sdf.grid_data[vec_to_index(v101, sdf)];
    float s011 = sdf.grid_data[vec_to_index(v011, sdf)];
    float s111 = sdf.grid_data[vec_to_index(v111, sdf)];

    Vector3f ray_p_in_voxel = ray(t_bbox_beg);
    float o_x = ray_p_in_voxel.x();
    float o_y = ray_p_in_voxel.y();
    float o_z = ray_p_in_voxel.z();

    float d_x = ray.d.x();
    float d_y = ray.d.y();
    float d_z = ray.d.z();

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
    float m2 = fmaf(o_x, d_y, o_y * d_x);
    float m3 = fmaf(k5, o_z, -k1);
    float m4 = fmaf(k6, o_z, -k2);
    float m5 = fmaf(k7, o_z, -k3);

    float c0 = fmaf(k4, o_z, -k0) + fmaf(o_x, m3, fmaf(o_y, m4, m0 * m5));
    float c1 = fmaf(d_x, m3, d_y * m4) + m2 * m5 +
               d_z * (k4 + fmaf(k5, o_x, fmaf(k6, o_y, k7 * m0)));
    float c2 = fmaf(m1, m5, d_z * (fmaf(k5, d_x, fmaf(k6, d_y, k7 * m2))));
    float c3 = k7 * m1 * d_z;

    float t_beg = 0.f;
    float t_end = t_bbox_end - t_bbox_beg;

    if (c3 != 0) {
        // Cubic polynomial
        float t = 0;
        bool hit = sdf_solve_cubic(t_beg, t_end, c3, c2, c1, c0, t);

        if (hit && t_beg <= t && t <= t_end && t_bbox_beg + t < ray.maxt)
            optixReportIntersection(t_bbox_beg + t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
    } else {
        // Quadratic or linear polynomial
        float root_0;
        float root_1;
        bool hit = solve_quadratic(c2, c1, c0, root_0, root_1);

        if (hit && t_beg <= root_0 && root_0 <= t_end && t_bbox_beg + root_0 < ray.maxt)
            optixReportIntersection(t_bbox_beg + root_0, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
        else if (hit && t_beg <= root_1 && root_1 <= t_end && t_bbox_beg + root_1 < ray.maxt)
            optixReportIntersection(t_bbox_beg + root_1, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
    }
}

extern "C" __global__ void __closesthit__sdfgrid() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
    unsigned int prim_index = optixGetPrimitiveIndex();
    set_preliminary_intersection_to_payload(
        optixGetRayTmax(), Vector2f(), prim_index, sbt_data->shape_registry_id);
}
#endif
