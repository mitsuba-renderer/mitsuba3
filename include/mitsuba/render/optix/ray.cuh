#pragma once
#ifdef __CUDACC__

# include <optix.h>
#include <mitsuba/render/optix/vector.cuh>

namespace optix {

struct Ray3f {
    Vector3f o;         ///< Ray origin
    Vector3f d;         ///< Ray direction
    Vector3f d_rcp;     ///< Componentwise reciprocals of the ray direction
    float mint;         ///< Minimum position on the ray segment
    float maxt;         ///< Maximum position on the ray segment
    float time;         ///< Time value associated with this ray

    DEVICE Ray3f() {}

    /// Construct a new ray (o, d) with bounds
    DEVICE Ray3f(const Vector3f &o, const Vector3f &d, float mint, float maxt, float time)
        : o(o), d(d), d_rcp(frcp(d)), mint(mint), maxt(maxt), time(time) { }

    /// Return the position of a point along the ray
    DEVICE Vector3f operator() (float t) const { return fmaf(t, d, o); }

    /// Update the reciprocal ray directions after changing 'd'
    DEVICE void update() { d_rcp = frcp(d); }
};

/// Returns the current ray in instance space (based on the current instance transformation)
DEVICE Ray3f get_ray() {
    Ray3f ray;
    ray.o = make_vector3f(optixTransformPointFromWorldToObjectSpace(optixGetWorldRayOrigin()));
    ray.d = make_vector3f(optixTransformVectorFromWorldToObjectSpace(optixGetWorldRayDirection()));
    ray.mint = optixGetRayTmin();
    ray.maxt = optixGetRayTmax();
    ray.time = optixGetRayTime();
    ray.update();
    return ray;
}
} // end namespace optix
#endif // defined __CUDACC__