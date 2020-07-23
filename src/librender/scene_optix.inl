#include "librender_ptx.h"
#include <iomanip>

#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/optix/shapes.h>
#include <mitsuba/render/optix_api.h>

NAMESPACE_BEGIN(mitsuba)

#if !defined(NDEBUG)
# define MTS_OPTIX_DEBUG 1
#endif

#ifdef __WINDOWS__
# define strdup _strdup
#endif

static void context_log_cb(unsigned int level, const char* tag, const char* message, void* /*cbdata */) {
    std::cerr << "[" << std::setw(2) << level << "][" << std::setw(12) << tag
              << "]: " << message << "\n";
}

#if defined(MTS_OPTIX_DEBUG)
    static constexpr size_t ProgramGroupCount = 4 + custom_optix_shapes_count;
#else
    static constexpr size_t ProgramGroupCount = 3 + custom_optix_shapes_count;
#endif

struct OptixState {
    OptixDeviceContext context;
    OptixPipeline pipeline = nullptr;
    OptixModule module = nullptr;
    OptixProgramGroup program_groups[ProgramGroupCount];
    OptixShaderBindingTable sbt = {};
    OptixAccelData accel;
    OptixTraversableHandle ias_handle = 0ull;
    void* ias_buffer = nullptr;

    void* params;
    char *custom_optix_shapes_program_names[2 * custom_optix_shapes_count];

    enoki::CUDAArray<const void*> shapes_ptr;
};

MTS_VARIANT void Scene<Float, Spectrum>::accel_init_gpu(const Properties &/*props*/) {
    if constexpr (is_cuda_array_v<Float>) {
        Log(Info, "Building scene in OptiX ..");
        m_accel = new OptixState();
        OptixState &s = *(OptixState *) m_accel;

        // Copy shapes pointers to the GPU
        s.shapes_ptr = enoki::CUDAArray<const void*>::copy((void**)m_shapes.data(), m_shapes.size());

        // ------------------------
        //  OptiX context creation
        // ------------------------

        CUcontext cuCtx = 0;  // zero means take the current context
        OptixDeviceContextOptions options = {};
        options.logCallbackFunction       = &context_log_cb;
    #if !defined(MTS_OPTIX_DEBUG)
        options.logCallbackLevel          = 1;
    #else
        options.logCallbackLevel          = 3;
    #endif
        rt_check(optixDeviceContextCreate(cuCtx, &options, &s.context));

        // ----------------------------------------------
        //  Pipeline generation - Create Module from PTX
        // ----------------------------------------------

        OptixPipelineCompileOptions pipeline_compile_options = {};
        OptixModuleCompileOptions module_compile_options = {};

        module_compile_options.maxRegisterCount = OPTIX_COMPILE_DEFAULT_MAX_REGISTER_COUNT;
    #if !defined(MTS_OPTIX_DEBUG)
        module_compile_options.optLevel         = OPTIX_COMPILE_OPTIMIZATION_DEFAULT;
        module_compile_options.debugLevel       = OPTIX_COMPILE_DEBUG_LEVEL_NONE;
    #else
        module_compile_options.optLevel         = OPTIX_COMPILE_OPTIMIZATION_LEVEL_0;
        module_compile_options.debugLevel       = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
    #endif

        pipeline_compile_options.usesMotionBlur        = false;
        pipeline_compile_options.traversableGraphFlags = OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_ANY;
        pipeline_compile_options.numPayloadValues      = 3;
        pipeline_compile_options.numAttributeValues    = 3;
        pipeline_compile_options.pipelineLaunchParamsVariableName = "params";

    #if !defined(MTS_OPTIX_DEBUG)
        pipeline_compile_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;
    #else
        pipeline_compile_options.exceptionFlags =
                OPTIX_EXCEPTION_FLAG_STACK_OVERFLOW
                | OPTIX_EXCEPTION_FLAG_TRACE_DEPTH
                | OPTIX_EXCEPTION_FLAG_USER
                | OPTIX_EXCEPTION_FLAG_DEBUG;
    #endif

        rt_check_log(optixModuleCreateFromPTX(
            s.context,
            &module_compile_options,
            &pipeline_compile_options,
            (const char *)optix_rt_ptx,
            optix_rt_ptx_size,
            optix_log_buffer,
            &optix_log_buffer_size,
            &s.module
        ));

        // ---------------------------------------------
        //  Pipeline generation - Create program groups
        // ---------------------------------------------

        OptixProgramGroupOptions program_group_options = {};

        OptixProgramGroupDesc prog_group_descs[ProgramGroupCount];
        memset(prog_group_descs, 0, sizeof(prog_group_descs));

        prog_group_descs[0].kind                     = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
        prog_group_descs[0].raygen.module            = s.module;
        prog_group_descs[0].raygen.entryFunctionName = "__raygen__rg";

        prog_group_descs[1].kind                   = OPTIX_PROGRAM_GROUP_KIND_MISS;
        prog_group_descs[1].miss.module            = s.module;
        prog_group_descs[1].miss.entryFunctionName = "__miss__ms";

        prog_group_descs[2].kind                         = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        prog_group_descs[2].hitgroup.moduleCH            = s.module;
        prog_group_descs[2].hitgroup.entryFunctionNameCH = "__closesthit__mesh";

        for (size_t i = 0; i < custom_optix_shapes_count; i++) {
            prog_group_descs[3+i].kind                         = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;

            std::string name = string::to_lower(custom_optix_shapes[i]);
            s.custom_optix_shapes_program_names[2*i] = strdup(("__closesthit__" + name).c_str());
            s.custom_optix_shapes_program_names[2*i+1] = strdup(("__intersection__" + name).c_str());

            prog_group_descs[3+i].hitgroup.moduleCH            = s.module;
            prog_group_descs[3+i].hitgroup.entryFunctionNameCH = s.custom_optix_shapes_program_names[2*i];
            prog_group_descs[3+i].hitgroup.moduleIS            = s.module;
            prog_group_descs[3+i].hitgroup.entryFunctionNameIS = s.custom_optix_shapes_program_names[2*i+1];
        }

    #if defined(MTS_OPTIX_DEBUG)
        OptixProgramGroupDesc &exception_prog_group_desc = prog_group_descs[ProgramGroupCount-1];
        exception_prog_group_desc.kind                         = OPTIX_PROGRAM_GROUP_KIND_EXCEPTION;
        exception_prog_group_desc.hitgroup.moduleCH            = s.module;
        exception_prog_group_desc.hitgroup.entryFunctionNameCH = "__exception__err";
    #endif

        rt_check_log(optixProgramGroupCreate(
            s.context,
            prog_group_descs,
            ProgramGroupCount,
            &program_group_options,
            optix_log_buffer,
            &optix_log_buffer_size,
            s.program_groups
        ));

        // ---------------------------------------
        //  Pipeline generation - Create pipeline
        // ---------------------------------------

        OptixPipelineLinkOptions pipeline_link_options = {};
        pipeline_link_options.maxTraceDepth          = 1;
    #if defined(MTS_OPTIX_DEBUG)
        pipeline_link_options.debugLevel             = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
    #else
        pipeline_link_options.debugLevel             = OPTIX_COMPILE_DEBUG_LEVEL_NONE;
    #endif
        pipeline_link_options.overrideUsesMotionBlur = false;
        rt_check_log(optixPipelineCreate(
            s.context,
            &pipeline_compile_options,
            &pipeline_link_options,
            s.program_groups,
            ProgramGroupCount,
            optix_log_buffer,
            &optix_log_buffer_size,
            &s.pipeline
        ));

        // ---------------------------------
        //  Shader Binding Table generation
        // ---------------------------------

        std::vector<HitGroupSbtRecord> hg_sbts;
        fill_hitgroup_records(m_shapes, hg_sbts, s.program_groups);
        for (auto& shapegroup: m_shapegroups)
            shapegroup->optix_fill_hitgroup_records(hg_sbts, s.program_groups);

        size_t shapes_count = hg_sbts.size();
        void* records = cuda_malloc(sizeof(RayGenSbtRecord) + sizeof(MissSbtRecord) + sizeof(HitGroupSbtRecord) * shapes_count);

        RayGenSbtRecord raygen_sbt;
        rt_check(optixSbtRecordPackHeader(s.program_groups[0], &raygen_sbt));
        void* raygen_record = records;
        cuda_memcpy_to_device(raygen_record, &raygen_sbt, sizeof(RayGenSbtRecord));

        MissSbtRecord miss_sbt;
        rt_check(optixSbtRecordPackHeader(s.program_groups[1], &miss_sbt));
        void* miss_record = (char*)records + sizeof(RayGenSbtRecord);
        cuda_memcpy_to_device(miss_record, &miss_sbt, sizeof(MissSbtRecord));

        // Allocate hitgroup records array
        void* hitgroup_records = (char*)records + sizeof(RayGenSbtRecord) + sizeof(MissSbtRecord);

        // Copy HitGroupRecords to the GPU
        cuda_memcpy_to_device(hitgroup_records, hg_sbts.data(), shapes_count * sizeof(HitGroupSbtRecord));

        s.sbt.raygenRecord                = (CUdeviceptr)raygen_record;
        s.sbt.missRecordBase              = (CUdeviceptr)miss_record;
        s.sbt.missRecordStrideInBytes     = sizeof(MissSbtRecord);
        s.sbt.missRecordCount             = 1;
        s.sbt.hitgroupRecordBase          = (CUdeviceptr)hitgroup_records;
        s.sbt.hitgroupRecordStrideInBytes = sizeof(HitGroupSbtRecord);
        s.sbt.hitgroupRecordCount         = (unsigned int) shapes_count;

        // --------------------------------------
        //  Acceleration data structure building
        // --------------------------------------

        // Build and bind GAS and IAS for all the shapes and instances in the scene
        accel_parameters_changed_gpu();

        // Allocate launch-varying OptixParams data structure
        s.params = cuda_malloc(sizeof(OptixParams));

        // This will trigger the scatter calls to upload geometry to the device
        cuda_eval();
    }
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_parameters_changed_gpu() {
    if constexpr (is_cuda_array_v<Float>) {
        if (m_shapes.empty())
            return;

        OptixState &s = *(OptixState *) m_accel;

        // Build geometry acceleration structures for all the shapes
        build_gas(s.context, m_shapes, s.accel);
        for (auto& shapegroup: m_shapegroups)
            shapegroup->optix_build_gas(s.context);

        // Gather information about the instance acceleration structures to be built
        std::vector<OptixInstance> ias;
        prepare_ias(s.context, m_shapes, 0, s.accel, (uint32_t) m_shapes.size(),
                    ScalarTransform4f(), ias);

        // If there is only a single IAS, no need to build the "master" IAS
        if (ias.size() == 1) {
            s.ias_buffer = nullptr;
            s.ias_handle = ias[0].traversableHandle;
            return;
        }

        // Build a "master" IAS that contains all the IAS of the scene (meshes,
        // custom shapes, instances, ...)
        OptixAccelBuildOptions accel_options = {};
        accel_options.buildFlags = OPTIX_BUILD_FLAG_ALLOW_COMPACTION | OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
        accel_options.operation  = OPTIX_BUILD_OPERATION_BUILD;
        accel_options.motionOptions.numKeys = 0;

        void* d_ias = cuda_malloc(ias.size() * sizeof(OptixInstance));
        cuda_memcpy_to_device(d_ias, ias.data(), ias.size() * sizeof(OptixInstance));

        OptixBuildInput build_input;
        build_input.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
        build_input.instanceArray.instances = (CUdeviceptr) d_ias;
        build_input.instanceArray.numInstances = (unsigned int) ias.size();
        build_input.instanceArray.aabbs = 0;
        build_input.instanceArray.numAabbs = 0;

        OptixAccelBufferSizes buffer_sizes;
        rt_check(optixAccelComputeMemoryUsage(s.context, &accel_options, &build_input, 1, &buffer_sizes));
        void* d_temp_buffer = cuda_malloc(buffer_sizes.tempSizeInBytes);
        s.ias_buffer    = cuda_malloc(buffer_sizes.outputSizeInBytes);

        rt_check(optixAccelBuild(
            s.context,
            0, // CUDA stream
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

        cuda_free(d_temp_buffer);
        cuda_free(d_ias);
    }
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_release_gpu() {
    if constexpr (is_cuda_array_v<Float>) {
        OptixState &s = *(OptixState *) m_accel;
        cuda_free((void*)s.sbt.raygenRecord);
        cuda_free((void*)s.params);
        cuda_free(s.ias_buffer);
        rt_check(optixPipelineDestroy(s.pipeline));
        for (size_t i = 0; i < ProgramGroupCount; i++)
            rt_check(optixProgramGroupDestroy(s.program_groups[i]));
        for (size_t i = 0; i < 2 * custom_optix_shapes_count; i++)
            free(s.custom_optix_shapes_program_names[i]);
        rt_check(optixModuleDestroy(s.module));
        rt_check(optixDeviceContextDestroy(s.context));

        delete (OptixState *) m_accel;
        m_accel = nullptr;
    }
}

/// Helper function to launch the OptiX kernel (try twice if unsuccessful)
void launch_optix_kernel(const OptixState &s,
                         const OptixParams &params,
                         size_t ray_count) {

    cuda_memcpy_to_device(s.params, &params, sizeof(OptixParams));

    unsigned int width = 1, height = (unsigned int) ray_count;
    while (!(height & 1) && width < height) {
        width <<= 1;
        height >>= 1;
    }

    OptixResult rt = optixLaunch(
        s.pipeline,
        0, // default cuda stream
        (CUdeviceptr)s.params,
        sizeof(OptixParams),
        &s.sbt,
        width,
        height,
        1u // depth
    );
    if (rt == OPTIX_ERROR_HOST_OUT_OF_MEMORY) {
        cuda_malloc_trim();
        rt = optixLaunch(
            s.pipeline,
            0, // default cuda stream
            (CUdeviceptr)s.params,
            sizeof(OptixParams),
            &s.sbt,
            width,
            height,
            1u // depth
        );
    }

    rt_check(rt);
}

/// Helper function to bind CUDAArray data pointer to fields in the OptixParams struct
template <typename T> void bind_data(scalar_t<T> **field, T &value) {
    if constexpr (is_static_array_v<T>) {
        for (size_t i = 0; i < array_size_v<T>; ++i)
            field[i] = value[i].data();
    } else {
        *field = value.data();
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_gpu(const Ray3f &ray_, Mask active) const {
    if constexpr (is_cuda_array_v<Float>) {
        Assert(!m_shapes.empty());
        OptixState &s = *(OptixState *) m_accel;

        Ray3f ray(ray_);
        size_t ray_count = std::max(slices(ray.o), slices(ray.d));
        set_slices(ray, ray_count);
        set_slices(active, ray_count);

        PreliminaryIntersection3f pi = empty<PreliminaryIntersection3f>(ray_count);

        // Initialize instance index with the highest possible index an instance
        // could have in m_shapes. As the hierarchy of IAS is built, this value
        // is used to tag IAS that are not related to instancing (e.g. custom
        // shape tree).
        uint32_t max_inst_index = m_shapegroups.empty() ? 0u : (unsigned int) m_shapes.size();
        UInt32 instance_index = full<UInt32>(max_inst_index, ray_count);

        // Ensure pi and instance_index are allocated before binding the data pointers
        cuda_eval();

        // Initialize OptixParams with all members initialized to 0 (e.g. nullptr)
        OptixParams params = {};

        // Bind GPU data pointers to be filled by the OptiX kernel
        bind_data(&params.in_mask, active);
        bind_data(params.in_o, ray.o);
        bind_data(params.in_d, ray.d);
        bind_data(&params.in_mint, ray.mint);
        bind_data(&params.in_maxt, ray.maxt);
        bind_data(&params.out_t, pi.t);
        bind_data(params.out_prim_uv, pi.prim_uv);
        bind_data(&params.out_prim_index, pi.prim_index);
        bind_data(&params.out_inst_index, instance_index);
        params.out_shape_ptr = (unsigned long long*)pi.shape.data();
        params.handle = s.ias_handle;

        launch_optix_kernel(s, params, ray_count);

        // Only gather instance pointers for valid instance indices
        Mask valid_instances = instance_index < max_inst_index;
        pi.instance =
            gather<ShapePtr>(reinterpret_array<ShapePtr>(s.shapes_ptr),
                             instance_index, active & valid_instances);

        return pi;
    } else {
        ENOKI_MARK_USED(ray_);
        ENOKI_MARK_USED(active);
        Throw("ray_intersect_gpu() should only be called in GPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_gpu(const Ray3f &ray_, HitComputeFlags flags, Mask active) const {
    if constexpr (is_cuda_array_v<Float>) {
        Assert(!m_shapes.empty());
        OptixState &s = *(OptixState *) m_accel;

        if constexpr (is_diff_array_v<Float>) {
            // Differentiable SurfaceInteraction needs to be computed outside of the OptiX kernel
            if (!has_flag(flags, HitComputeFlags::NonDifferentiable) &&
                (requires_gradient(ray_.o) || shapes_grad_enabled())) {
                auto pi = ray_intersect_preliminary_gpu(ray_, active);
                return pi.compute_surface_interaction(ray_, flags, active);
            }
        }

        Ray3f ray(ray_);
        size_t ray_count = std::max(slices(ray.o), slices(ray.d));
        set_slices(ray, ray_count);
        set_slices(active, ray_count);

        // Allocate only the required fields of the SurfaceInteraction struct
        SurfaceInteraction3f si = empty<SurfaceInteraction3f>(1); // needed for virtual calls

        si.t          = empty<Float>(ray_count);
        si.p          = empty<Point3f>(ray_count);
        si.n          = empty<Normal3f>(ray_count);
        si.prim_index = empty<UInt32>(ray_count);
        si.shape      = empty<ShapePtr>(ray_count);

        if (has_flag(flags, HitComputeFlags::ShadingFrame))
            si.sh_frame.n = empty<Normal3f>(ray_count);

        if (has_flag(flags, HitComputeFlags::UV))
            si.uv = empty<Point2f>(ray_count);

        if (has_flag(flags, HitComputeFlags::dPdUV)) {
            si.dp_du = empty<Vector3f>(ray_count);
            si.dp_dv = empty<Vector3f>(ray_count);
        }

        if (has_flag(flags, HitComputeFlags::dNGdUV) || has_flag(flags, HitComputeFlags::dNSdUV)) {
            si.dn_du = empty<Vector3f>(ray_count);
            si.dn_dv = empty<Vector3f>(ray_count);
        }

        // DEBUG mode: Explicitly instantiate `si` with NaN values.
        // As the integrator should only deal with the lanes of `si` for which
        // `si.is_valid()==true`, this makes it easier to catch bugs in the
        // masking logic implemented in the integrator.
#if !defined(NDEBUG)
        #define SET_NAN(name) name = full<decltype(name)>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
        SET_NAN(si.t); SET_NAN(si.time); SET_NAN(si.p); SET_NAN(si.uv); SET_NAN(si.n);
        SET_NAN(si.sh_frame.n); SET_NAN(si.dp_du); SET_NAN(si.dp_dv);
        #undef SET_NAN
#endif  // !defined(NDEBUG)

        // Initialize instance index with the highest possible index an instance
        // could have in m_shapes. As the hierarchy of IAS is built, this value
        // is used to tag IAS that are not related to instancing (e.g. custom
        // shape tree).
        uint32_t max_inst_index = m_shapegroups.empty() ? 0u : (unsigned int) m_shapes.size();
        UInt32 instance_index = full<UInt32>(max_inst_index, ray_count);

        // Ensure si and instance_index are allocated before binding the
        // data pointers
        cuda_eval();

        // Initialize OptixParams with all members initialized to 0 (e.g.
        // nullptr)
        OptixParams params = {};

        // Bind GPU data pointers to be filled by the OptiX kernel
        bind_data(&params.in_mask, active);
        bind_data(params.in_o, ray.o);
        bind_data(params.in_d, ray.d);
        bind_data(&params.in_mint, ray.mint);
        bind_data(&params.in_maxt, ray.maxt);
        bind_data(&params.out_t, si.t);
        if (has_flag(flags, HitComputeFlags::UV))
            bind_data(params.out_uv, si.uv);
        bind_data(params.out_ng, si.n);
        if (has_flag(flags, HitComputeFlags::ShadingFrame))
            bind_data(params.out_ns, si.sh_frame.n);
        bind_data(params.out_p, si.p);
        if (has_flag(flags, HitComputeFlags::dPdUV)) {
            bind_data(params.out_dp_du, si.dp_du);
            bind_data(params.out_dp_dv, si.dp_dv);
        }
        if (has_flag(flags, HitComputeFlags::dNGdUV)) {
            bind_data(params.out_dng_du, si.dn_du);
            bind_data(params.out_dng_dv, si.dn_dv);
        }
        if (has_flag(flags, HitComputeFlags::dNSdUV)) {
            bind_data(params.out_dns_du, si.dn_du);
            bind_data(params.out_dns_dv, si.dn_dv);
        }
        bind_data(&params.out_prim_index, si.prim_index);
        bind_data(&params.out_inst_index, instance_index);
        params.out_shape_ptr = (unsigned long long*)si.shape.data();
        params.handle = s.ias_handle;

        launch_optix_kernel(s, params, ray_count);

        si.time = ray.time;
        si.wavelengths = ray.wavelengths;
        si.duv_dx = si.duv_dy = zero<Point2f>();

        if (has_flag(flags, HitComputeFlags::ShadingFrame))
            si.initialize_sh_frame();

        // Only gather instance pointers for valid instance indices
        Mask valid_instances = instance_index < m_shapes.size();
        si.instance =
            gather<ShapePtr>(reinterpret_array<ShapePtr>(s.shapes_ptr),
                             instance_index, active & valid_instances);

        // Incident direction in local coordinates
        si.wi = select(si.is_valid(), si.to_local(-ray.d), -ray.d);

        return si;
    } else {
        ENOKI_MARK_USED(ray_);
        ENOKI_MARK_USED(flags);
        ENOKI_MARK_USED(active);
        Throw("ray_intersect_gpu() should only be called in GPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_gpu(const Ray3f &ray_, Mask active) const {
    if constexpr (is_cuda_array_v<Float>) {
        OptixState &s = *(OptixState *) m_accel;
        Ray3f ray(ray_);
        size_t ray_count = std::max(slices(ray.o), slices(ray.d));
        Mask hit = empty<Mask>(ray_count);

        set_slices(ray, ray_count);
        set_slices(active, ray_count);

        cuda_eval();

        // Initialize OptixParams with all members initialized to 0 (e.g. nullptr)
        OptixParams params = {};

        // Bind GPU data pointers to be filled by the OptiX kernel
        bind_data(&params.in_mask, active);
        bind_data(params.in_o, ray.o);
        bind_data(params.in_d, ray.d);
        bind_data(&params.in_mint, ray.mint);
        bind_data(&params.in_maxt, ray.maxt);
        bind_data(&params.out_hit, hit);
        params.handle = s.ias_handle;

        launch_optix_kernel(s, params, ray_count);

        return hit;
    } else {
        ENOKI_MARK_USED(ray_);
        ENOKI_MARK_USED(active);
        Throw("ray_test_gpu() should only be called in GPU mode.");
    }
}

NAMESPACE_END(msiuba)
