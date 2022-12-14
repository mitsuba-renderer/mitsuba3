#pragma once

#include <math.h>
#include <mitsuba/render/optix/common.h>

//FIXME: remove
#define printf false && printf

struct OptixSDFGridData {
    optix::BoundingBox3f* bboxes; // TODO: Can this computed on-the-fly ?
    size_t res_x;
    size_t res_y;
    size_t res_z;
    float* grid_data;
    optix::Transform4f to_object;
};

#ifdef __CUDACC__


__device__ unsigned int vec_to_index(const Vector3u &vec,
                                     const OptixSDFGridData &sdf) {
    return vec[2] * sdf.res_y * sdf.res_x + vec[1] * sdf.res_x + vec[0];
}

__device__ Vector3f index_to_vec(unsigned int index,
                                 const OptixSDFGridData &sdf) {
    unsigned int x = index % sdf.res_x;
    unsigned int y = ((index - x) / (sdf.res_y)) % sdf.res_y;
    unsigned int z = (index - x - y * sdf.res_x) / (sdf.res_y * sdf.res_x);

    return Vector3u(x, y, z);
}

__device__ bool intersect_aabb(const Ray3f &ray,
                               const optix::BoundingBox3f &bbox, float &t_min,
                               float &t_max) {
    /**
      An Efficient and Robust Ray–Box Intersection Algorithm. Amy Williams et al. 2004.
    */

    float t_y_min, t_y_max, t_z_min, t_z_max;
    Vector3f d_rcp = 1.f / ray.d;

    t_min  = ((d_rcp.x() < 0 ? bbox.max[0] : bbox.min[0]) - ray.o.x()) * d_rcp.x();
    t_max = ((d_rcp.x() < 0 ? bbox.min[0] : bbox.max[0]) - ray.o.x()) * d_rcp.x();
    t_y_min  = ((d_rcp.y() < 0 ? bbox.max[1] : bbox.min[1]) - ray.o.y()) * d_rcp.y();
    t_y_max = ((d_rcp.y() < 0 ? bbox.min[1] : bbox.max[1]) - ray.o.y()) * d_rcp.y();

    if ((t_min > t_y_max) || (t_y_min > t_max))
        return false;

    if (t_min < t_y_min)
        t_min = t_y_min;
    if (t_y_max < t_max)
        t_max = t_y_max;

    t_z_min  = ((d_rcp.z() < 0 ? bbox.max[2] : bbox.min[2]) - ray.o.z()) * d_rcp.z();
    t_z_max = ((d_rcp.z() < 0 ? bbox.min[2] : bbox.max[2]) - ray.o.z()) * d_rcp.z();

    if ((t_min > t_z_max) || (t_z_min > t_max))
        return false;

    if (t_z_min > t_min)
        t_min = t_z_min;
    if (t_z_max < t_max)
        t_max = t_z_max;

    return true;
}

__device__ void transform_bbox(BoundingBox3f &bbox,
                               const optix::Transform4f &transform) {
    Vector3f min = {
        bbox.min[0],
        bbox.min[1],
        bbox.min[2],
    };
    Vector3f max = {
        bbox.max[0],
        bbox.max[1],
        bbox.max[2],
    };
    min = transform.transform_point(min);
    max = transform.transform_point(max);

    bbox.min[0] = min[0];
    bbox.min[1] = min[1];
    bbox.min[2] = min[2];
    bbox.max[0] = max[0];
    bbox.max[1] = max[1];
    bbox.max[2] = max[2];
}

__device__ bool sdf_solve_quadratic(float a, float b, float c, float &root_0,
                                float &root_1) {
    float delta = b * b - 4 * a * c;
    if (delta < 0)
        return false;

    float sqrt_delta = sqrt(delta);
    root_0 = (-b - sqrt_delta) / (2 * a);
    root_1 = (-b + sqrt_delta) / (2 * a);
    if (root_0 > root_1) {
        float tmp = root_0;
        root_0 = root_1;
        root_1 = tmp;
    }

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
        sdf_solve_quadratic(c3 * 3, c2 * 2, c1, root_0, root_1);

    auto eval_sdf_t = [&](float t_) -> float {
        return -(c3 * t_ * t_ * t_ + c2 * t_ * t_ + c1 * t_ + c0);
    };

    auto numerical_solve = [&](float t_near, float t_far, float f_near,
                               float f_far) -> float {
#define NUM_SOLVE_MAX_ITER 1000
#define NUM_SOLVE_EPSILON 0.0001

        float t = 0.f;
        float f_t = 0.f;

        unsigned int i = 0;
        bool done = false;
        while (!done) {
            t = t_near + (t_far - t_near) * (-f_near / (f_far - f_near));
            f_t = eval_sdf_t(t);
            if (f_t * f_near <= 0) { // different signs
                t_far = t;
                f_far = f_t;
            } else {
                t_near = t;
                f_near = f_t;
            }
            done = (abs(f_t) < NUM_SOLVE_EPSILON) || (NUM_SOLVE_MAX_ITER < ++i);
        }

        return t;
    };

    // TODO only compute what is necessary
    float f_t_beg  = eval_sdf_t(t_beg);
    float f_root_0 = eval_sdf_t(root_0);
    float f_root_1 = eval_sdf_t(root_1);
    float f_t_end  = eval_sdf_t(t_end);

    float t_near = t_beg;
    float f_near = f_t_beg;
    float t_far = t_end;
    float f_far = f_t_end;
    if (has_derivative_roots) {
        if (t_near <= root_0 && root_0 <= t_far){
            if (f_near * f_root_0 <= 0) { // different signs
                t_far = root_0;
                f_far = f_root_0;
            } else {
                t_near = root_0;
                f_near = f_root_0;
            }
        }
        if (t_near <= root_1 && root_1 <= t_far){
            if (f_near * f_root_1 <= 0) { // different signs
                t_far = root_1;
                f_far = f_root_1;
            } else {
                t_near = root_1;
                f_near = f_root_1;
            }
        }
    }

    if (f_near * f_far > 0) // No roots in the interval
        return false;

    t = numerical_solve(t_near, t_far, f_near, f_far);

    return true;
}

extern "C" __global__ void __intersection__sdfgrid() {
    const OptixHitGroupData *sbt_data =
        (OptixHitGroupData *) optixGetSbtDataPointer();
    OptixSDFGridData &sdf = *((OptixSDFGridData *) sbt_data->data);
    unsigned int prim_index = optixGetPrimitiveIndex();

    Ray3f ray = get_ray();
    ray = sdf.to_object.transform_ray(ray); // object-space

    BoundingBox3f bbox_local = sdf.bboxes[prim_index];
    transform_bbox(bbox_local, sdf.to_object);

    float t_beg = 0;
    float t_end = 0;
    bool bbox_its = intersect_aabb(ray, bbox_local, t_beg, t_end);
    // This should theoretically always hit, but OptiX might be a bit
    // less/more tight numerically hence some rays will miss
    if (!bbox_its)
        return;

    /**
       Herman Hansson-Söderlund, Alex Evans, and Tomas Akenine-Möller, Ray
       Tracing of Signed Distance Function Grids, Journal of Computer Graphics
       Techniques (JCGT), vol. 11, no. 3, 94-113, 2022
    */
    Vector3u v000 = index_to_vec(prim_index, sdf);
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

    float o_x = ray.o.x();
    float o_y = ray.o.y();
    float o_z = ray.o.z();
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
    float m2 = o_x * d_y + o_y * d_x;
    float m3 = k5 * o_z - k1;
    float m4 = k6 * o_z - k2;
    float m5 = k7 * o_z - k3;

    float c0 = (k4 * o_z - k0) + o_x * m3 + o_y * m4 + m0 * m5;
    float c1 = d_x * m3 + d_y * m4 + m2 * m5 +
               d_z * (k4 + k5 * o_x + k6 * o_y + k7 * m0);
    float c2 = m1 * m5 + d_z * (k5 * d_x + k6 * d_y + k7 * m2);
    float c3 = k7 * m1 * d_z;

    auto eval_sdf = [&](float t_) -> float {
        return -(c3 * t_ * t_ * t_ + c2 * t_ * t_ + c1 * t_ + c0);
    };

    // Avoid leaking through cracks
    if (eval_sdf(t_beg) < 0) {
        optixReportIntersection(t_beg, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
        return;
    }

    if (c3 != 0) {
        // Cubic polynomial
        float t = 0;
        bool hit = sdf_solve_cubic(t_beg, t_end, c3, c2, c1, c0, t);

        if (hit && t_beg <= t && t <= t_end)
            optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
        printf("t_beg: %f\n", t_beg);
        printf("t_end: %f\n", t_end);
        printf("Cubic: %f\n\n", t);
    } else if (c2 != 0) {
        // Quadratic polynomial
        float root_0;
        float root_1;
        sdf_solve_quadratic(c2, c1, c0, root_0, root_1);

        if (t_beg <= root_0 && root_0 <= t_end)
            optixReportIntersection(root_0, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
        else if (t_beg <= root_1 && root_1 <= t_end)
            optixReportIntersection(root_1, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
    } else if (c1 != 0) {
        // Linear polynomial
        float t = -c0 / c1;

        if (t_beg <= t && t <= t_end)
            optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
    }
}

extern "C" __global__ void __closesthit__sdfgrid() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
    unsigned int prim_index = optixGetPrimitiveIndex();
    set_preliminary_intersection_to_payload(
        optixGetRayTmax(), Vector2f(), prim_index, sbt_data->shape_registry_id);
}
#endif
