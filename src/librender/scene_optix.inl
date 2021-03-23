
#include <iomanip>
#include <cstring>
#include <cstdio>

#include <enoki-jit/optix.h>

#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/optix/shapes.h>
#include <mitsuba/render/optix_api.h>

#include "librender_ptx.h"

NAMESPACE_BEGIN(mitsuba)

#if !defined(NDEBUG)
#  define MTS_OPTIX_DEBUG 1
#endif

#ifdef _MSC_VER
#  define strdup(x) _strdup(x)
#endif

static constexpr size_t ProgramGroupCount = 2 + custom_optix_shapes_count;

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
        Timer timer;
        optix_initialize();

        m_accel = new OptixState();
        OptixState &s = *(OptixState *) m_accel;

        bool scene_has_meshes = false;
        bool scene_has_others = false;
        bool scene_has_instances = false;

        for (auto& shape : m_shapes) {
            scene_has_meshes    |= shape->is_mesh();
            scene_has_others    |= !shape->is_mesh() && !shape->is_instance();
            scene_has_instances |= shape->is_instance();
        }

        for (auto& shape : m_shapegroups) {
            scene_has_meshes |= !shape->has_meshes();
            scene_has_others |= !shape->has_others();
        }

        // =====================================================
        // OptiX context creation
        // =====================================================

        s.context = jit_optix_context();

        // =====================================================
        // Configure options for OptiX pipeline
        // =====================================================

        OptixModuleCompileOptions module_compile_options { };
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
        s.pipeline_compile_options.numAttributeValues    = 2; // the minimum legal value
        s.pipeline_compile_options.pipelineLaunchParamsVariableName = "params";

        if (scene_has_instances)
            s.pipeline_compile_options.traversableGraphFlags =
                OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_ANY;
        else if (scene_has_others && scene_has_meshes)
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

        unsigned int prim_flags = 0;
        if (scene_has_meshes)
            prim_flags |= OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE;
        if (scene_has_others)
            prim_flags |= OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM;

        s.pipeline_compile_options.usesPrimitiveTypeFlags = prim_flags;

        // =====================================================
        // Logging infrastructure for pipeline setup
        // =====================================================

        char optix_log[2048];
        size_t optix_log_size = sizeof(optix_log);
        auto check_log = [&](int rv) {
            if (rv) {
                fprintf(stderr, "\tLog: %s%s", optix_log,
                        optix_log_size > sizeof(optix_log) ? "<TRUNCATED>" : "");
                jit_optix_check(rv);
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

        s.sbt.missRecordBase = jit_malloc(AllocType::HostPinned, sizeof(MissSbtRecord));
        s.sbt.missRecordStrideInBytes = sizeof(MissSbtRecord);
        s.sbt.missRecordCount         = 1;

        s.sbt.hitgroupRecordBase = jit_malloc(AllocType::HostPinned,
                                               shapes_count * sizeof(HitGroupSbtRecord));
        s.sbt.hitgroupRecordStrideInBytes = sizeof(HitGroupSbtRecord);
        s.sbt.hitgroupRecordCount         = (unsigned int) shapes_count;

        jit_optix_check(optixSbtRecordPackHeader(s.program_groups[0], s.sbt.missRecordBase));
        jit_memcpy_async(JitBackend::CUDA,
                         s.sbt.hitgroupRecordBase,
                         hg_sbts.data(),
                         shapes_count * sizeof(HitGroupSbtRecord));

        s.sbt.missRecordBase = jit_malloc_migrate(s.sbt.missRecordBase,
                                                   AllocType::Device, 1);
        s.sbt.hitgroupRecordBase = jit_malloc_migrate(s.sbt.hitgroupRecordBase,
                                                       AllocType::Device, 1);

        // =====================================================
        //  Acceleration data structure building
        // =====================================================

        accel_parameters_changed_gpu();

        // =====================================================
        // Let Enoki know about all of this
        // =====================================================

        jit_optix_configure(
            &s.pipeline_compile_options,
            &s.sbt,
            s.program_groups, ProgramGroupCount
        );
        Log(Info, "OptiX ready. (took %s)", util::time_string((float) timer.value()));
    }
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_parameters_changed_gpu() {
    if constexpr (ek::is_cuda_array_v<Float>) {
        if (m_shapes.empty())
            return;

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
        void* d_ias = jit_malloc(AllocType::HostPinned, ias_data_size);
        jit_memcpy_async(JitBackend::CUDA, d_ias, ias.data(), ias_data_size);

        OptixBuildInput build_input;
        build_input.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
        build_input.instanceArray.instances =
            (CUdeviceptr) jit_malloc_migrate(d_ias, AllocType::Device, 1);
        build_input.instanceArray.numInstances = (unsigned int) ias.size();

        OptixAccelBufferSizes buffer_sizes;
        jit_optix_check(optixAccelComputeMemoryUsage(
            s.context,
            &accel_options,
            &build_input,
            1,
            &buffer_sizes
        ));

        void* d_temp_buffer
            = jit_malloc(AllocType::Device, buffer_sizes.tempSizeInBytes);
        s.ias_buffer
            = jit_malloc(AllocType::Device, buffer_sizes.outputSizeInBytes);

        jit_optix_check(optixAccelBuild(
            s.context,
            (CUstream) jit_cuda_stream(),
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

        jit_free(d_temp_buffer);
    }
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_release_gpu() {
    if constexpr (ek::is_cuda_array_v<Float>) {
        OptixState &s = *(OptixState *) m_accel;
        jit_free(s.sbt.raygenRecord);
        jit_free(s.sbt.hitgroupRecordBase);
        jit_free(s.sbt.missRecordBase);
        jit_free(s.ias_buffer);

        for (size_t i = 0; i < ProgramGroupCount; i++)
            jit_optix_check(optixProgramGroupDestroy(s.program_groups[i]));
        for (size_t i = 0; i < 2 * custom_optix_shapes_count; i++)
            free(s.custom_shapes_program_names[i]);
        jit_optix_check(optixModuleDestroy(s.module));

        delete (OptixState *) m_accel;
        m_accel = nullptr;
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_gpu(const Ray3f &ray, uint32_t,
                                                      Mask active) const {
    if constexpr (ek::is_cuda_array_v<Float>) {
        Assert(!m_shapes.empty());

        OptixState &s = *(OptixState *) m_accel;

        UInt64 handle = ek::opaque<UInt64>(s.ias_handle, 1);
        UInt32 ray_mask(255), ray_flags(OPTIX_RAY_FLAG_NONE),
               sbt_offset(0), sbt_stride(1), miss_sbt_index(0);

        UInt32 payload_t(0),
               payload_prim_u(0),
               payload_prim_v(0),
               payload_prim_index(0),
               payload_shape_ptr(0);

        // Instance index is initialized to 0 when there is no instancing in the scene
        UInt32 payload_inst_index(m_shapegroups.empty() ? 0u : 1u);

        using Single = ek::float32_array_t<Float>;
        ek::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_mint(ray.mint), ray_maxt(ray.maxt), ray_time(ray.time);
        if constexpr (std::is_same_v<double, ek::scalar_t<Float>>)
            ray_maxt[ek::eq(ray.maxt, ek::Largest<Float>)] = ek::Largest<Single>;

        uint32_t trace_args[] {
            handle.index(),
            ray_o.x().index(), ray_o.y().index(), ray_o.z().index(),
            ray_d.x().index(), ray_d.y().index(), ray_d.z().index(),
            ray_mint.index(), ray_maxt.index(), ray_time.index(),
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

        jit_optix_trace(sizeof(trace_args) / sizeof(uint32_t),
                        trace_args, active.index());

        PreliminaryIntersection3f pi;
        pi.t          = ek::reinterpret_array<Single, UInt32>(UInt32::steal(trace_args[15]));
        pi.prim_uv[0] = ek::reinterpret_array<Single, UInt32>(UInt32::steal(trace_args[16]));
        pi.prim_uv[1] = ek::reinterpret_array<Single, UInt32>(UInt32::steal(trace_args[17]));
        pi.prim_index = UInt32::steal(trace_args[18]);
        pi.shape      = ShapePtr::steal(trace_args[19]);
        pi.instance   = ShapePtr::steal(trace_args[20]);

        // This field is only used by embree, but we still need to initialize it for vcalls
        pi.shape_index = ek::zero<UInt32>();

        // jit_optix_trace leaves payload data uninitialized for inactive lanes
        pi.t[!active] = ek::Infinity<Float>;

        // Ensure pointers are initialized to nullptr for inactive lanes
        active &= pi.is_valid();
        pi.shape[!active]    = nullptr;
        pi.instance[!active] = nullptr;

        return pi;
    } else {
        ENOKI_MARK_USED(ray);
        ENOKI_MARK_USED(active);
        Throw("ray_intersect_gpu() should only be called in GPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_gpu(const Ray3f &ray, uint32_t hit_flags,
                                          Mask active) const {
    if constexpr (ek::is_cuda_array_v<Float>) {
        PreliminaryIntersection3f pi = ray_intersect_preliminary_gpu(ray, hit_flags, active);
        return pi.compute_surface_interaction(ray, hit_flags, active);
    } else {
        ENOKI_MARK_USED(ray);
        ENOKI_MARK_USED(hit_flags);
        ENOKI_MARK_USED(active);
        Throw("ray_intersect_gpu() should only be called in GPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_gpu(const Ray3f &ray, uint32_t, Mask active) const {
    if constexpr (ek::is_cuda_array_v<Float>) {
        Assert(!m_shapes.empty());

        OptixState &s = *(OptixState *) m_accel;

        UInt64 handle = ek::opaque<UInt64>(s.ias_handle, 1);
        UInt32 ray_mask(255),
               ray_flags(OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT |
                         OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT),
               sbt_offset(0), sbt_stride(1), miss_sbt_index(0);

        UInt32 payload_hit(1);

        using Single = ek::float32_array_t<Float>;
        ek::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_mint(ray.mint), ray_maxt(ray.maxt), ray_time(ray.time);
        if constexpr (std::is_same_v<double, ek::scalar_t<Float>>)
            ray_maxt[ek::eq(ray.maxt, ek::Largest<Float>)] = ek::Largest<Single>;

        uint32_t trace_args[] {
            handle.index(),
            ray_o.x().index(), ray_o.y().index(), ray_o.z().index(),
            ray_d.x().index(), ray_d.y().index(), ray_d.z().index(),
            ray_mint.index(), ray_maxt.index(), ray_time.index(),
            ray_mask.index(), ray_flags.index(),
            sbt_offset.index(), sbt_stride.index(),
            miss_sbt_index.index(), payload_hit.index()
        };

        jit_optix_trace(sizeof(trace_args) / sizeof(uint32_t),
                        trace_args, active.index());

        return active && ek::eq(UInt32::steal(trace_args[15]), 1);
    } else {
        ENOKI_MARK_USED(ray);
        ENOKI_MARK_USED(active);
        Throw("ray_test_gpu() should only be called in GPU mode.");
    }
}

NAMESPACE_END(mitsuba)
