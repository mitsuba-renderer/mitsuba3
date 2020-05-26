#pragma once

#ifdef __CUDACC__
// List all shape's CUDA header files to be included in the PTX code generation.
// Those header files are located in '/mitsuba/src/shapes/optix/'
#include "cylinder.cuh"
#include "disk.cuh"
#include "mesh.cuh"
#include "rectangle.cuh"
#include "sphere.cuh"
#else
NAMESPACE_BEGIN(mitsuba)
/// List of the custom shapes supported by OptiX
static std::string custom_optix_shapes[] = {
    "Disk", "Rectangle", "Sphere", "Cylinder",
};
static constexpr size_t custom_optix_shapes_count = std::size(custom_optix_shapes);

/// Retrieve index of custom shape descriptor in the list above for a given shape
template <typename Shape>
size_t get_shape_descr_idx(Shape *shape) {
    std::string name = shape->class_()->name();
    for (size_t i = 0; i < custom_optix_shapes_count; i++) {
        if (custom_optix_shapes[i] == name)
            return i;
    }
    Throw("Unexpected shape: %s. Couldn't be found in the "
          "'custom_optix_shapes' table.", name);
}
NAMESPACE_END(mitsuba)
#endif
