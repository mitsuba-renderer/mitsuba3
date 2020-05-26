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
    OptixTraversableHandle accel;
    void* accel_buffer_meshes;
    void* accel_buffer_others;
    void* accel_buffer_ias;
    void* params;
    char *custom_optix_shapes_program_names[2 * custom_optix_shapes_count];
};

template <typename T>
struct alignas(OPTIX_SBT_RECORD_ALIGNMENT) SbtRecord {
    char header[OPTIX_SBT_RECORD_HEADER_SIZE];
    T data;
};

struct alignas(OPTIX_SBT_RECORD_ALIGNMENT) EmptySbtRecord {
    char header[OPTIX_SBT_RECORD_HEADER_SIZE];
};

using RayGenSbtRecord   = EmptySbtRecord;
using MissSbtRecord     = EmptySbtRecord;
using HitGroupSbtRecord = SbtRecord<OptixHitGroupData>;

MTS_VARIANT void Scene<Float, Spectrum>::accel_init_gpu(const Properties &/*props*/) {
    Log(Info, "Building scene in OptiX ..");
    m_accel = new OptixState();
    OptixState &s = *(OptixState *) m_accel;

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

    size_t shapes_count = m_shapes.size();
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

    uint32_t shape_index = 0;
    std::vector<HitGroupSbtRecord> hg_sbts(shapes_count);

    // First process all the meshes, then the other shapes
    for (size_t i = 0; i < 2; i++) {
        for (Shape* shape: m_shapes) {
            // true for meshes at 1st outer iteration
            if (i == !shape->is_mesh()) {
                size_t program_group_idx = (shape->is_mesh() ? 2 : 3 + get_shape_descr_idx(shape));
                // Setup the hitgroup record and copy it to the hitgroup records array
                rt_check(optixSbtRecordPackHeader(s.program_groups[program_group_idx], &hg_sbts[shape_index]));
                // Prepare shape optix data and AABB
                shape->optix_prepare_geometry();
                // Set hitgroup record data
                hg_sbts[shape_index].data.shape_ptr = (uintptr_t) shape;
                hg_sbts[shape_index].data.data = shape->optix_hitgroup_data();
                ++shape_index;
            }
        }
    }

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

    accel_parameters_changed_gpu();

    // Allocate params pointer
    s.params = cuda_malloc(sizeof(OptixParams));

    // This will trigger the scatter calls to upload geometry to the device
    cuda_eval();

    // TODO: check if we still want to do run a dummy launch
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_parameters_changed_gpu() {
    OptixState &s = *(OptixState *) m_accel;

    if (m_shapes.empty())
        return;

    // ----------------------------------------
    //  Build GAS for meshes and custom shapes
    // ----------------------------------------

    std::vector<Shape*> shape_meshes, shape_others;
    for (Shape* shape: m_shapes) {
        if (shape->is_mesh())
            shape_meshes.push_back(shape);
        else
            shape_others.push_back(shape);
    }

    OptixAccelBuildOptions accel_options = {};
    accel_options.buildFlags = OPTIX_BUILD_FLAG_ALLOW_COMPACTION;
    accel_options.operation  = OPTIX_BUILD_OPERATION_BUILD;
    accel_options.motionOptions.numKeys = 0;

    // Lambda function to build a GAS given a subset of shape pointers
    auto build_gas = [&s, &accel_options](const std::vector<Shape*> &shape_subset, void* &output_buffer) {
        if (output_buffer)
            cuda_free((void*)output_buffer);

        size_t shapes_count = shape_subset.size();

        if (shapes_count == 0)
            return OptixTraversableHandle(0);

        std::vector<OptixBuildInput> build_inputs(shapes_count);
        for (size_t i = 0; i < shapes_count; i++)
            shape_subset[i]->optix_build_input(build_inputs[i]);

        OptixAccelBufferSizes buffer_sizes;
        rt_check(optixAccelComputeMemoryUsage(
            s.context,
            &accel_options,
            build_inputs.data(),
            (unsigned int) shapes_count,
            &buffer_sizes
        ));

        void* d_temp_buffer = cuda_malloc(buffer_sizes.tempSizeInBytes);
        output_buffer = cuda_malloc(buffer_sizes.outputSizeInBytes + 8);

        OptixAccelEmitDesc emit_property = {};
        emit_property.type   = OPTIX_PROPERTY_TYPE_COMPACTED_SIZE;
        emit_property.result = (CUdeviceptr)((char*)output_buffer + buffer_sizes.outputSizeInBytes);

        OptixTraversableHandle accel;
        rt_check(optixAccelBuild(
            s.context,
            0,              // CUDA stream
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

        cuda_free((void*)d_temp_buffer);

        size_t compact_size;
        cuda_memcpy_from_device(&compact_size, (void*)emit_property.result, sizeof(size_t));
        if (compact_size < buffer_sizes.outputSizeInBytes) {
            void* compact_buffer = cuda_malloc(compact_size);
            // Use handle as input and output
            rt_check(optixAccelCompact(
                s.context,
                0, // CUDA stream
                accel,
                (CUdeviceptr)compact_buffer,
                compact_size,
                &accel
            ));
            cuda_free((void*)output_buffer);
            output_buffer = compact_buffer;
        }

        return accel;
    };

    OptixTraversableHandle meshes_accel = build_gas(shape_meshes, s.accel_buffer_meshes);
    OptixTraversableHandle others_accel = build_gas(shape_others, s.accel_buffer_others);

    // ----------------------------------------------------------
    //  Build IAS to support mixture of meshes and custom shapes
    // ----------------------------------------------------------

    if (!shape_others.empty() && shape_meshes.empty()) {
        s.accel = others_accel;
    } else if (shape_others.empty() && !shape_meshes.empty()) {
        s.accel = meshes_accel;
    } else {
        // Create two empty instance with the identity transform
        OptixInstance instances[2] = { {
            { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 }, // transform
            0,                                      // instanceId
            0,                                      // sbtOffset
            255,                                    // visibilityMask
            OPTIX_INSTANCE_FLAG_DISABLE_TRANSFORM,  // flags
            meshes_accel,                           // handle
            { 0, 0 }                                // pad
        } };
        instances[1] = instances[0]; // duplicate the struct
        instances[1].instanceId = 1;
        instances[1].sbtOffset = (unsigned int) shape_meshes.size();
        instances[1].traversableHandle = others_accel;

        void* d_instances = cuda_malloc(2 * sizeof(OptixInstance));
        cuda_memcpy_to_device(d_instances, &instances, 2 * sizeof(OptixInstance));

        OptixBuildInput build_input;
        build_input.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
        build_input.instanceArray.instances = (CUdeviceptr) d_instances;
        build_input.instanceArray.numInstances = 2;
        build_input.instanceArray.aabbs = 0;
        build_input.instanceArray.numAabbs = 0;

        OptixAccelBufferSizes buffer_sizes;
        rt_check(optixAccelComputeMemoryUsage(s.context, &accel_options, &build_input, 1, &buffer_sizes));

        void* d_temp_buffer = cuda_malloc(buffer_sizes.tempSizeInBytes);
        s.accel_buffer_ias  = cuda_malloc(buffer_sizes.outputSizeInBytes);

        rt_check(optixAccelBuild(
            s.context,
            0,              // CUDA stream
            &accel_options,
            &build_input,
            1,              // num build inputs
            (CUdeviceptr)d_temp_buffer,
            buffer_sizes.tempSizeInBytes,
            (CUdeviceptr)s.accel_buffer_ias,
            buffer_sizes.outputSizeInBytes,
            &s.accel,
            0,  // emitted property list
            0   // num emitted properties
        ));

        cuda_free((void*)d_temp_buffer);
    }
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_release_gpu() {
    OptixState &s = *(OptixState *) m_accel;
    cuda_free((void*)s.sbt.raygenRecord);
    cuda_free((void*)s.accel_buffer_meshes);
    cuda_free((void*)s.accel_buffer_others);
    cuda_free((void*)s.accel_buffer_ias);
    cuda_free((void*)s.params);
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

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_gpu(const Ray3f &ray_, Mask active) const {
    if constexpr (is_cuda_array_v<Float>) {
        Assert(!m_shapes.empty());
        OptixState &s = *(OptixState *) m_accel;
        Ray3f ray(ray_);
        size_t ray_count = std::max(slices(ray.o), slices(ray.d));
        set_slices(ray, ray_count);
        set_slices(active, ray_count);

        SurfaceInteraction3f si = empty<SurfaceInteraction3f>(ray_count);

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

        cuda_eval();

        const OptixParams params = {
            // Active mask
            active.data(),
            // In: ray origin
            ray.o.x().data(), ray.o.y().data(), ray.o.z().data(),
            // In: ray direction
            ray.d.x().data(), ray.d.y().data(), ray.d.z().data(),
            // In: ray extents
            ray.mint.data(), ray.maxt.data(),
            // Out: Distance along ray
            si.t.data(),
            // Out: UV coordinates
            si.uv.x().data(), si.uv.y().data(),
            // Out: Geometric normal
            si.n.x().data(), si.n.y().data(), si.n.z().data(),
            // Out: Shading normal
            si.sh_frame.n.x().data(), si.sh_frame.n.y().data(), si.sh_frame.n.z().data(),
            // Out: Intersection position
            si.p.x().data(), si.p.y().data(), si.p.z().data(),
            // Out: Texture space derivative (U)
            si.dp_du.x().data(), si.dp_du.y().data(), si.dp_du.z().data(),
            // Out: Texture space derivative (V)
            si.dp_dv.x().data(), si.dp_dv.y().data(), si.dp_dv.z().data(),
            // Out: Shape pointer (on host)
            (unsigned long long*)si.shape.data(),
            // Out: Primitive index
            si.prim_index.data(),
            // Out: Hit flag
            nullptr,
            // top_object
            s.accel
        };

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
                (unsigned int) height,
                1u // depth
            );
        }
        rt_check(rt);

        si.time = ray.time;
        si.wavelengths = ray.wavelengths;
        si.instance = nullptr;
        si.duv_dx = si.duv_dy = 0.f;

        // Gram-schmidt orthogonalization to compute local shading frame
        si.sh_frame.s = normalize(
            fnmadd(si.sh_frame.n, dot(si.sh_frame.n, si.dp_du), si.dp_du));
        si.sh_frame.t = cross(si.sh_frame.n, si.sh_frame.s);

        // Incident direction in local coordinates
        si.wi = select(si.is_valid(), si.to_local(-ray.d), -ray.d);

        return si;
    } else {
        ENOKI_MARK_USED(ray_);
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

        const OptixParams params = {
            // Active mask
            active.data(),
            // In: ray origin
            ray.o.x().data(), ray.o.y().data(), ray.o.z().data(),
            // In: ray direction
            ray.d.x().data(), ray.d.y().data(), ray.d.z().data(),
            // In: ray extents
            ray.mint.data(), ray.maxt.data(),
            // Out: Distance along ray
            nullptr,
            // Out: UV coordinates
            nullptr, nullptr,
            // Out: Geometric normal
            nullptr, nullptr, nullptr,
            // Out: Shading normal
            nullptr, nullptr, nullptr,
            // Out: Intersection position
            nullptr, nullptr, nullptr,
            // Out: Texture space derivative (U)
            nullptr, nullptr, nullptr,
            // Out: Texture space derivative (V)
            nullptr, nullptr, nullptr,
            // Out: Shape pointer (on host)
            nullptr,
            // Out: Primitive index
            nullptr,
            // Out: Hit flag
            hit.data(),
            // top_object
            s.accel
        };

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

        return hit;
    } else {
        ENOKI_MARK_USED(ray_);
        ENOKI_MARK_USED(active);
        Throw("ray_test_gpu() should only be called in GPU mode.");
    }
}

NAMESPACE_END(msiuba)
