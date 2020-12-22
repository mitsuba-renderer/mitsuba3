
#include <iomanip>
#include <stdio.h>

#include <enoki-jit/optix.h>

#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/optix/shapes.h>
#include <mitsuba/render/optix_api.h>

#include "librender_ptx.h"

NAMESPACE_BEGIN(mitsuba)

#if !defined(NDEBUG)
# define MTS_OPTIX_DEBUG 1
#endif

#ifdef __WINDOWS__
# define strdup _strdup
#endif

static constexpr size_t ProgramGroupCount = 2 + custom_optix_shapes_count;

template <typename Float>
struct OptixState {
    OptixDeviceContext context;
    OptixPipelineCompileOptions pipeline_compile_options = {};
    OptixModule module = nullptr;
    OptixProgramGroup program_groups[ProgramGroupCount];
    OptixShaderBindingTable sbt = {};
    OptixAccelData accel;
    OptixTraversableHandle ias_handle = 0ull;
    void* ias_buffer = nullptr;

    char *custom_shapes_program_names[2 * custom_optix_shapes_count];
};

MTS_VARIANT void Scene<Float, Spectrum>::accel_init_gpu(const Properties &/*props*/) {
    if constexpr (ek::is_cuda_array_v<Float>) {
        ScopedPhase phase(ProfilerPhase::InitAccel);
        Log(Info, "Building scene in OptiX ..");

        using OptixState = OptixState<Float>;
        m_accel = new OptixState();
        OptixState &s = *(OptixState *) m_accel;

        bool scene_has_custom_shapes = false;
        bool scene_has_instances = false;
        for (auto& s : m_shapes) {
            scene_has_custom_shapes |= !s->is_mesh() && !s->is_instance();
            scene_has_instances     |= s->is_instance();
        }
        for (auto& s : m_shapegroups)
            scene_has_custom_shapes |= !s->has_meshes_only();

        // =====================================================
        // OptiX context creation
        // =====================================================

        s.context = jitc_optix_context();

        // =====================================================
        // Configure options for OptiX pipeline
        // =====================================================

        OptixModuleCompileOptions module_compile_options = {};
        module_compile_options.maxRegisterCount = OPTIX_COMPILE_DEFAULT_MAX_REGISTER_COUNT;
    #if !defined(MTS_OPTIX_DEBUG)
        module_compile_options.optLevel         = OPTIX_COMPILE_OPTIMIZATION_DEFAULT;
        module_compile_options.debugLevel       = OPTIX_COMPILE_DEBUG_LEVEL_NONE;
    #else
        module_compile_options.optLevel         = OPTIX_COMPILE_OPTIMIZATION_LEVEL_0;
        module_compile_options.debugLevel       = OPTIX_COMPILE_DEBUG_LEVEL_LINEINFO;
    #endif

        s.pipeline_compile_options.usesMotionBlur        = false;
        s.pipeline_compile_options.numPayloadValues      = 6;
        s.pipeline_compile_options.numAttributeValues    = 0;
        s.pipeline_compile_options.pipelineLaunchParamsVariableName = "params";

        if (scene_has_instances)
            s.pipeline_compile_options.traversableGraphFlags =
                OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_ANY;
        else if (scene_has_custom_shapes)
            s.pipeline_compile_options.traversableGraphFlags =
                OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;
        else
            s.pipeline_compile_options.traversableGraphFlags =
                OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS;

    #if !defined(MTS_OPTIX_DEBUG)
        s.pipeline_compile_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;
    #else
        s.pipeline_compile_options.exceptionFlags =
                OPTIX_EXCEPTION_FLAG_STACK_OVERFLOW
                | OPTIX_EXCEPTION_FLAG_TRACE_DEPTH
                | OPTIX_EXCEPTION_FLAG_DEBUG;
    #endif

        if (scene_has_custom_shapes)
            s.pipeline_compile_options.usesPrimitiveTypeFlags = OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM;
        else
            s.pipeline_compile_options.usesPrimitiveTypeFlags = OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE;

        // =====================================================
        // Logging infra for pipeline setup
        // =====================================================

        char optix_log[2048];
        size_t optix_log_size = sizeof(optix_log);
        auto check_log = [&](int rv) {
            if (rv) {
                fprintf(stderr, "\tLog: %s%s", optix_log,
                        optix_log_size > sizeof(optix_log) ? "<TRUNCATED>" : "");
                jitc_optix_check(rv);
            }
        };

        // =====================================================
        // Create Optix module from supplemental PTX code
        // =====================================================

        check_log(optixModuleCreateFromPTX(
            s.context,
            &module_compile_options,
            &s.pipeline_compile_options,
            (const char *)optix_rt_ptx,
            optix_rt_ptx_size,
            optix_log,
            &optix_log_size,
            &s.module
        ));

        // =====================================================
        // Create program groups (raygen provided by Enoki..)
        // =====================================================

        OptixProgramGroupOptions program_group_options = {};
        OptixProgramGroupDesc pgd[ProgramGroupCount] {};

        pgd[0].kind                         = OPTIX_PROGRAM_GROUP_KIND_MISS;
        pgd[0].miss.module                  = s.module;
        pgd[0].miss.entryFunctionName       = "__miss__ms";
        pgd[1].kind                         = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        pgd[1].hitgroup.moduleCH            = s.module;
        pgd[1].hitgroup.entryFunctionNameCH = "__closesthit__mesh";

        for (size_t i = 0; i < custom_optix_shapes_count; i++) {
            pgd[2+i].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;

            std::string name = string::to_lower(custom_optix_shapes[i]);
            s.custom_shapes_program_names[2*i]   = strdup(("__closesthit__"   + name).c_str());
            s.custom_shapes_program_names[2*i+1] = strdup(("__intersection__" + name).c_str());

            pgd[2+i].hitgroup.moduleCH            = s.module;
            pgd[2+i].hitgroup.entryFunctionNameCH = s.custom_shapes_program_names[2*i];
            pgd[2+i].hitgroup.moduleIS            = s.module;
            pgd[2+i].hitgroup.entryFunctionNameIS = s.custom_shapes_program_names[2*i+1];
        }

        optix_log_size = sizeof(optix_log);
        check_log(optixProgramGroupCreate(
            s.context,
            pgd,
            ProgramGroupCount,
            &program_group_options,
            optix_log,
            &optix_log_size,
            s.program_groups
        ));

        // =====================================================
        //  Shader Binding Table generation
        // =====================================================

        std::vector<HitGroupSbtRecord> hg_sbts;
        fill_hitgroup_records(m_shapes, hg_sbts, s.program_groups);
        for (auto& shapegroup: m_shapegroups)
            shapegroup->optix_fill_hitgroup_records(hg_sbts, s.program_groups);

        size_t shapes_count = hg_sbts.size();

        s.sbt.missRecordBase = jitc_malloc(AllocType::HostPinned, sizeof(MissSbtRecord));
        s.sbt.missRecordStrideInBytes = sizeof(MissSbtRecord);
        s.sbt.missRecordCount         = 1;

        s.sbt.hitgroupRecordBase = jitc_malloc(AllocType::HostPinned,
                                               shapes_count * sizeof(HitGroupSbtRecord));
        s.sbt.hitgroupRecordStrideInBytes = sizeof(HitGroupSbtRecord);
        s.sbt.hitgroupRecordCount         = (unsigned int) shapes_count;

        jitc_optix_check(optixSbtRecordPackHeader(s.program_groups[0], s.sbt.missRecordBase));
        jitc_memcpy_async(true, s.sbt.hitgroupRecordBase, hg_sbts.data(), shapes_count * sizeof(HitGroupSbtRecord));

        s.sbt.missRecordBase = jitc_malloc_migrate(s.sbt.missRecordBase,
                                                   AllocType::Device, 1);
        s.sbt.hitgroupRecordBase = jitc_malloc_migrate(s.sbt.hitgroupRecordBase,
                                                       AllocType::Device, 1);

        // =====================================================
        //  Acceleration data structure building
        // =====================================================

        accel_parameters_changed_gpu();

        // =====================================================
        // Let Enoki know about all of this
        // =====================================================

        jitc_optix_configure(
            &s.pipeline_compile_options,
            &s.sbt,
            s.program_groups, ProgramGroupCount
        );
    }
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_parameters_changed_gpu() {
    if constexpr (ek::is_cuda_array_v<Float>) {
        if (m_shapes.empty())
            return;

        using OptixState = OptixState<Float>;
        OptixState &s = *(OptixState *) m_accel;

        // Build geometry acceleration structures for all the shapes
        build_gas(s.context, m_shapes, s.accel);
        for (auto& shapegroup: m_shapegroups)
            shapegroup->optix_build_gas(s.context);

        // Gather information about the instance acceleration structures to be built
        std::vector<OptixInstance> ias;
        prepare_ias(s.context, m_shapes, 0, s.accel, 0u, ScalarTransform4f(), ias);

        // If there is only a single IAS, no need to build the "master" IAS
        if (ias.size() == 1) {
            s.ias_buffer = nullptr;
            s.ias_handle = ias[0].traversableHandle;
            return;
        }

        // Build a "master" IAS that contains all the IAS of the scene (meshes,
        // custom shapes, instances, ...)
        OptixAccelBuildOptions accel_options = {};
        accel_options.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
        accel_options.operation  = OPTIX_BUILD_OPERATION_BUILD;
        accel_options.motionOptions.numKeys = 0;

        size_t ias_data_size = ias.size() * sizeof(OptixInstance);
        void* d_ias = jitc_malloc(AllocType::HostPinned, ias_data_size);
        jitc_memcpy_async(true, d_ias, ias.data(), ias_data_size);

        OptixBuildInput build_input;
        build_input.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
        build_input.instanceArray.instances =
            (CUdeviceptr) jitc_malloc_migrate(d_ias, AllocType::Device, 1);
        build_input.instanceArray.numInstances = (unsigned int) ias.size();

        OptixAccelBufferSizes buffer_sizes;
        jitc_optix_check(optixAccelComputeMemoryUsage(
            s.context,
            &accel_options,
            &build_input,
            1,
            &buffer_sizes
        ));

        void* d_temp_buffer = jitc_malloc(AllocType::Device, buffer_sizes.tempSizeInBytes);
        s.ias_buffer = jitc_malloc(AllocType::Device, buffer_sizes.outputSizeInBytes);

        jitc_optix_check(optixAccelBuild(
            s.context,
            (CUstream) jitc_cuda_stream(),
            &accel_options,
            &build_input,
            1, // num build inputs
            (CUdeviceptr)d_temp_buffer,
            buffer_sizes.tempSizeInBytes,
            (CUdeviceptr)s.ias_buffer,
            buffer_sizes.outputSizeInBytes,
            &s.ias_handle,
            0, // emitted property list
            0  // num emitted properties
        ));

        jitc_free(d_temp_buffer);
        jitc_free(d_ias);
    }
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_release_gpu() {
    if constexpr (ek::is_cuda_array_v<Float>) {
        using OptixState = OptixState<Float>;
        OptixState &s = *(OptixState *) m_accel;
        jitc_free((void*)s.sbt.raygenRecord);
        jitc_free(s.ias_buffer);

        for (size_t i = 0; i < ProgramGroupCount; i++)
            jitc_optix_check(optixProgramGroupDestroy(s.program_groups[i]));
        for (size_t i = 0; i < 2 * custom_optix_shapes_count; i++)
            free(s.custom_shapes_program_names[i]);
        jitc_optix_check(optixModuleDestroy(s.module));

        delete (OptixState *) m_accel;
        m_accel = nullptr;
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_gpu(const Ray3f &ray_, Mask active) const {
    if constexpr (ek::is_cuda_array_v<Float>) {
        Assert(!m_shapes.empty());

        using UInt32C = ek::detached_t<UInt32>;
        using UInt64C = ek::detached_t<UInt64>;
        using FloatC  = ek::detached_t<Float>;

        OptixState<Float> &s = *(OptixState<Float> *) m_accel;

        auto ray = ek::detach(ray_);

        UInt64C handle = ek::full<UInt64C>(s.ias_handle, 1, /* eval = */ true);
        UInt32C ray_mask(255), ray_flags(OPTIX_RAY_FLAG_NONE),
                sbt_offset(0), sbt_stride(1), miss_sbt_index(0);

        UInt32C payload_t(0),
                payload_prim_u(0),
                payload_prim_v(0),
                payload_prim_index(0),
                payload_shape_ptr(0);


    // Instance index is initialized to 0 when there is no instancing in the scene
        UInt32C payload_inst_index(m_shapegroups.empty() ? 0u : 1u);

        uint32_t trace_args[] {
            handle.index(),
            ray.o.x().index(), ray.o.y().index(), ray.o.z().index(),
            ray.d.x().index(), ray.d.y().index(), ray.d.z().index(),
            ray.mint.index(), ray.maxt.index(), ray.time.index(),
            ray_mask.index(), ray_flags.index(),
            sbt_offset.index(), sbt_stride.index(),
            miss_sbt_index.index(),
            payload_t.index(),
            payload_prim_u.index(),
            payload_prim_v.index(),
            payload_prim_index.index(),
            payload_shape_ptr.index(),
            payload_inst_index.index(),
        };

        jitc_optix_trace(sizeof(trace_args) / sizeof(uint32_t), trace_args);

        PreliminaryIntersection3f pi;
        pi.t          = ek::reinterpret_array<FloatC, UInt32C>(UInt32C::steal(trace_args[15]));
        pi.prim_uv[0] = ek::reinterpret_array<FloatC, UInt32C>(UInt32C::steal(trace_args[16]));
        pi.prim_uv[1] = ek::reinterpret_array<FloatC, UInt32C>(UInt32C::steal(trace_args[17]));
        pi.prim_index = UInt32C::steal(trace_args[18]);
        pi.shape      = ShapePtr::steal(trace_args[19]);
        pi.instance   = ShapePtr::steal(trace_args[20]);

        // This field is only used by embree, but we still need to initialized it for vcalls
        pi.shape_index = ek::empty<UInt32>(ek::width(ray));

        return pi;
    } else {
        ENOKI_MARK_USED(ray_);
        ENOKI_MARK_USED(active);
        Throw("ray_intersect_gpu() should only be called in GPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_gpu(const Ray3f &ray, uint32_t hit_flags, Mask active) const {
    if constexpr (ek::is_cuda_array_v<Float>) {
        PreliminaryIntersection3f pi = ray_intersect_preliminary_gpu(ray, active);
        return pi.compute_surface_interaction(ray, hit_flags, active);
    } else {
        ENOKI_MARK_USED(ray);
        ENOKI_MARK_USED(hit_flags);
        ENOKI_MARK_USED(active);
        Throw("ray_intersect_gpu() should only be called in GPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_gpu(const Ray3f &ray_, Mask active) const {
    if constexpr (ek::is_cuda_array_v<Float>) {
        Assert(!m_shapes.empty());

        using UInt32C = ek::detached_t<UInt32>;
        using UInt64C = ek::detached_t<UInt64>;

        OptixState<Float> &s = *(OptixState<Float> *) m_accel;

        auto ray = ek::detach(ray_);

        UInt64C handle = ek::full<UInt64C>(s.ias_handle, 1, /* eval = */ true);
        UInt32C ray_mask(255), ray_flags(OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT),
                sbt_offset(0), sbt_stride(1), miss_sbt_index(0);

        UInt32C payload_hit(0);

        uint32_t trace_args[] {
            handle.index(),
            ray.o.x().index(), ray.o.y().index(), ray.o.z().index(),
            ray.d.x().index(), ray.d.y().index(), ray.d.z().index(),
            ray.mint.index(), ray.maxt.index(), ray.time.index(),
            ray_mask.index(), ray_flags.index(),
            sbt_offset.index(), sbt_stride.index(),
            miss_sbt_index.index(), payload_hit.index()
        };

        jitc_optix_trace(sizeof(trace_args) / sizeof(uint32_t), trace_args);

        return ek::eq(UInt32C::steal(trace_args[15]), 1);
    } else {
        ENOKI_MARK_USED(ray_);
        ENOKI_MARK_USED(active);
        Throw("ray_test_gpu() should only be called in GPU mode.");
    }
}

NAMESPACE_END(mitsuba)
