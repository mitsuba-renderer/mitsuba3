#include <math_constants.h>

#include <mitsuba/render/optix/matrix.cuh>
#include <mitsuba/render/optix/common.h>

// Include all shapes CUDA headers to generate their PTX programs
#include <mitsuba/render/optix/shapes.h>

enum class ShapeType : uint32_t {
    Mesh            = 1u << 0,
    Rectangle       = Mesh | (1u << 1),
    BSplineCurve    = 1u << 2,
    LinearCurve     = 1u << 3,
    Cylinder        = 1u << 4,
    Disk            = 1u << 5,
    SDFGrid         = 1u << 6,
    Sphere          = 1u << 7,
    Ellipsoids      = 1u << 8,
    EllipsoidsMesh  = Mesh | (1u << 9),
    Instance        = 1u << 10,
    ShapeGroup      = 1u << 11,
    // IMPORTANT: Keep in sync with shape.h!
};

extern "C" __global__ void __anyhit__intersection_check() {
    const OptixHitGroupData *sbt_data = (OptixHitGroupData *) optixGetSbtDataPointer();
    unsigned int shape_type  = sbt_data->shape_type;
    unsigned int prim_index  = optixGetPrimitiveIndex();
    unsigned int ray_flags   = optixGetPayload_0();

    if (shape_type == (unsigned int) ShapeType::EllipsoidsMesh) {
        stochastic_intersection_mesh_ellipsoids(sbt_data->data, prim_index, ray_flags);
    }
}
