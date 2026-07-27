#pragma once

#ifdef __CUDACC__
// List all shape's CUDA header files to be included in the PTX code generation.
// Those header files are located in '/mitsuba/src/shapes/optix/'
#include "cylinder.cuh"
#include "disk.cuh"
#include "sdfgrid.cuh"
#include "sphere.cuh"
#include "ellipsoids.cuh"
#endif
