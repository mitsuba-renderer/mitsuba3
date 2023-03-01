#pragma once

#ifdef __CUDACC__
// List all shape's CUDA header files to be included in the PTX code generation.
// Those header files are located in '/mitsuba/src/shapes/optix/'
#include "cylinder.cuh"
#include "disk.cuh"
#include "mesh.cuh"
#include "rectangle.cuh"
#include "sphere.cuh"
#include "bsplinecurve.cuh"
#include "linearcurve.cuh"
#else

#include <drjit-core/optix.h>

#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/optix_api.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)
/// List of the custom shapes supported by OptiX
static std::string CUSTOM_OPTIX_SHAPE_NAMES[] = {
    "BSplineCurve", "LinearCurve", "Disk", "Rectangle", "Sphere", "Cylinder",
};
static constexpr size_t CUSTOM_OPTIX_SHAPE_COUNT = std::size(CUSTOM_OPTIX_SHAPE_NAMES);

/// Retrieve index of custom shape descriptor in the list above for a given shape
template <typename Shape>
size_t get_shape_descr_idx(Shape *shape) {
    std::string name = shape->class_()->name();
    for (size_t i = 0; i < CUSTOM_OPTIX_SHAPE_COUNT; i++) {
        if (CUSTOM_OPTIX_SHAPE_NAMES[i] == name)
            return i;
    }
    Throw("Unexpected shape: %s. Couldn't be found in the "
          "'CUSTOM_OPTIX_SHAPE_NAMES' table.", name);
}

/// Stores multiple OptiXTraversables: one for the each type
struct OptixAccelData {
    struct HandleData {
        OptixTraversableHandle handle = 0ull;
        void* buffer = nullptr;
        uint32_t count = 0u;
    };
    HandleData meshes;
    HandleData bspline_curves;
    HandleData linear_curves;
    HandleData custom_shapes;

    ~OptixAccelData() {
        if (meshes.buffer) jit_free(meshes.buffer);
        if (bspline_curves.buffer) jit_free(bspline_curves.buffer);
        if (linear_curves.buffer) jit_free(linear_curves.buffer);
        if (custom_shapes.buffer) jit_free(custom_shapes.buffer);
    }
};

/// Creates and appends the HitGroupSbtRecord for a given list of shapes
template <typename Shape>
void fill_hitgroup_records(std::vector<ref<Shape>> &shapes,
                           std::vector<HitGroupSbtRecord> &out_hitgroup_records,
                           const OptixProgramGroup *program_groups) {
    for (size_t i = 0; i < 4; i++) {
        for (Shape* shape: shapes) {
            switch (i) {
            case 0:
                if (shape->is_mesh())
                    shape->optix_fill_hitgroup_records(out_hitgroup_records, program_groups);
                break;
            case 1:
                if (shape->is_bspline_curve())
                    shape->optix_fill_hitgroup_records(out_hitgroup_records, program_groups);
                break;
            case 2:
                if (shape->is_linear_curve())
                    shape->optix_fill_hitgroup_records(out_hitgroup_records, program_groups);
                break;
            case 3:
                if (!shape->is_mesh() && !shape->is_curve())
                    shape->optix_fill_hitgroup_records(out_hitgroup_records, program_groups);
                break;
            }
        }
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
               OptixAccelData& out_accel) {

    // Separate geometry types
    std::vector<ref<Shape>> meshes, bspline_curves,
        linear_curves, custom_shapes;
    for (auto shape : shapes) {
        if (shape->is_mesh())
            meshes.push_back(shape);
        else if (shape->is_bspline_curve())
            bspline_curves.push_back(shape);
        else if (shape->is_linear_curve())
            linear_curves.push_back(shape);
        else if (!shape->is_instance())
            custom_shapes.push_back(shape);
    }

    // Build a GAS given a subset of shape pointers
    auto build_single_gas = [&context](const std::vector<ref<Shape>> &shape_subset,
                                       OptixAccelData::HandleData &handle) {

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

        size_t shapes_count = shape_subset.size();
        if (shapes_count == 0)
            return;

        std::vector<OptixBuildInput> build_inputs(shapes_count);
        for (size_t i = 0; i < shapes_count; i++)
            shape_subset[i]->optix_build_input(build_inputs[i]);

        // Ensure shape data pointers are fully evaluated before building the BVH
        dr::sync_thread();

        OptixAccelBufferSizes buffer_sizes;
        jit_optix_check(optixAccelComputeMemoryUsage(
            context,
            &accel_options,
            build_inputs.data(),
            (unsigned int) shapes_count,
            &buffer_sizes
        ));

        void* d_temp_buffer = jit_malloc(AllocType::Device, buffer_sizes.tempSizeInBytes);
        void* output_buffer = jit_malloc(AllocType::Device, buffer_sizes.outputSizeInBytes + 8);

        OptixAccelEmitDesc emit_property = {};
        emit_property.type   = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;
        emit_property.result = (CUdeviceptr)((char*)output_buffer + buffer_sizes.outputSizeInBytes);

        OptixTraversableHandle accel;
        jit_optix_check(optixAccelBuild(
            context,
            (CUstream) jit_cuda_stream(),
            &accel_options,
            build_inputs.data(),
            (unsigned int) shapes_count, // num build inputs
            (CUdeviceptr)d_temp_buffer,
            buffer_sizes.tempSizeInBytes,
            (CUdeviceptr)output_buffer,
            buffer_sizes.outputSizeInBytes,
            &accel,
            &emit_property,  // emitted property list
            1                // num emitted properties
        ));

        jit_free(d_temp_buffer);

        size_t compact_size;
        jit_memcpy(JitBackend::CUDA,
                   &compact_size,
                   (void*)emit_property.result,
                   sizeof(size_t));
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

    build_single_gas(meshes, out_accel.meshes);
    build_single_gas(bspline_curves, out_accel.bspline_curves);
    build_single_gas(linear_curves, out_accel.linear_curves);
    build_single_gas(custom_shapes, out_accel.custom_shapes);
}

/// Prepares and fills the \ref OptixInstance array associated with a given list of shapes.
template <typename Shape, typename Transform4f>
void prepare_ias(const OptixDeviceContext &context,
                 std::vector<ref<Shape>> &shapes,
                 uint32_t base_sbt_offset,
                 const OptixAccelData &accel,
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

    // Check whether transformation should be disabled on the IAS
    uint32_t flags = (transf == Transform4f())
                         ? OPTIX_INSTANCE_FLAG_DISABLE_TRANSFORM
                         : OPTIX_INSTANCE_FLAG_NONE;

    auto build_optix_instance = [&](const OptixAccelData::HandleData &handle) {
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

    build_optix_instance(accel.meshes);
    build_optix_instance(accel.bspline_curves);
    build_optix_instance(accel.linear_curves);
    build_optix_instance(accel.custom_shapes);

    // Apply the same process to every shape instances
    for (Shape* shape: shapes) {
        if (shape->is_instance())
            shape->optix_prepare_ias(
                context, out_instances,
                jit_registry_get_id(JitBackend::CUDA, shape), transf);
    }
}

NAMESPACE_END(mitsuba)
#endif
