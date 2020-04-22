#pragma once

struct HitGroupData {
    unsigned long long shape_ptr;
    void* faces;
    void* vertex_positions;
    void* vertex_normals;
    void* vertex_texcoords;
};

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
    uint32_t *out_primitive_id;

    bool *out_hit;

    OptixTraversableHandle handle;
};
