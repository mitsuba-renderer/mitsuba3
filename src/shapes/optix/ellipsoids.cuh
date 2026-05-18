#pragma once

#include <math.h>
#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/optix/math.cuh>

enum IntersectionMode : uint32_t {
    SHELL     = 0, /// Set intersection distance to shell boundary
    PEAK      = 1, /// Set intersection distance to peak value
};

enum RayFlags : uint32_t {
    StochasticEllipsoids = 0x1000,
    GaussianEllipsoids   = 0x2000,
    // !!! KEEP IN SYNC WITH interaction.h !!!
};

struct OptixEllipsoidsData {
    optix::BoundingBox3f bbox;
    float* extents;
    float* opacity;
    IntersectionMode mode;
    float* data;
    unsigned int subprimitive_count;

#ifdef __CUDACC__
    DEVICE Vector3f center(unsigned int index) const {
        return Vector3f::load(&data[index * 10]);
    }

    DEVICE Vector3f scale(unsigned int index) const {
        return Vector3f::load(&data[index * 10 + 3]);
    }

    DEVICE Matrix3f rotation(unsigned int index) const {
        auto q = Vector4f::load(&data[index * 10 + 6]);
        q = q * SqrtTwo;
        float xx = q.x() * q.x(), yy = q.y() * q.y(), zz = q.z() * q.z(),
              xy = q.x() * q.y(), xz = q.x() * q.z(), yz = q.y() * q.z(),
              xw = q.x() * q.w(), yw = q.y() * q.w(), zw = q.z() * q.w();
        auto m = Matrix3f(); // Already transposed
        m[0] = Vector3f(1.f - (yy + zz), xy - zw, xz + yw);
        m[1] = Vector3f(xy + zw, 1.f - (xx + zz), yz - xw);
        m[2] = Vector3f(xz - yw,  yz + xw, 1.f - (xx + yy));
        return m;
    }
#endif
};

#ifdef __CUDACC__
DEVICE Vector3f peak_location(const Ray3f& ray,
                              const Vector3f& center,
                              const Matrix3f& rotation,
                              const Vector3f& scale) {
    // Find the peak location along the ray. From "3D Gaussian Ray Tracing"
    // Ray in instance-space
    Vector3f o = rotation.transposed_prod(ray.o - center) / scale;
    Vector3f d = rotation.transposed_prod(ray.d) / scale;
    float t_peak = -dot(o, d) / dot(d, d);
    return ray(t_peak);
}

DEVICE float kernel_eval_gaussian(const Vector3f& p_peak,
                                  const Vector3f& center,
                                  const Matrix3f& rotation,
                                  const Vector3f& s) {
    Vector3f p = rotation.transposed_prod(p_peak - center);
    return exp(-0.5f * ((p.x()*p.x()) / (s.x()*s.x()) + (p.y()*p.y()) / (s.y()*s.y()) + (p.z()*p.z()) / (s.z()*s.z())));
}

// Function to compute the fractional part of a number
DEVICE float fractional(float x) {
    return x - std::floor(x);
}

// Function to compute r1(q)
DEVICE float r1(float q, float a1, float b1) {
    return fractional(b1 * std::sin(a1 * q));
}

// Function to compute r2(q)
DEVICE float r2(const Vector2f& q, const Vector2f& a2, float b2) {
    return fractional(b2 * std::sin(dot(a2, q)));
}

// Function to generate a pseudo-random number
DEVICE float generate_random_number(const Vector3f& p) {
    float a1 = 91.3458;
    Vector2f a2 = {12.9898, 78.233};
    float b1 = 47453.5453;
    float b2 = 43758.5453;

    Vector2f p_xy = {p.x(), p.y()};
    float r1_val = r1(p.z(), a1, b1);
    Vector2f perturbed_p_xy = {p_xy.x() + r1_val, p_xy.y() + r1_val};
    return r2(perturbed_p_xy, a2, b2);
}

extern "C" __global__ void __intersection__ellipsoids() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData*) optixGetSbtDataPointer();
    OptixEllipsoidsData &ellipsoid = *((OptixEllipsoidsData *) sbt_data->data);

    unsigned int prim_index = optixGetPrimitiveIndex();
    Ray3f ray = get_ray();

    float t;
    bool active;

    Vector3f center   = ellipsoid.center(prim_index);
    Vector3f scale    = ellipsoid.scale(prim_index);
    Matrix3f rotation = ellipsoid.rotation(prim_index);
    float extent = ellipsoid.extents[prim_index];

    // Ray in instance-space
    Vector3f o = rotation.transposed_prod(ray.o - center);
    Vector3f d = rotation.transposed_prod(ray.d);
    o /= scale * extent;
    d /= scale * extent;

    if (ellipsoid.mode == IntersectionMode::PEAK) {
        // Compute closest approach to center in canonical space (from "3D Gaussian Ray Tracing", NVIDIA)
        t = -dot(o, d) / dot(d, d);

        // Compute minimum squared distance from ray to center in canonical space
        Vector3f peak_p = o + t * d;
        float squared_dist = dot(peak_p, peak_p);

        // Accept hit if within 3σ (already divided by extent)
        bool within_extent = (squared_dist < 1.f);

        active = within_extent && (t > 0.f) && (t < ray.maxt);
    } else {
        Ray3f ray_relative(o, d, ray.maxt, ray.time);

        // We define a plane which is perpendicular to the ray direction and
        // contains the ellipsoid center and intersect it. We then solve the
        // ray-ellipsoid intersection as if the ray origin was this new
        // intersection point. This additional step makes the whole intersection
        // routine numerically more robust.

        float plane_t = dot(-o, d) / norm(d);
        Vector3f plane_p = ray_relative(plane_t);

        // Ray is perpendicular to the origin-center segment,
        // and intersection with plane is outside of the sphere
        if (plane_t == 0.f && norm(plane_p) > 1.f)
            return;

        float A = squared_norm(d);
        float B = 2.f * dot(plane_p, d);
        float C = squared_norm(plane_p) - 1.f;

        float near_t, far_t;
        bool solution_found = solve_quadratic(A, B, C, near_t, far_t);

        // Adjust distances for plane intersection
        near_t += plane_t;
        far_t += plane_t;

        // Ellipsoid doesn't intersect with the segment on the ray
        bool out_bounds = !(near_t <= ray.maxt && far_t >= 0.f); // NaN-aware conditionals

        // Ellipsoid fully contains the segment of the ray
        bool in_bounds = near_t < 0.f && far_t > ray.maxt;

        // Ellipsoid is backfacing
        bool backfacing = near_t < 0.f;

        active = solution_found && !out_bounds && !in_bounds && !backfacing;

        t = (near_t < 0.f ? far_t: near_t);
    }

    unsigned int ray_flags = optixGetPayload_0();
    bool is_gaussian   = (ray_flags & (unsigned int) RayFlags::GaussianEllipsoids) != 0;
    bool is_stochastic = (ray_flags & (unsigned int) RayFlags::StochasticEllipsoids) != 0;

    // Discard intersection based on kernel density and primitive's opacity
    if (active && is_gaussian) {
        // Find the peak location along the ray. From "3D Gaussian Ray Tracing"
        float t_peak = -dot(o, d) / dot(d, d);
        Vector3f p_peak = ray(t_peak);

        float density = kernel_eval_gaussian(p_peak, center, rotation, scale);
        float opacity = ellipsoid.opacity[prim_index];
        float weight = min(density * opacity, 1.0);

        // Reject primitive if has insignificant contribution
        active &= weight > 0.01f;

        if (active && is_stochastic) {
            // Generate random number based on intersection position
            float sample = generate_random_number(ray(t));
            // Stochastic rejection of the primitive
            active &=  weight >= sample;
        }
    }

    if (active) {
        optixReportIntersection(t, OPTIX_HIT_KIND_TRIANGLE_FRONT_FACE);
    }
}

DEVICE void stochastic_intersection_mesh_ellipsoids(void* data,
                                                    unsigned int prim_index,
                                                    unsigned int ray_flags) {
    bool is_gaussian   = (ray_flags & (unsigned int) RayFlags::GaussianEllipsoids) != 0;
    bool is_stochastic = (ray_flags & (unsigned int) RayFlags::StochasticEllipsoids) != 0;

    // Do nothing is ellipsoid kernel type is not defined (e.g. backward pass)
    if (!is_gaussian)
        return;

    OptixEllipsoidsData &ellipsoid = *((OptixEllipsoidsData *) data);
    Ray3f ray = get_ray();

    // For EllipsoidsMesh, the prim_index represents the triangle index, and need to be mapped to primitive index
    prim_index /= ellipsoid.subprimitive_count;

    Vector3f center   = ellipsoid.center(prim_index);
    Matrix3f rotation = ellipsoid.rotation(prim_index);
    Vector3f scale    = ellipsoid.scale(prim_index);

    Vector3f p_peak = peak_location(ray, center, rotation, scale);

    float density = kernel_eval_gaussian(p_peak, center, rotation, scale);
    float opacity = ellipsoid.opacity[prim_index];
    float weight = min(density * opacity, 1.0);

    // Reject primitive if has insignificant contribution
    if (weight < 0.01f)
        return optixIgnoreIntersection();

    if (is_stochastic) {
        // ray.maxt is the value passed in to optixReportIntersection in intersection function above
        Vector3f p = ray(ray.maxt);
        float sample = generate_random_number(p);

        // Stochastic rejection of the primitive
        if (weight < sample)
            return optixIgnoreIntersection();
    }
}

#endif
