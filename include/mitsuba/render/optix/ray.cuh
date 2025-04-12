#pragma once
#ifdef __CUDACC__

# include <optix.h>
#include <mitsuba/render/optix/vector.cuh>

namespace optix {

struct Ray3f {
    Vector3f o;         /// Ray origin
    Vector3f d;         /// Ray direction
    float mint;         /// Minimum position on the ray segment
    float maxt;         /// Maximum position on the ray segment
    float time;         /// Time value associated with this ray

    DEVICE Ray3f() {}

    /// Construct a new ray (o, d) with bounds
    DEVICE Ray3f(const Vector3f &o, const Vector3f &d, float maxt, float time)
        : o(o), d(d), maxt(maxt), time(time) { }

    /// Return the position of a point along the ray
    DEVICE Vector3f operator() (float t) const { return fmaf(t, d, o); }
};

/// Returns the current ray in instance space (based on the current instance transformation)
DEVICE Ray3f get_ray() {
    Ray3f ray;
    ray.o = make_vector3f(optixGetObjectRayOrigin());
    ray.d = make_vector3f(optixGetObjectRayDirection());
    ray.mint = 0.0f; // Rays cast by Mitsuba always have mint==0.
    ray.maxt = optixGetRayTmax();
    ray.time = optixGetRayTime();
    return ray;
}
} // end namespace optix
#endif // defined __CUDACC__
