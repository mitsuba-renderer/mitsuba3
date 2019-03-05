#include <optix_world.h>
#include <math_constants.h>

using namespace optix;

rtDeclareVariable(rtObject, top_object, , );
rtDeclareVariable(unsigned long long, shape_ptr, , );
rtDeclareVariable(unsigned int, launch_index, rtLaunchIndex, );

rtDeclareVariable(float3, p, attribute p, );
rtDeclareVariable(float2, uv, attribute uv, );
rtDeclareVariable(float3, ns, attribute ns, );
rtDeclareVariable(float3, ng, attribute ng, );
rtDeclareVariable(float3, dp_du, attribute dp_du, );
rtDeclareVariable(float3, dp_dv, attribute dp_dv, );
rtDeclareVariable(Ray, ray, rtCurrentRay,);

rtBuffer<bool> in_mask;

rtBuffer<float> in_ox, in_oy, in_oz,
                in_dx, in_dy, in_dz,
                in_mint, in_maxt;

rtBuffer<float> out_t, out_u, out_v, out_ng_x, out_ng_y,
                out_ng_z, out_ns_x, out_ns_y, out_ns_z,
                out_p_x, out_p_y, out_p_z,
                out_dp_du_x, out_dp_du_y, out_dp_du_z,
                out_dp_dv_x, out_dp_dv_y, out_dp_dv_z;

rtBuffer<unsigned long long> out_shape_ptr;

rtBuffer<uint32_t> out_primitive_id;

rtBuffer<bool> out_hit;

struct PerRayData { };

rtDeclareVariable(PerRayData, prd, rtPayload, );

RT_PROGRAM void ray_gen_closest() {
    float3 ro = make_float3(in_ox[launch_index],
                            in_oy[launch_index],
                            in_oz[launch_index]),
           rd = make_float3(in_dx[launch_index],
                            in_dy[launch_index],
                            in_dz[launch_index]);
    float  mint = in_mint[launch_index],
           maxt = in_maxt[launch_index];

    if (!in_mask[launch_index]) {
        out_shape_ptr[launch_index] = 0;
        out_t[launch_index] = CUDART_INF_F;
    } else {
        PerRayData prd;
        Ray ray = make_Ray(ro, rd, 0, mint, maxt);
        rtTrace(top_object, ray, prd);
    }
}

RT_PROGRAM void ray_gen_any() {
    float3 ro = make_float3(in_ox[launch_index],
                            in_oy[launch_index],
                            in_oz[launch_index]),
           rd = make_float3(in_dx[launch_index],
                            in_dy[launch_index],
                            in_dz[launch_index]);
    float  mint = in_mint[launch_index],
           maxt = in_maxt[launch_index];

    Ray ray = make_Ray(ro, rd, 0, mint, maxt);

    if (!in_mask[launch_index]) {
        out_hit[launch_index] = false;
    } else {
        PerRayData prd;
        rtTrace(top_object, ray, prd, RT_VISIBILITY_ALL,
                RT_RAY_FLAG_TERMINATE_ON_FIRST_HIT);
    }
}

__device__ inline float squared_norm(float3 v) {
    return dot(v, v);
}

RT_PROGRAM void ray_hit() {
    if (out_hit.size() > 0) {
        out_hit[launch_index] = true;
    } else {
        out_shape_ptr[launch_index] = shape_ptr;

        out_primitive_id[launch_index] = rtGetPrimitiveIndex();

        out_u[launch_index] = uv.x;
        out_v[launch_index] = uv.y;

        out_ng_x[launch_index] = ng.x;
        out_ng_y[launch_index] = ng.y;
        out_ng_z[launch_index] = ng.z;

        out_ns_x[launch_index] = ns.x;
        out_ns_y[launch_index] = ns.y;
        out_ns_z[launch_index] = ns.z;

        out_p_x[launch_index] = p.x;
        out_p_y[launch_index] = p.y;
        out_p_z[launch_index] = p.z;

        out_dp_du_x[launch_index] = dp_du.x;
        out_dp_du_y[launch_index] = dp_du.y;
        out_dp_du_z[launch_index] = dp_du.z;

        out_dp_dv_x[launch_index] = dp_dv.x;
        out_dp_dv_y[launch_index] = dp_dv.y;
        out_dp_dv_z[launch_index] = dp_dv.z;

        out_t[launch_index] = sqrt(squared_norm(p - ray.origin) / squared_norm(ray.direction));
    }
}

RT_PROGRAM void ray_miss() {
    if (out_hit.size() > 0) {
        out_hit[launch_index] = false;
    } else {
        out_shape_ptr[launch_index] = 0;
        out_t[launch_index] = CUDART_INF_F;
    }
}

RT_PROGRAM void ray_err() {
    rtPrintExceptionDetails();
}
