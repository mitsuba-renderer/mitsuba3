#pragma once

#ifdef __CUDACC__
// List all shape's CUDA header files to be included in the PTX code generation.
// Those header files are located in '/mitsuba/src/shapes/optix/'
#include "cylinder.cuh"
#include "disk.cuh"
#include "sdfgrid.cuh"
#include "sphere.cuh"
#include "ellipsoids.cuh"
#else

#include <drjit-core/optix.h>

#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/optix_api.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

/// Stores multiple OptiXTraversables: one for the each type
struct MiOptixAccelData {
    struct HandleData {
        OptixTraversableHandle handle = 0ull;
        void* buffer = nullptr;
        uint32_t count = 0u;
    };
    HandleData meshes;
    HandleData ellipsoids_meshes; // Separate from `meshes`, as we'll want to enable face culling
    HandleData bspline_curves;
    HandleData linear_curves;
    HandleData custom_shapes;

    ~MiOptixAccelData() {
        if (meshes.buffer) jit_free(meshes.buffer);
        if (ellipsoids_meshes.buffer) jit_free(ellipsoids_meshes.buffer);
        if (bspline_curves.buffer) jit_free(bspline_curves.buffer);
        if (linear_curves.buffer) jit_free(linear_curves.buffer);
        if (custom_shapes.buffer) jit_free(custom_shapes.buffer);
    }
};

/// Highest bit that may be set in a ShapeType flag vector
#define MI_SHAPE_TYPE_HIGHEST_BIT 10

// Map ShapeType to an OptiX program group
struct OptixProgramGroupMapping {
    uint32_t mapping[MI_SHAPE_TYPE_HIGHEST_BIT];

    OptixProgramGroupMapping() {
        for (uint32_t i = 0; i < MI_SHAPE_TYPE_HIGHEST_BIT; ++i)
            mapping[i] = (uint32_t) -1;
    }

    uint32_t index(ShapeType type) const {
        uint32_t index = dr::tzcnt((uint32_t) type);
        if (index >= MI_SHAPE_TYPE_HIGHEST_BIT)
            throw std::runtime_error("OptixParamGroupMapping: invalid shape type!");
        return index;
    }

    const uint32_t &operator[](ShapeType type) const { return mapping[index(type)]; }
    uint32_t &operator[](ShapeType type) { return mapping[index(type)]; }

    uint32_t at(ShapeType type) const {
        uint32_t value = operator[](type);
        if (value == (uint32_t) -1)
            throw std::runtime_error("OptixParamGroupMapping: shape type not mapped!");
        return value;
    }
};


/// Creates and appends the HitGroupSbtRecord for a given list of shapes
template <typename Shape>
void fill_hitgroup_records(
    std::vector<ref<Shape>> &shapes,
    std::vector<HitGroupSbtRecord> &out_hitgroup_records,
    const OptixProgramGroup *pg,
    const OptixProgramGroupMapping& pg_mapping) {

    // In order to match the IAS indexing (see `prepare_ias()`), records must be
    // filled out in the following order: meshes, ellipsoids meshes, bspline
    // curves, linear curves, custom shapes
    struct {
        size_t idx(const ref<Shape>& shape) const {
            uint32_t type = (uint32_t) shape->shape_type();
            if (type & ShapeType::Mesh) {
                if (type != +ShapeType::EllipsoidsMesh)
                    return 0;
                else
                    return 1;
            } else if (type & ShapeType::BSplineCurve) {
                return 2;
            } else if (type & ShapeType::LinearCurve) {
                return 3;
            } else {
                return 4;
            }
        };

        bool operator()(const ref<Shape> &a, const ref<Shape> &b) const {
            return idx(a) < idx(b);
        }
    } ShapeSorter;

    std::vector<ref<Shape>> shapes_sorted(shapes.size());
    std::copy(shapes.begin(), shapes.end(), shapes_sorted.begin());
    std::stable_sort(shapes_sorted.begin(), shapes_sorted.end(), ShapeSorter);

    for (ref<Shape> &shape : shapes_sorted) {
        shape->optix_fill_hitgroup_records(out_hitgroup_records, pg,
                                           pg_mapping);
    }
}

/**
 * \brief Build OptiX geometry acceleration structures (GAS) for a given list of shapes.
 *
 * Two different GAS will be created for the meshes and the custom shapes. Optix
 * handles to those GAS will be stored in an \ref OptixAccelData.
 */
template <typename Shape>
void build_gas(const OptixDeviceContext &context,
               const std::vector<ref<Shape>> &shapes,
               MiOptixAccelData& out_accel) {

    // Separate geometry types
    std::vector<ref<Shape>> meshes, ellipsoids_meshes, bspline_curves,
        linear_curves, custom_shapes;
    for (auto shape : shapes) {
        uint32_t type = (uint32_t) shape->shape_type();
        if (type & ShapeType::Mesh && type != +ShapeType::EllipsoidsMesh)
            meshes.push_back(shape);
        else if (type & ShapeType::EllipsoidsMesh)
            ellipsoids_meshes.push_back(shape);
        else if (type & ShapeType::BSplineCurve)
            bspline_curves.push_back(shape);
        else if (type & ShapeType::LinearCurve)
            linear_curves.push_back(shape);
        else if (!shape->is_instance())
            custom_shapes.push_back(shape);
    }

    // Build a GAS given a subset of shape pointers
    auto build_single_gas = [&context](const std::vector<ref<Shape>> &shape_subset,
                                       MiOptixAccelData::HandleData &handle) {
        size_t shapes_count = shape_subset.size();
        if (shapes_count == 0)
            return;

        OptixAccelBuildOptions accel_options = {};
        accel_options.buildFlags = OPTIX_BUILD_FLAG_ALLOW_COMPACTION |
                                   OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
        accel_options.operation  = OPTIX_BUILD_OPERATION_BUILD;
        accel_options.motionOptions.numKeys = 0;

        if (handle.buffer) {
            jit_free(handle.buffer);
            handle.handle = 0ull;
            handle.buffer = nullptr;
            handle.count = 0;
        }

        std::vector<OptixBuildInput> build_inputs(shapes_count);
        for (size_t i = 0; i < shapes_count; i++)
            shape_subset[i]->optix_build_input(build_inputs[i]);

        OptixAccelBufferSizes buffer_sizes;
        jit_optix_check(optixAccelComputeMemoryUsage(
            context,
            &accel_options,
            build_inputs.data(),
            (unsigned int) shapes_count,
            &buffer_sizes
        ));

        void* d_temp_buffer = jit_malloc(AllocType::Device, buffer_sizes.tempSizeInBytes);
        void* output_buffer = jit_malloc(AllocType::Device, buffer_sizes.outputSizeInBytes);
        void* compact_size_buffer = jit_malloc(AllocType::Device, 8);

        OptixAccelEmitDesc emit_property = {};
        emit_property.type   = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;
        emit_property.result = (CUdeviceptr) compact_size_buffer; // needs to be aligned

        OptixTraversableHandle accel;
        jit_optix_check(optixAccelBuild(
            context,
            (CUstream) jit_cuda_stream(),
            &accel_options,
            build_inputs.data(),
            (unsigned int) shapes_count,
            (CUdeviceptr) d_temp_buffer,
            buffer_sizes.tempSizeInBytes,
            (CUdeviceptr) output_buffer,
            buffer_sizes.outputSizeInBytes,
            &accel,
            &emit_property,  // emitted property list
            1                // num emitted properties
        ));

        jit_free(d_temp_buffer);

        size_t compact_size;
        jit_memcpy(JitBackend::CUDA,
                   &compact_size,
                   (void*) emit_property.result,
                   sizeof(size_t));
        jit_free(emit_property.result);

        if (compact_size < buffer_sizes.outputSizeInBytes) {
            void* compact_buffer = jit_malloc(AllocType::Device, compact_size);
            // Use handle as input and output
            jit_optix_check(optixAccelCompact(
                context,
                (CUstream) jit_cuda_stream(),
                accel,
                (CUdeviceptr) compact_buffer,
                compact_size,
                &accel
            ));
            jit_free(output_buffer);
            output_buffer = compact_buffer;
        }

        handle.handle = accel;
        handle.buffer = output_buffer;
        handle.count = (uint32_t) shapes_count;
    };

    scoped_optix_context guard;

    // The order of the following function calls does NOT matter
    build_single_gas(meshes, out_accel.meshes);
    build_single_gas(ellipsoids_meshes, out_accel.ellipsoids_meshes);
    build_single_gas(bspline_curves, out_accel.bspline_curves);
    build_single_gas(linear_curves, out_accel.linear_curves);
    build_single_gas(custom_shapes, out_accel.custom_shapes);
}

/// Prepares and fills the \ref OptixInstance array associated with a given list of shapes.
template <typename Shape, typename Transform4f>
void prepare_ias(const OptixDeviceContext &context,
                 std::vector<ref<Shape>> &shapes,
                 uint32_t base_sbt_offset,
                 const MiOptixAccelData &accel,
                 uint32_t instance_id,
                 const Transform4f& transf,
                 std::vector<OptixInstance> &out_instances) {
    unsigned int sbt_offset = base_sbt_offset;

    float T[12] = { (float) transf.matrix(0, 0), (float) transf.matrix(0, 1),
                    (float) transf.matrix(0, 2), (float) transf.matrix(0, 3),
                    (float) transf.matrix(1, 0), (float) transf.matrix(1, 1),
                    (float) transf.matrix(1, 2), (float) transf.matrix(1, 3),
                    (float) transf.matrix(2, 0), (float) transf.matrix(2, 1),
                    (float) transf.matrix(2, 2), (float) transf.matrix(2, 3) };

    auto build_optix_instance = [&](const MiOptixAccelData::HandleData &handle,
                                    bool disable_face_culling = true) {

        // Here we are forcing backface culling to be disabled for meshes other
        // than EllipsoidsMeshes.
        uint32_t flags = disable_face_culling ?
                         OPTIX_INSTANCE_FLAG_DISABLE_TRIANGLE_FACE_CULLING :
                         OPTIX_INSTANCE_FLAG_NONE;

        if (handle.handle) {
            OptixInstance instance = {
                { T[0], T[1], T[2], T[3], T[4], T[5], T[6], T[7], T[8], T[9], T[10], T[11] },
                instance_id, sbt_offset, /* visibilityMask = */ 255,
                flags, handle.handle, /* pads = */ { 0, 0 }
            };
            out_instances.push_back(instance);
            sbt_offset += (unsigned int) handle.count;
        }
    };

    // The order matters here, as it defines the SBT offsets. We packed them in
    // the following order: meshes, ellipsoids_meshes, b-spline curves,
    // linear curves, custom shapes
    build_optix_instance(accel.meshes, /* disable_face_culling */ true);
    build_optix_instance(accel.ellipsoids_meshes, /* disable_face_culling */ false);
    build_optix_instance(accel.bspline_curves);
    build_optix_instance(accel.linear_curves);
    build_optix_instance(accel.custom_shapes);

    // Apply the same process to every shape instance: each instance will query
    // its group's geometry acceleration structure(s) and add them as an
    // `OptixInstance` to `out_instances`. Effectively, this is flattening the
    // tree of shapes into a single level of instances.
    for (Shape* shape: shapes) {
        if (shape->is_instance())
            shape->optix_prepare_ias(
                context, out_instances,
                jit_registry_id(shape), transf);
    }
}

NAMESPACE_END(mitsuba)
#endif
