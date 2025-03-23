#include <iomanip>
#include <cstring>
#include <cstdio>

#include <drjit-core/optix.h>

#include <nanothread/nanothread.h>

#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/optix/shapes.h>
#include <mitsuba/render/optix_api.h>

#include "librender_ptx.h"

NAMESPACE_BEGIN(mitsuba)

#if !defined(NDEBUG) || defined(MI_ENABLE_OPTIX_DEBUG_VALIDATION)
#define MI_ENABLE_OPTIX_DEBUG_VALIDATION_ON
#endif

#ifdef _MSC_VER
#  define strdup(x) _strdup(x)
#endif

// In this file:
// * In order to disamiguate Mitsuba data structures that are only used for
//   OptiX from data structures defined in the OptiX API, the former are
//   prefixed by "Mi", i.e "MiOptixDataStructure" vs "OptixDataStructure".

static constexpr size_t MAX_PROGRAM_GROUP_COUNT = 1 + MI_OPTIX_SHAPE_TYPE_COUNT;

// Per scene OptiX state data structure
struct MiOptixSceneState {
    OptixShaderBindingTable sbt = {};
    MiOptixAccelData accel;
    OptixTraversableHandle ias_handle = 0ull;
    struct InstanceData {
        void* buffer = nullptr;  // Device-visible storage for IAS
        void* inputs = nullptr;  // Device-visible storage for OptixInstance array
    } ias_data;
    size_t config_index;
    uint32_t sbt_jit_index;
};

/**
 * \brief Optix configuration data structure
 *
 * OptiX modules and program groups can be compiled with different set of
 * features and optimizations, which might vary depending on the scene's
 * requirements. This data structure hold those OptiX pipeline components for a
 * specific configuration, which can be shared across multiple scenes.
 *
 * \ref Scene::static_accel_shutdown is responsible for freeing those programs.
 */
struct MiOptixConfig {
    OptixDeviceContext context;
    OptixPipelineCompileOptions pipeline_compile_options;
    OptixModule main_module;
    OptixModule bspline_curve_module; /// Built-in module for B-spline curves
    OptixModule linear_curve_module; /// Built-in module for linear curves
    OptixProgramGroup program_groups[MAX_PROGRAM_GROUP_COUNT];
    char *intersection_pg_name[MI_OPTIX_SHAPE_TYPE_COUNT]; /// Intersection program names
    std::unordered_map<size_t, size_t> program_index_mapping;
    uint32_t pipeline_jit_index;
};

enum class MiOptixConfigShapes : uint32_t {
    HasCustom        = 1 << 0,
    HasMeshes        = 1 << 1,
    HasInstances     = 1 << 2,
    HasLinearCurves  = 1 << 3,
    HasBSplineCurves = 1 << 4,
    Count            = 1 << 5
};
MI_DECLARE_ENUM_OPERATORS(MiOptixConfigShapes)

// Array storing previously initialized optix configurations
static constexpr uint32_t MI_OPTIX_CONFIG_COUNT = (uint32_t) MiOptixConfigShapes::Count;
static MiOptixConfig optix_configs[MI_OPTIX_CONFIG_COUNT] = {};

size_t init_optix_config(uint32_t optix_config_shapes) {
    if (optix_config_shapes >= MI_OPTIX_CONFIG_COUNT)
        Throw("Optix configuration initialization failed! Unknown set of requested shapes.");

    // Use flags as config index in optix_configs
    size_t config_index = optix_config_shapes;
    MiOptixConfig &config = optix_configs[config_index];

    // Optix config already initialized
    if (config.main_module) {
        return config_index;
    }

    Log(Debug, "Initialize Optix configuration (index=%zu)..", config_index);

    config.context = jit_optix_context();

    // =====================================================
    // Setup OptiX pipeline
    // =====================================================

    OptixModuleCompileOptions module_compile_options { };
    module_compile_options.maxRegisterCount = OPTIX_COMPILE_DEFAULT_MAX_REGISTER_COUNT;
#if !defined(MI_ENABLE_OPTIX_DEBUG_VALIDATION_ON)
    module_compile_options.optLevel         = OPTIX_COMPILE_OPTIMIZATION_DEFAULT;
    module_compile_options.debugLevel       = OPTIX_COMPILE_DEBUG_LEVEL_NONE;
#else
    module_compile_options.optLevel         = OPTIX_COMPILE_OPTIMIZATION_LEVEL_0;
    module_compile_options.debugLevel       = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
#endif

    config.pipeline_compile_options.usesMotionBlur     = false;
    config.pipeline_compile_options.numPayloadValues   = 0;
    config.pipeline_compile_options.numAttributeValues = 2; // the minimum legal value
    config.pipeline_compile_options.pipelineLaunchParamsVariableName = "params";
    config.pipeline_compile_options.traversableGraphFlags =
        OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;

#if !defined(MI_ENABLE_OPTIX_DEBUG_VALIDATION_ON)
    config.pipeline_compile_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;
#else
    config.pipeline_compile_options.exceptionFlags =
            OPTIX_EXCEPTION_FLAG_DEBUG |
            OPTIX_EXCEPTION_FLAG_TRACE_DEPTH |
            OPTIX_EXCEPTION_FLAG_STACK_OVERFLOW;
#endif

    unsigned int prim_flags = 0;
    if (optix_config_shapes & MiOptixConfigShapes::HasMeshes)
        prim_flags |= OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE;
    if (optix_config_shapes & MiOptixConfigShapes::HasCustom)
        prim_flags |= OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM;
    if (optix_config_shapes & MiOptixConfigShapes::HasBSplineCurves)
        prim_flags |= OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_CUBIC_BSPLINE;
    if (optix_config_shapes & MiOptixConfigShapes::HasLinearCurves)
        prim_flags |= OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_LINEAR;

    config.pipeline_compile_options.usesPrimitiveTypeFlags = prim_flags;

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

    OptixTask task;
    check_log(optixModuleCreateWithTasks(
        config.context,
        &module_compile_options,
        &config.pipeline_compile_options,
        (const char *)optix_rt_ptx,
        optix_rt_ptx_size,
        optix_log,
        &optix_log_size,
        &config.main_module,
        &task
    ));

    std::function<void(OptixTask)> execute_task = [&](OptixTask task) {
        unsigned int max_new_tasks = std::max(pool_size(), 1u);

        std::unique_ptr<OptixTask[]> new_tasks =
            std::make_unique<OptixTask[]>(max_new_tasks);
        unsigned int new_task_count = 0;
        optixTaskExecute(task, new_tasks.get(), max_new_tasks,
                         &new_task_count);

        parallel_for(
            drjit::blocked_range<size_t>(0, new_task_count, 1),
            [&](const drjit::blocked_range<size_t> &range) {
                for (auto i = range.begin(); i != range.end(); ++i) {
                    OptixTask new_task = new_tasks[i];
                    execute_task(new_task);
                }
            }
        );
    };
    execute_task(task);

    int compilation_state = 0;
    check_log(
        optixModuleGetCompilationState(config.main_module, &compilation_state));
    if (compilation_state != OPTIX_MODULE_COMPILE_STATE_COMPLETED)
        Throw("Optix configuration initialization failed! The OptiX module "
              "compilation did not complete successfully. The module's "
              "compilation state is: %#06x", compilation_state);

    // =====================================================
    // Load built-in Optix modules for curves
    // =====================================================

    if (optix_config_shapes & MiOptixConfigShapes::HasBSplineCurves) {
        OptixBuiltinISOptions options = {};
        options.builtinISModuleType   = OPTIX_PRIMITIVE_TYPE_ROUND_CUBIC_BSPLINE;
        options.usesMotionBlur        = false;
        options.curveEndcapFlags      = 0;
        // buildFlags must match the flags used in OptixAccelBuildOptions (shapes.h)
        options.buildFlags            = OPTIX_BUILD_FLAG_ALLOW_COMPACTION |
                                        OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
        jit_optix_check(
            optixBuiltinISModuleGet(config.context, &module_compile_options,
                                    &config.pipeline_compile_options,
                                    &options, &config.bspline_curve_module));
    }
    if (optix_config_shapes & MiOptixConfigShapes::HasLinearCurves) {
        OptixBuiltinISOptions options = {};
        options.builtinISModuleType = OPTIX_PRIMITIVE_TYPE_ROUND_LINEAR;
        options.usesMotionBlur      = false;
        options.curveEndcapFlags    = 0;
        // buildFlags must match the flags used in OptixAccelBuildOptions (shapes.h)
        options.buildFlags          = OPTIX_BUILD_FLAG_ALLOW_COMPACTION |
                                      OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
        jit_optix_check(
            optixBuiltinISModuleGet(config.context, &module_compile_options,
                                    &config.pipeline_compile_options,
                                    &options, &config.linear_curve_module));
    }

    // =====================================================
    // Create program groups (raygen provided by Dr.Jit..)
    // =====================================================

    // Every shape type defines its own program group. Note that none of the
    // program groups will have a closest hit program.

    OptixProgramGroupOptions program_group_options = {};
    OptixProgramGroupDesc pgd[MAX_PROGRAM_GROUP_COUNT] {};

    size_t pg_count = 0;
    for (size_t i = 0; i < MI_OPTIX_SHAPE_TYPE_COUNT; i++) {
        bool need_pg = false;
        MiOptixShapeType shape_type = MI_OPTIX_SHAPE_ORDER[i];
        switch (shape_type) {
            case MiOptixShapeType::Mesh:
                if (optix_config_shapes & MiOptixConfigShapes::HasMeshes) {
                    pg_count++;
                    need_pg = true;
                }
                break;
            case MiOptixShapeType::BSplineCurve:
                if (optix_config_shapes & MiOptixConfigShapes::HasBSplineCurves) {
                    pg_count++;
                    need_pg = true;
                }
                break;
            case MiOptixShapeType::LinearCurve:
                if (optix_config_shapes & MiOptixConfigShapes::HasLinearCurves) {
                    pg_count++;
                    need_pg = true;
                }
                break;
            default: // Custom shapes
                if (optix_config_shapes & MiOptixConfigShapes::HasCustom) {
                    pg_count++;
                    need_pg = true;
                }
        }

        // Shape type is not part of the current config, skip it
        if (!need_pg)
            continue;

        MiOptixShape& optix_shape = MI_OPTIX_SHAPES.at(shape_type);

        char* is_name = nullptr;
        if (!optix_shape.is_builtin)
            is_name = strdup(optix_shape.is_name().c_str());

        config.intersection_pg_name[i] = is_name;
        config.program_index_mapping[i] = pg_count - 1;

        pgd[pg_count - 1].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        pgd[pg_count - 1].hitgroup.moduleCH = nullptr;
        pgd[pg_count - 1].hitgroup.entryFunctionNameCH = nullptr;
        pgd[pg_count - 1].hitgroup.entryFunctionNameIS = config.intersection_pg_name[i];

        switch(shape_type) {
            case MiOptixShapeType::Mesh:
                pgd[pg_count - 1].hitgroup.moduleIS = nullptr;
                break;
            case MiOptixShapeType::BSplineCurve:
                pgd[pg_count - 1].hitgroup.moduleIS = config.bspline_curve_module;
                break;
            case MiOptixShapeType::LinearCurve:
                pgd[pg_count - 1].hitgroup.moduleIS = config.linear_curve_module;
                break;
            default: // Custom shapes
                pgd[pg_count - 1].hitgroup.moduleIS = config.main_module;
        }
    }

    if (pg_count == 0){
        // Create a dummy program so that we can still build a valid pipeline
        pg_count = 1;
        pgd[0].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        pgd[0].hitgroup.moduleCH = nullptr;
        pgd[0].hitgroup.entryFunctionNameCH = nullptr;
        pgd[0].hitgroup.entryFunctionNameIS = nullptr;
        pgd[0].hitgroup.moduleIS = nullptr;
    }

    optix_log_size = sizeof(optix_log);
    check_log(optixProgramGroupCreate(
        config.context,
        pgd,
        pg_count,
        &program_group_options,
        optix_log,
        &optix_log_size,
        config.program_groups
    ));

    // Create this variable in the JIT scope 0 to ensure a consistent
    // ordering in the generated PTX kernel (e.g. for other scenes).
    uint32_t scope = jit_scope(JitBackend::CUDA);
    jit_set_scope(JitBackend::CUDA, 0);
    config.pipeline_jit_index = jit_optix_configure_pipeline(
        &config.pipeline_compile_options,
        config.main_module,
        config.program_groups,
        pg_count
    );
    jit_set_scope(JitBackend::CUDA, scope);

    return config_index;
}

MI_VARIANT void Scene<Float, Spectrum>::accel_init_gpu(const Properties &props) {
    DRJIT_MARK_USED(props);
    if constexpr (!dr::is_cuda_v<Float>) {
        return;
    }

    ScopedPhase phase(ProfilerPhase::InitAccel);
    Log(Info, "Building scene in OptiX ..");
    Timer timer;
    optix_initialize();

    m_accel = new MiOptixSceneState();
    MiOptixSceneState &s = *(MiOptixSceneState *) m_accel;

    // Check if another scene was passed to the constructor
    Scene *other_scene = nullptr;
    for (auto &[k, v] : props.objects()) {
        other_scene = dynamic_cast<Scene *>(v.get());
        if (other_scene)
            break;
    }

    /* When another scene is passed via props, the new scene should re-use
       the same configuration, pipeline and update the shader binding table
       rather than constructing a new one from scratch. This is necessary for
       two scenes to be ray traced within the same megakernel. */
    if (other_scene) {
        Log(Debug, "Re-use OptiX config, pipeline and update SBT ..");
        MiOptixSceneState &s2 = *(MiOptixSceneState *) other_scene->m_accel;

        const MiOptixConfig &config = optix_configs[s2.config_index];

        HitGroupSbtRecord* prev_data
            = (HitGroupSbtRecord*) jit_malloc_migrate(s2.sbt.hitgroupRecordBase, AllocType::Host, 1);
        dr::sync_thread();

        std::vector<HitGroupSbtRecord> hg_sbts;
        hg_sbts.assign(prev_data, prev_data + s2.sbt.hitgroupRecordCount);
        jit_free(prev_data);

        fill_hitgroup_records(
                m_shapes,
                hg_sbts,
                config.program_groups,
                config.program_index_mapping
        );
        for (auto &shapegroup : m_shapegroups) {
            shapegroup->optix_fill_hitgroup_records(
                hg_sbts,
                config.program_groups,
                config.program_index_mapping
            );
        }

        size_t shapes_count = hg_sbts.size();

        s2.sbt.hitgroupRecordBase = jit_malloc(
            AllocType::HostPinned, shapes_count * sizeof(HitGroupSbtRecord));
        s2.sbt.hitgroupRecordCount = (unsigned int) shapes_count;

        jit_memcpy_async(JitBackend::CUDA, s2.sbt.hitgroupRecordBase, hg_sbts.data(),
                         shapes_count * sizeof(HitGroupSbtRecord));

        s2.sbt.hitgroupRecordBase =
            jit_malloc_migrate(s2.sbt.hitgroupRecordBase, AllocType::Device, 1);

        jit_optix_update_sbt(s2.sbt_jit_index, &s2.sbt);

        memcpy(&s.sbt, &s2.sbt, sizeof(OptixShaderBindingTable));

        s.sbt_jit_index = s2.sbt_jit_index;
        jit_var_inc_ref(s.sbt_jit_index);

        s.config_index = s2.config_index;
    } else {
        // =====================================================
        //  Initialize OptiX configuration
        // =====================================================

        uint32_t optix_config_shapes = 0;
        for (auto& shape : m_shapes) {
            uint32_t type = shape->shape_type();

            if (type == +ShapeType::Mesh)
                optix_config_shapes |= (uint32_t) MiOptixConfigShapes::HasMeshes;
            else if (type == +ShapeType::Instance)
                optix_config_shapes |= (uint32_t) MiOptixConfigShapes::HasInstances;
            else if (type == +ShapeType::BSplineCurve)
                optix_config_shapes |= (uint32_t) MiOptixConfigShapes::HasBSplineCurves;
            else if (type == +ShapeType::LinearCurve)
                optix_config_shapes |= (uint32_t) MiOptixConfigShapes::HasLinearCurves;
            else
                optix_config_shapes |= (uint32_t) MiOptixConfigShapes::HasCustom;

            for (auto &shape : m_shapegroups) {
                if (shape->has_meshes())
                    optix_config_shapes |= (uint32_t) MiOptixConfigShapes::HasMeshes;
                if (shape->has_bspline_curves())
                    optix_config_shapes |= (uint32_t) MiOptixConfigShapes::HasBSplineCurves;
                if (shape->has_linear_curves())
                    optix_config_shapes |= (uint32_t) MiOptixConfigShapes::HasLinearCurves;
                if (shape->has_others())
                    optix_config_shapes |= (uint32_t) MiOptixConfigShapes::HasCustom;
            }
        }

        s.config_index = init_optix_config(optix_config_shapes);
        const MiOptixConfig &config = optix_configs[s.config_index];

        // =====================================================
        //  Shader Binding Table generation
        // =====================================================

        s.sbt.missRecordBase =
            jit_malloc(AllocType::HostPinned, sizeof(MissSbtRecord));
        s.sbt.missRecordStrideInBytes = sizeof(MissSbtRecord);
        s.sbt.missRecordCount = 1;

        jit_optix_check(optixSbtRecordPackHeader(config.program_groups[0],
                                                 s.sbt.missRecordBase));

        std::vector<HitGroupSbtRecord> hg_sbts;
        fill_hitgroup_records(m_shapes, hg_sbts, config.program_groups,
                              config.program_index_mapping);
        for (auto &shapegroup : m_shapegroups) {
            shapegroup->optix_fill_hitgroup_records(
                    hg_sbts,
                    config.program_groups,
                    config.program_index_mapping
            );
        }

        size_t shapes_count = hg_sbts.size();

        s.sbt.hitgroupRecordBase = jit_malloc(
            AllocType::HostPinned, shapes_count * sizeof(HitGroupSbtRecord));
        s.sbt.hitgroupRecordStrideInBytes = sizeof(HitGroupSbtRecord);
        s.sbt.hitgroupRecordCount = (unsigned int) shapes_count;

        jit_memcpy_async(JitBackend::CUDA, s.sbt.hitgroupRecordBase, hg_sbts.data(),
                         shapes_count * sizeof(HitGroupSbtRecord));

        s.sbt.missRecordBase =
            jit_malloc_migrate(s.sbt.missRecordBase, AllocType::Device, 1);
        s.sbt.hitgroupRecordBase =
            jit_malloc_migrate(s.sbt.hitgroupRecordBase, AllocType::Device, 1);

        s.sbt_jit_index = jit_optix_configure_sbt(&s.sbt, config.pipeline_jit_index);
    }

    // =====================================================
    //  Acceleration data structure building
    // =====================================================

    accel_parameters_changed_gpu();

    Log(Info, "OptiX ready. (took %s)", util::time_string((float) timer.value()));
}

MI_VARIANT void Scene<Float, Spectrum>::accel_parameters_changed_gpu() {
    if constexpr (dr::is_cuda_v<Float>) {
        dr::sync_thread();
        MiOptixSceneState &s = *(MiOptixSceneState *) m_accel;
        const MiOptixConfig &config = optix_configs[s.config_index];

        if (!m_shapes.empty()) {
            scoped_optix_context guard;

            // Build geometry acceleration structures for all the shapes
            build_gas(config.context, m_shapes, s.accel);
            for (auto& shapegroup: m_shapegroups)
                shapegroup->optix_build_gas(config.context);

            // Gather information about the instance acceleration structure to be built
            std::vector<OptixInstance> ias;
            prepare_ias(config.context, m_shapes, 0, s.accel, 0u, ScalarTransform4f(), ias);

            // Build a "master" IAS that contains all the GAS of the scene (meshes,
            // custom shapes, curves, ...)
            OptixAccelBuildOptions accel_options = {};
            accel_options.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
            accel_options.operation  = OPTIX_BUILD_OPERATION_BUILD;
            accel_options.motionOptions.numKeys = 0;

            size_t ias_data_size = ias.size() * sizeof(OptixInstance);
            void* d_ias = jit_malloc(AllocType::HostPinned, ias_data_size);
            jit_memcpy_async(JitBackend::CUDA, d_ias, ias.data(), ias_data_size);

            jit_free(s.ias_data.buffer);
            jit_free(s.ias_data.inputs);
            s.ias_data = {};
            s.ias_data.inputs = jit_malloc_migrate(d_ias, AllocType::Device, 1);

            OptixBuildInput build_input{};
            build_input.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
            build_input.instanceArray.instances = (CUdeviceptr) s.ias_data.inputs;
            build_input.instanceArray.numInstances = (unsigned int) ias.size();

            OptixAccelBufferSizes buffer_sizes{};
            jit_optix_check(optixAccelComputeMemoryUsage(
                config.context,
                &accel_options,
                &build_input,
                1,
                &buffer_sizes
            ));

            void* d_temp_buffer
                = jit_malloc(AllocType::Device, buffer_sizes.tempSizeInBytes);
            s.ias_data.buffer
                = jit_malloc(AllocType::Device, buffer_sizes.outputSizeInBytes);

            jit_optix_check(optixAccelBuild(
                config.context,
                (CUstream) jit_cuda_stream(),
                &accel_options,
                &build_input,
                1, // num build inputs
                (CUdeviceptr) d_temp_buffer,
                buffer_sizes.tempSizeInBytes,
                (CUdeviceptr) s.ias_data.buffer,
                buffer_sizes.outputSizeInBytes,
                &s.ias_handle,
                0, // emitted property list
                0  // num emitted properties
            ));

            jit_free(d_temp_buffer);
        }

        /* Set up a callback on the handle variable to release the OptiX scene
           state when this variable is freed. This ensures that the lifetime of
           the pipeline goes beyond the one of the Scene instance if there are
           still some pending ray tracing calls (e.g. non evaluated variables
           depending on a ray tracing call). */

        // Prevents the pipeline to be released when updating the scene parameters
        if (m_accel_handle.index())
            jit_var_set_callback(m_accel_handle.index(), nullptr, nullptr);
        m_accel_handle = dr::opaque<UInt64>(s.ias_handle);

        jit_var_set_callback(
            m_accel_handle.index(),
            [](uint32_t /* index */, int should_free, void *payload) {
                if (should_free) {
                    Log(Debug, "Free OptiX IAS..");
                    auto* s = (MiOptixSceneState *)payload;
                    jit_free(s->ias_data.buffer);
                    jit_free(s->ias_data.inputs);
                    delete s;
                }
            },
            (void *) m_accel
        );

        clear_shapes_dirty();
    }
}

MI_VARIANT void Scene<Float, Spectrum>::accel_release_gpu() {
    if constexpr (dr::is_cuda_v<Float>) {
        Log(Debug, "Scene GPU acceleration release ..");

        // Ensure all ray tracing kernels are terminated before releasing the scene
        dr::sync_thread();

        MiOptixSceneState *s = (MiOptixSceneState *) m_accel;

        /* This will decrease the reference count of the shader binding table
            JIT variable which might trigger the release of the OptiX SBT if
            no ray tracing calls are pending. */
        (void) UInt32::steal(s->sbt_jit_index);

        /* Decrease the reference count of the IAS handle variable. This will
           trigger the release of the OptiX acceleration data structure if no
           ray tracing calls are pending.
           This **needs** to be done after decreasing the SBT index */
        m_accel_handle = 0;

        m_accel = nullptr;
    }
}

MI_VARIANT void Scene<Float, Spectrum>::static_accel_initialization_gpu() { }
MI_VARIANT void Scene<Float, Spectrum>::static_accel_shutdown_gpu() {
    if constexpr (dr::is_cuda_v<Float>) {
        Log(Debug, "Scene static GPU acceleration shutdown ..");
        for (size_t j = 0; j < MI_OPTIX_CONFIG_COUNT; j++) {
            MiOptixConfig &config = optix_configs[j];
            if (config.pipeline_jit_index) {
                /* Decrease the reference count of the pipeline JIT variable.
                   This will trigger the release of the OptiX pipeline data
                   structure if no ray tracing calls are pending. */
                (void) UInt32::steal(config.pipeline_jit_index);

                for (size_t i = 0; i < MI_OPTIX_SHAPE_TYPE_COUNT; i++)
                    free(config.intersection_pg_name[i]);

                config.pipeline_jit_index = 0;
            }
        }
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_gpu(const Ray3f &ray,
                                                      Mask active) const {
    if constexpr (dr::is_cuda_v<Float>) {
        MiOptixSceneState &s = *(MiOptixSceneState *) m_accel;
        const MiOptixConfig &config = optix_configs[s.config_index];

        UInt32 ray_mask(255),
               ray_flags(OPTIX_RAY_FLAG_DISABLE_ANYHIT |
                         OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT),
               sbt_offset(0), sbt_stride(1), miss_sbt_index(0);

        bool has_instances = !m_shapegroups.empty();

        using Single = dr::float32_array_t<Float>;
        dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_mint(0.f), ray_maxt(ray.maxt), ray_time(ray.time);

        // Be careful with 'ray.maxt' in double precision variants
        if constexpr (!std::is_same_v<Single, Float>)
            ray_maxt = dr::minimum(ray_maxt, dr::Largest<Single>);

        uint32_t trace_args[] {
            m_accel_handle.index(),
            ray_o.x().index(), ray_o.y().index(), ray_o.z().index(),
            ray_d.x().index(), ray_d.y().index(), ray_d.z().index(),
            ray_mint.index(), ray_maxt.index(), ray_time.index(),
            ray_mask.index(), ray_flags.index(),
            sbt_offset.index(), sbt_stride.index(),
            miss_sbt_index.index(),
        };
        OptixHitObjectField fields[] {
            OptixHitObjectField::IsHit,
            OptixHitObjectField::RayTMax,
            OptixHitObjectField::Attribute0,
            OptixHitObjectField::Attribute1,
            OptixHitObjectField::PrimitiveIndex,
            OptixHitObjectField::SBTDataPointer,
            OptixHitObjectField::InstanceId
        };
        uint32_t hitobject_out[7];

        jit_optix_ray_trace(sizeof(trace_args) / sizeof(uint32_t), trace_args,
                            has_instances ? 7 : 6, fields, hitobject_out, false,
                            active.index(), config.pipeline_jit_index,
                            s.sbt_jit_index);

        Mask hitobject_is_hit = UInt32::steal(hitobject_out[0]) != 0;
        active &= hitobject_is_hit;

        // Get shape registry ID from SBT data (first 32 bits of `OptixHitGroupData`)
        UInt64 hitobject_sbt_ptr = UInt64::steal(hitobject_out[5]);
        uint32_t shape_id_idx = jit_optix_sbt_data_load(
            hitobject_sbt_ptr.index(), VarType::UInt32, 0, active.index());

        PreliminaryIntersection3f pi;
        pi.t          = dr::reinterpret_array<Single, UInt32>(UInt32::steal(hitobject_out[1]));
        pi.prim_uv[0] = dr::reinterpret_array<Single, UInt32>(UInt32::steal(hitobject_out[2]));
        pi.prim_uv[1] = dr::reinterpret_array<Single, UInt32>(UInt32::steal(hitobject_out[3]));
        pi.prim_index = UInt32::steal(hitobject_out[4]);
        pi.shape      = dr::reinterpret_array<ShapePtr, UInt32>(UInt32::steal(shape_id_idx));
        pi.instance   = has_instances ? ShapePtr::steal(hitobject_out[6]) : dr::zeros<ShapePtr>();

        // This field is only used by embree, but we still need to initialize it for vcalls
        pi.shape_index = dr::zeros<UInt32>();

        // jit_optix_ray_trace leaves payload data uninitialized for inactive lanes
        pi.t[!active] = dr::Infinity<Float>;

        // Ensure pointers are initialized to nullptr for inactive lanes
        active &= pi.is_valid();
        pi.shape[!active]    = nullptr;
        pi.instance[!active] = nullptr;

        return pi;
    } else {
        DRJIT_MARK_USED(ray);
        DRJIT_MARK_USED(active);
        Throw("ray_intersect_gpu() should only be called in GPU mode.");
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_gpu(const Ray3f &ray, uint32_t ray_flags,
                                          Mask active) const {
    if constexpr (dr::is_cuda_v<Float>) {
        PreliminaryIntersection3f pi = ray_intersect_preliminary_gpu(ray, active);
        return pi.compute_surface_interaction(ray, ray_flags, active);
    } else {
        DRJIT_MARK_USED(ray);
        DRJIT_MARK_USED(ray_flags);
        DRJIT_MARK_USED(active);
        Throw("ray_intersect_gpu() should only be called in GPU mode.");
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_gpu(const Ray3f &ray, Mask active) const {
    if constexpr (dr::is_cuda_v<Float>) {
        MiOptixSceneState &s = *(MiOptixSceneState *) m_accel;
        const MiOptixConfig &config = optix_configs[s.config_index];

        UInt32 ray_mask(255),
               ray_flags(OPTIX_RAY_FLAG_DISABLE_ANYHIT |
                         OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT |
                         OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT),
               sbt_offset(0), sbt_stride(1), miss_sbt_index(0);

        using Single = dr::float32_array_t<Float>;
        dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_mint(0.f), ray_maxt(ray.maxt), ray_time(ray.time);

        // Be careful with 'ray.maxt' in double precision variants
        if constexpr (!std::is_same_v<Single, Float>)
            ray_maxt = dr::minimum(ray_maxt, dr::Largest<Single>);

        uint32_t trace_args[] {
            m_accel_handle.index(),
            ray_o.x().index(), ray_o.y().index(), ray_o.z().index(),
            ray_d.x().index(), ray_d.y().index(), ray_d.z().index(),
            ray_mint.index(), ray_maxt.index(), ray_time.index(),
            ray_mask.index(), ray_flags.index(),
            sbt_offset.index(), sbt_stride.index(),
            miss_sbt_index.index()
        };

        OptixHitObjectField field = OptixHitObjectField::IsHit;
        uint32_t hitobject_out;

        jit_optix_ray_trace(sizeof(trace_args) / sizeof(uint32_t), trace_args,
                                1, &field, &hitobject_out,
                                false, active.index(),
                                config.pipeline_jit_index, s.sbt_jit_index);

        UInt32 hitobject_is_hit = UInt32::steal(hitobject_out);

        return active && (hitobject_is_hit != 0);
    } else {
        DRJIT_MARK_USED(ray);
        DRJIT_MARK_USED(active);
        Throw("ray_test_gpu() should only be called in GPU mode.");
    }
}

NAMESPACE_END(mitsuba)
