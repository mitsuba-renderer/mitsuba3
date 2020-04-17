#include <optix.h>
#include <iostream>
#include "vector.cuh"
#include <math_constants.h>

struct Params {
    bool    *in_mask;
    float   *in_ox, *in_oy, *in_oz,
            *in_dx, *in_dy, *in_dz,
            *in_mint, *in_maxt;
    float   *out_t, *out_u, *out_v,
            *out_ng_x, *out_ng_y, *out_ng_z,
            *out_ns_x, *out_ns_y, *out_ns_z,
            *out_p_x, *out_p_y, *out_p_z,
            *out_dp_du_x, *out_dp_du_y, *out_dp_du_z,
            *out_dp_dv_x, *out_dp_dv_y, *out_dp_dv_z;

    unsigned long long *out_shape_ptr;
    unsigned int *out_primitive_id;

    bool *out_hit;

    OptixTraversableHandle handle;
    bool rg_any;
};


extern "C" {
__constant__ Params params;
}

struct HitGroupData {
    unsigned long long shape_ptr;
    Vector3ui* faces;
    Vector3f* vertex_positions;
    Vector3f* vertex_normals;
    Vector2f* vertex_texcoords;
};

__forceinline__ __device__ float3 make_float3(const Vector3f& v) {
    return make_float3(v.x(), v.y(), v.z());
}
__forceinline__ __device__ Vector3f make_Vector3f(const float3& v) {
    return Vector3f(v.x, v.y, v.z);
}

__device__ void coordinate_system(Vector3f n, Vector3f &x, Vector3f &y) {
    /* Based on "Building an Orthonormal Basis, Revisited" by
       Tom Duff, James Burgess, Per Christensen,
       Christophe Hery, Andrew Kensler, Max Liani,
       and Ryusuke Villemin (JCGT Vol 6, No 1, 2017) */

    float s = copysignf(1.f, n.z()),
          a = -1.f / (s + n.z()),
          b = n.x() * n.y() * a;

    x = Vector3f(n.x() * n.x() * a * s + 1.f, b * s, -n.x() * s);
    y = Vector3f(b, s + n.y() * n.y() * a, -n.y());
}

__device__ void ray_attr(
    const HitGroupData* sbt_data,
    Vector3f &p,
    Vector2f &uv,
    Vector3f &ns,
    Vector3f &ng,
    Vector3f &dp_du,
    Vector3f &dp_dv) {
    float2 float2_uv = optixGetTriangleBarycentrics();
    uv = Vector2f(float2_uv.x, float2_uv.y);
    float uv0 = 1.f - uv.x() - uv.y(),
          uv1 = uv.x(),
          uv2 = uv.y();

    const Vector3ui* faces            = sbt_data->faces;
    const Vector3f* vertex_positions  = sbt_data->vertex_positions;
    const Vector3f* vertex_normals    = sbt_data->vertex_normals;
    const Vector2f* vertex_texcoords  = sbt_data->vertex_texcoords;

    Vector3ui face = faces[optixGetPrimitiveIndex()];

    Vector3f p0 = vertex_positions[face.x()],
             p1 = vertex_positions[face.y()],
             p2 = vertex_positions[face.z()];

    Vector3f dp0 = p1 - p0,
             dp1 = p2 - p0;

    p = p0 * uv0 + p1 * uv1 + p2 * uv2;

    ng = normalize(cross(dp0, dp1));
    coordinate_system(ng, dp_du, dp_dv);

    if (vertex_normals != nullptr) {
        Vector3f n0 = vertex_normals[face.x()],
                 n1 = vertex_normals[face.y()],
                 n2 = vertex_normals[face.z()];

        ns = normalize(n0 * uv0 + n1 * uv1 + n2 * uv2);
    } else {
        ns = ng;
    }

    if (vertex_texcoords != nullptr) {
        Vector2f t0 = vertex_texcoords[face.x()],
                 t1 = vertex_texcoords[face.y()],
                 t2 = vertex_texcoords[face.z()];

        uv = t0 * uv0 + t1 * uv1 + t2 * uv2;

        Vector2f dt0 = t1 - t0, dt1 = t2 - t0;
        float det = dt0.x() * dt1.y() - dt0.y() * dt1.x();

        if (det != 0.f) {
            float inv_det = 1.f / det;
            dp_du = ( dt1.y() * dp0 - dt0.y() * dp1) * inv_det;
            dp_dv = (-dt1.x() * dp0 + dt0.x() * dp1) * inv_det;
        }
    }
}

extern "C" __global__ void __raygen__rg() {
    uint3 launch_dims = optixGetLaunchDimensions();
    uint3 launch_index3 = optixGetLaunchIndex();
    unsigned int launch_index = launch_index3.x + (launch_index3.y + launch_index3.z * launch_dims.y) * launch_dims.x;

    Vector3f ro = Vector3f(params.in_ox[launch_index],
                           params.in_oy[launch_index],
                           params.in_oz[launch_index]),
             rd = Vector3f(params.in_dx[launch_index],
                           params.in_dy[launch_index],
                           params.in_dz[launch_index]);
    float  mint = params.in_mint[launch_index],
           maxt = params.in_maxt[launch_index];

    if (params.rg_any) {
        if (!params.in_mask[launch_index]) {
            params.out_hit[launch_index] = false; // TODO: check if out_hit valid??
        } else {
            optixTrace(
                params.handle,
                make_float3(ro), make_float3(rd),
                mint, maxt, 0.0f,
                OptixVisibilityMask( 1 ),
                OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT,
                0, 1, 0
                );
        }
    } else {
        if (!params.in_mask[launch_index]) {
            params.out_shape_ptr[launch_index] = 0;
            params.out_t[launch_index] = CUDART_INF_F;
        } else {
            optixTrace(
                params.handle,
                make_float3(ro), make_float3(rd),
                mint, maxt, 0.0f,
                OptixVisibilityMask( 1 ),
                OPTIX_RAY_FLAG_NONE,
                0, 1, 0
                );
        }
    }
}

__device__ inline float squared_norm(Vector3f v) {
    return dot(v, v);
}

extern "C" __global__ void __closesthit__ch() {
    uint3 launch_dims = optixGetLaunchDimensions();
    uint3 launch_index3 = optixGetLaunchIndex();
    unsigned int launch_index = launch_index3.x + (launch_index3.y + launch_index3.z * launch_dims.y) * launch_dims.x;

    if (params.out_hit != nullptr) {
        params.out_hit[launch_index] = true;
    } else {
        Vector3f p;
        Vector2f uv;
        Vector3f ns;
        Vector3f ng;
        Vector3f dp_du;
        Vector3f dp_dv;
        const HitGroupData* sbt_data = (HitGroupData*)optixGetSbtDataPointer();

        ray_attr(sbt_data, p, uv, ns, ng, dp_du, dp_dv);

        params.out_shape_ptr[launch_index] = sbt_data->shape_ptr;

        params.out_primitive_id[launch_index] = optixGetPrimitiveIndex();

        params.out_u[launch_index] = uv.x();
        params.out_v[launch_index] = uv.y();

        params.out_ng_x[launch_index] = ng.x();
        params.out_ng_y[launch_index] = ng.y();
        params.out_ng_z[launch_index] = ng.z();

        params.out_ns_x[launch_index] = ns.x();
        params.out_ns_y[launch_index] = ns.y();
        params.out_ns_z[launch_index] = ns.z();

        params.out_p_x[launch_index] = p.x();
        params.out_p_y[launch_index] = p.y();
        params.out_p_z[launch_index] = p.z();

        params.out_dp_du_x[launch_index] = dp_du.x();
        params.out_dp_du_y[launch_index] = dp_du.y();
        params.out_dp_du_z[launch_index] = dp_du.z();

        params.out_dp_dv_x[launch_index] = dp_dv.x();
        params.out_dp_dv_y[launch_index] = dp_dv.y();
        params.out_dp_dv_z[launch_index] = dp_dv.z();

        Vector3f ray_o = make_Vector3f(optixGetWorldRayOrigin());
        Vector3f ray_d = make_Vector3f(optixGetWorldRayDirection());

        params.out_t[launch_index] = sqrt(squared_norm(p - ray_o) / squared_norm(ray_d));
    }
}

extern "C" __global__ void __miss__ms() {
    uint3 launch_dims = optixGetLaunchDimensions();
    uint3 launch_index3 = optixGetLaunchIndex();
    unsigned int launch_index = launch_index3.x + (launch_index3.y + launch_index3.z * launch_dims.y) * launch_dims.x;

    if (params.out_hit != nullptr) {
        params.out_hit[launch_index] = false;
    } else {
        params.out_shape_ptr[launch_index] = 0;
        params.out_t[launch_index] = CUDART_INF_F;
    }
}

struct OptixException
{
    int code;
    const char* string;
};

__constant__ OptixException exceptions[] = {
    { OPTIX_EXCEPTION_CODE_STACK_OVERFLOW, "OPTIX_EXCEPTION_CODE_STACK_OVERFLOW" },
    { OPTIX_EXCEPTION_CODE_TRACE_DEPTH_EXCEEDED, "OPTIX_EXCEPTION_CODE_TRACE_DEPTH_EXCEEDED" },
    { OPTIX_EXCEPTION_CODE_TRAVERSAL_DEPTH_EXCEEDED, "OPTIX_EXCEPTION_CODE_TRAVERSAL_DEPTH_EXCEEDED" },
    { OPTIX_EXCEPTION_CODE_TRAVERSAL_INVALID_TRAVERSABLE, "OPTIX_EXCEPTION_CODE_TRAVERSAL_INVALID_TRAVERSABLE" },
    { OPTIX_EXCEPTION_CODE_TRAVERSAL_INVALID_MISS_SBT, "OPTIX_EXCEPTION_CODE_TRAVERSAL_INVALID_MISS_SBT" },
    { OPTIX_EXCEPTION_CODE_TRAVERSAL_INVALID_HIT_SBT, "OPTIX_EXCEPTION_CODE_TRAVERSAL_INVALID_HIT_SBT" }
};

extern "C" __global__ void __exception__err() {
    int ex_code = optixGetExceptionCode();
    printf("Optix Exception %u: %s\n", ex_code, exceptions[ex_code].string);
    // TODO: retreive more informations based on exception
}
