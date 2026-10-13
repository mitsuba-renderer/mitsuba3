#include <iomanip>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <functional>

#include <drjit-core/optix.h>

#include <nanothread/nanothread.h>

#include <mitsuba/render/optix/common.h>
#include <mitsuba/render/optix_api.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/scene_ir.h>
#include <tsl/robin_map.h>

#include "optix/accel.h"
#include "librender_ptx.h"

NAMESPACE_BEGIN(mitsuba)

// -----------------------------------------------------------------------
//  The scene is lowered once into the backend-neutral SceneIR
//  (SceneIRBuilder::build(), scene_ir.h) -- the only templated build step, since
//  it calls the virtual Shape::describe() and resolves ShapeGroups. Everything
//  downstream consumes the non-templated SceneIR and is implemented in
//  src/render/optix/accel.cpp.
// -----------------------------------------------------------------------

// Per scene OptiX state data structure (the by-value OptixAccel::state pimpl)
struct MiOptixSceneState {
    OptixShaderBindingTable sbt = {};
    MiOptixAccelData accel;  // GAS for the top-level BLASes (SceneIR::top_blases)
    OptixTraversableHandle ias_handle = 0ull;
    struct InstanceData {
        void* buffer = nullptr;  // Device-visible storage for IAS
        void* inputs = nullptr;  // Device-visible storage for OptixInstance array
    } ias_data;
    /// Per-shape SBT data buffers, owned here; refreshed on rebuild().
    ShapeDataBuffers shape_data;
    /// Per-ShapeGroup GAS, index-aligned with scene->m_shapegroups (sized once;
    /// only dirty groups rebuild, keeping the freeze-visible handles stable).
    std::vector<MiOptixAccelData> group_accel;
    /// Base SBT-record offset of this scene's hit records. Non-zero only on the
    /// shared-SBT megakernel path (``other_scene``), where this scene's records
    /// follow the host scene's. Set once in init().
    uint32_t sbt_record_base = 0;
    bool compact_accel = false;
    uint32_t sbt_jit_index;

    /// Copies of MiOptixConfig fields, cached to avoid a hash lookup + mutex lock.
    uint32_t config_key;
    OptixDeviceContext context;
    uint32_t pipeline_jit_index;
};

/// Maximum number of OptiX program groups that can be instantiated by any kernel
#define MI_MAX_PROGRAM_GROUPS  8


/// Pipeline components (modules, program groups) for one set of shape-type
/// features, shared across all scenes that need it and keyed by \ref key.
/// \ref Scene::static_accel_shutdown frees them.
struct MiOptixConfig {
    OptixDeviceContext context;
    OptixPipelineCompileOptions pipeline_compile_options;
    OptixModule main_module;
    OptixModule bspline_curve_module; /// Built-in module for B-spline curves
    OptixModule linear_curve_module; /// Built-in module for linear curves
    OptixProgramGroup pg[MI_MAX_PROGRAM_GROUPS];
    OptixProgramGroupMapping pg_mapping;
    uint32_t pipeline_jit_index;
    uint32_t key;
};

// Array storing previously initialized optix configurations
static tsl::robin_map<uint32_t, MiOptixConfig> optix_configs;
static std::mutex optix_configs_lock;
static constexpr uint32_t OptixConfigCompactKey = 1u << 31;

const MiOptixConfig &init_optix_config(uint32_t shape_types, bool compact) {
    // Instances/groups are handled by IAS traversal, not by intersection
    // programs. Mask the bits so they don't spuriously add the CUSTOM flag
    shape_types &= ~((uint32_t) ShapeType::Instance |
                     (uint32_t) ShapeType::ShapeGroup);

    uint32_t key = shape_types;
    if ((key & +ShapeType::Rectangle) == +ShapeType::Rectangle) {
        key &= ~ShapeType::Rectangle; // Rectangles are actually meshes
        key |= +ShapeType::Mesh;
    }
    if (compact)
        key |= OptixConfigCompactKey;

    // Use flags as config index in optix_configs
    auto [it, success] = optix_configs.try_emplace(key);
    if (!success)
        return it->second;

    Log(Debug, "Initialize Optix configuration (key=0x%x, compact=%s)..",
        key, compact ? "true" : "false");

    MiOptixConfig &config = it.value();
    config.context = jit_optix_context();
    config.key = key;

    // =====================================================
    // Setup OptiX module + pipeline compilation options
    // =====================================================

    OptixModuleCompileOptions module_compile_options { };
    module_compile_options.maxRegisterCount = OPTIX_COMPILE_DEFAULT_MAX_REGISTER_COUNT;

    if (jit_flag(JitFlag::Debug)) {
        module_compile_options.optLevel   = OPTIX_COMPILE_OPTIMIZATION_LEVEL_0;
        module_compile_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
    } else {
        module_compile_options.optLevel   = OPTIX_COMPILE_OPTIMIZATION_DEFAULT;
        module_compile_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_NONE;
    }

    config.pipeline_compile_options.usesMotionBlur     = false;
    config.pipeline_compile_options.numPayloadValues   = 0;
    config.pipeline_compile_options.numAttributeValues = 2; // the minimum legal value
    config.pipeline_compile_options.pipelineLaunchParamsVariableName = "params";
    config.pipeline_compile_options.traversableGraphFlags =
        OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;

    if (jit_flag(JitFlag::Debug))
        config.pipeline_compile_options.exceptionFlags =
            OPTIX_EXCEPTION_FLAG_TRACE_DEPTH |
            OPTIX_EXCEPTION_FLAG_STACK_OVERFLOW;
    else
        config.pipeline_compile_options.exceptionFlags =
            OPTIX_EXCEPTION_FLAG_NONE;

    // Compute flags informing OptiX of the present shape types
    unsigned int prim_flags = 0, st = shape_types;
    if (st & ShapeType::Mesh) {
        prim_flags |= OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE;
        st &= ~(ShapeType::Mesh | ShapeType::Rectangle);
    }

    if (st & ShapeType::BSplineCurve) {
        prim_flags |= OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_CUBIC_BSPLINE;
        st &= ~ShapeType::BSplineCurve;
    }

    if (st & ShapeType::LinearCurve) {
        prim_flags |= OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_LINEAR;
        st &= ~ShapeType::LinearCurve;

    }

    if (st)
        prim_flags |= OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM;

    unsigned int accel_build_flags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
    if (compact)
        accel_build_flags |= OPTIX_BUILD_FLAG_ALLOW_COMPACTION;

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
    // Load additional Optix modules as needed
    // =====================================================

    if (prim_flags & OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_CUBIC_BSPLINE) {
        OptixBuiltinISOptions options = {};
        options.builtinISModuleType   = OPTIX_PRIMITIVE_TYPE_ROUND_CUBIC_BSPLINE;
        options.usesMotionBlur        = false;
        options.curveEndcapFlags      = 0;
        // buildFlags must match the flags used in OptixAccelBuildOptions (optix/accel.cpp)
        options.buildFlags            = accel_build_flags;
        jit_optix_check(
            optixBuiltinISModuleGet(config.context, &module_compile_options,
                                    &config.pipeline_compile_options,
                                    &options, &config.bspline_curve_module));
    }

    if (prim_flags & OPTIX_PRIMITIVE_TYPE_FLAGS_ROUND_LINEAR) {
        OptixBuiltinISOptions options = {};
        options.builtinISModuleType = OPTIX_PRIMITIVE_TYPE_ROUND_LINEAR;
        options.usesMotionBlur      = false;
        options.curveEndcapFlags    = 0;
        // buildFlags must match the flags used in OptixAccelBuildOptions (optix/accel.cpp)
        options.buildFlags          = accel_build_flags;
        jit_optix_check(
            optixBuiltinISModuleGet(config.context, &module_compile_options,
                                    &config.pipeline_compile_options,
                                    &options, &config.linear_curve_module));
    }

    if (prim_flags & OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM) {
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
    }

    // =====================================================
    // Generate program groups
    // =====================================================

    OptixProgramGroupOptions program_group_options = {};
    OptixProgramGroupDesc pgd[MI_MAX_PROGRAM_GROUPS];
    memset(pgd, 0, sizeof(pgd)); // Use memset due to union member.
    uint32_t pg_count = 0;

    // First hitgroup is for triangle meshes. We always create it, as
    // empty pipelines cause linker errors.
    pgd[pg_count].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
    config.pg_mapping[ShapeType::Mesh] = pg_count++;

    if (shape_types & (uint32_t) ShapeType::BSplineCurve) {
        pgd[pg_count].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        pgd[pg_count].hitgroup.moduleIS = config.bspline_curve_module;
        config.pg_mapping[ShapeType::BSplineCurve] = pg_count++;
    }

    if (shape_types & (uint32_t) ShapeType::LinearCurve) {
        pgd[pg_count].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        pgd[pg_count].hitgroup.moduleIS = config.linear_curve_module;
        config.pg_mapping[ShapeType::LinearCurve] = pg_count++;
    }

    if (shape_types & (uint32_t) ShapeType::Sphere) {
        pgd[pg_count].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        pgd[pg_count].hitgroup.entryFunctionNameIS = "__intersection__sphere";
        pgd[pg_count].hitgroup.moduleIS = config.main_module;
        config.pg_mapping[ShapeType::Sphere] = pg_count++;
    }

    if (shape_types & (uint32_t) ShapeType::Disk) {
        pgd[pg_count].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        pgd[pg_count].hitgroup.entryFunctionNameIS = "__intersection__disk";
        pgd[pg_count].hitgroup.moduleIS = config.main_module;
        config.pg_mapping[ShapeType::Disk] = pg_count++;
    }

    if (shape_types & (uint32_t) ShapeType::Cylinder) {
        pgd[pg_count].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        pgd[pg_count].hitgroup.entryFunctionNameIS = "__intersection__cylinder";
        pgd[pg_count].hitgroup.moduleIS = config.main_module;
        config.pg_mapping[ShapeType::Cylinder] = pg_count++;
    }

    if (shape_types & (uint32_t) ShapeType::SDFGrid) {
        pgd[pg_count].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        pgd[pg_count].hitgroup.entryFunctionNameIS = "__intersection__sdfgrid";
        pgd[pg_count].hitgroup.moduleIS = config.main_module;
        config.pg_mapping[ShapeType::SDFGrid] = pg_count++;
    }

    if (shape_types & (uint32_t) ShapeType::Ellipsoids) {
        pgd[pg_count].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        pgd[pg_count].hitgroup.entryFunctionNameIS = "__intersection__ellipsoids";
        pgd[pg_count].hitgroup.moduleIS = config.main_module;
        config.pg_mapping[ShapeType::Ellipsoids] = pg_count++;
    }

    // Note: adding further hit groups will likely require bumping
    // MI_MAX_PROGRAM_GROUPS

    optix_log_size = sizeof(optix_log);
    check_log(optixProgramGroupCreate(
        config.context,
        pgd,
        pg_count,
        &program_group_options,
        optix_log,
        &optix_log_size,
        config.pg
    ));

    // Create this variable in the JIT scope 0 to ensure a consistent
    // ordering in the generated PTX kernel (e.g. for other scenes).
    uint32_t scope = jit_scope(JitBackend::CUDA);
    jit_set_scope(JitBackend::CUDA, 0);
    config.pipeline_jit_index = jit_optix_configure_pipeline(
        &config.pipeline_compile_options,
        config.main_module,
        config.pg,
        pg_count
    );
    jit_set_scope(JitBackend::CUDA, scope);

    return config;
}

// -----------------------------------------------------------------------
//  OptixAccel<Float, Spectrum> -- lifecycle
// -----------------------------------------------------------------------

/// Build the per-BLAS GAS and the scene IAS from the lowered \ref SceneIR,
/// then publish the IAS as the rebindable accel handle and arm the scene-state
/// cleanup callback. Shared by init() (which passes the SceneIR it already
/// gathered for the SBT) and rebuild().
template <typename Float, typename Spectrum>
static void optix_rebuild_accel(
        Scene<Float, Spectrum> *scene, MiOptixSceneState *state,
        dr::uint64_array_t<Float> &accel_handle,
        const SceneIR &sd) {
    using UInt64 = dr::uint64_array_t<Float>;
    MiOptixSceneState &s = *state;

    if (!scene->shapes().empty()) {
        scoped_optix_context guard;

        // Refresh custom-shape SBT data in place: the SBT keeps pointing at the
        // same per-shape buffers, so updated per-primitive values reach the
        // device without rebuilding it.
        optix_refresh_shape_data(sd, s.shape_data);

        // Build one GAS per top-level BLAS, then per ShapeGroup. The group GAS
        // lives in the scene state (sized once, index-aligned with
        // scene->shapegroups()); a dirty group is rebuilt and its freeze-visible
        // handles refreshed.
        build_gas(s.context, sd.blases, sd.top_blases, s.accel,
                  s.compact_accel);
        auto &shapegroups = scene->shapegroups();
        for (size_t i = 0; i < shapegroups.size(); ++i) {
            auto &sg = shapegroups[i];
            MiOptixAccelData &group_gas = s.group_accel[i];
            if (sg->dirty()) {
                build_gas(s.context, sd.blases, sd.group_blases[i], group_gas,
                          s.compact_accel);
                std::vector<UInt64> handles;
                handles.reserve(group_gas.gas.size());
                for (const MiOptixAccelData::HandleData &h : group_gas.gas)
                    handles.push_back(dr::opaque<UInt64>(h.handle));
                sg->set_accel_handles(std::move(handles));
            }
        }

        // Resolve, by global BLAS index, each BLAS's GAS traversable.
        std::vector<OptixTraversableHandle> blas_handle(sd.blases.size(), 0ull);
        for (size_t k = 0; k < sd.top_blases.size(); ++k)
            blas_handle[sd.top_blases[k]] = s.accel.gas[k].handle;
        for (size_t i = 0; i < shapegroups.size(); ++i) {
            const std::vector<uint32_t> &idx = sd.group_blases[i];
            for (size_t k = 0; k < idx.size(); ++k)
                blas_handle[idx[k]] = s.group_accel[i].gas[k].handle;
        }

        // blas_sbt_offset[b] is the index of BLAS b's first hit record. There is
        // one record per geometry, written in BLAS order by fill_hitgroup_records,
        // so the offset is a prefix sum of the earlier BLASes' geometry counts.
        // The sum starts at sbt_record_base, which is nonzero only when this
        // scene's records are appended after another scene's in a shared SBT.
        std::vector<uint32_t> blas_sbt_offset(sd.blases.size());
        uint32_t off = s.sbt_record_base;
        for (size_t b = 0; b < sd.blases.size(); ++b) {
            blas_sbt_offset[b] = off;
            off += (uint32_t) sd.blases[b].geoms.size();
        }

        // One OptixInstance per SceneIR instance. Size a host-visible shared
        // buffer up front, fill the OptixInstances straight into it (no host
        // staging), and upload it with a single migrate.
        size_t ias_count = sd.instances.size();

        auto *ias = (OptixInstance *) jit_malloc(
            JitBackend::CUDA, ias_count * sizeof(OptixInstance), /* shared = */ 1);

        prepare_ias(sd, blas_handle, blas_sbt_offset, ias);

        // Build a "master" IAS that contains all the GAS of the scene (meshes,
        // custom shapes, curves, ...)
        OptixAccelBuildOptions accel_options = {};
        accel_options.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
        accel_options.operation  = OPTIX_BUILD_OPERATION_BUILD;
        accel_options.motionOptions.numKeys = 0;

        jit_free(s.ias_data.buffer);
        jit_free(s.ias_data.inputs);
        s.ias_data = {};
        s.ias_data.inputs = jit_malloc_migrate(ias, JitBackend::CUDA, 1);

        OptixBuildInput build_input;
        memset(&build_input, 0, sizeof(OptixBuildInput)); // Use memset due to union member.
        build_input.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
        build_input.instanceArray.instances = (CUdeviceptr) s.ias_data.inputs;
        build_input.instanceArray.numInstances = (unsigned int) ias_count;

        OptixAccelBufferSizes buffer_sizes{};
        jit_optix_check(optixAccelComputeMemoryUsage(
            s.context, &accel_options, &build_input, 1, &buffer_sizes));

        void *d_temp_buffer =
            jit_malloc(JitBackend::CUDA, buffer_sizes.tempSizeInBytes);
        s.ias_data.buffer =
            jit_malloc(JitBackend::CUDA, buffer_sizes.outputSizeInBytes);

        jit_optix_check(optixAccelBuild(
            s.context, (CUstream) jit_cuda_stream(), &accel_options,
            &build_input, 1, (CUdeviceptr) d_temp_buffer,
            buffer_sizes.tempSizeInBytes, (CUdeviceptr) s.ias_data.buffer,
            buffer_sizes.outputSizeInBytes, &s.ias_handle, 0, 0));

        jit_free(d_temp_buffer);
    }

    // Set up a callback on the handle variable to release the OptiX scene
    // state when this variable is freed. This ensures that the lifetime of
    // the pipeline goes beyond the one of the Scene instance if there are
    // still some pending ray tracing calls (e.g. non evaluated variables
    // depending on a ray tracing call). The by-value OptixAccel dies with the
    // Scene, so the callback owns the heap MiOptixSceneState it captures.

    // Prevents the pipeline to be released when updating the scene parameters
    if (accel_handle.index())
        jit_var_set_callback(accel_handle.index(), nullptr, nullptr);
    accel_handle = dr::opaque<UInt64>(s.ias_handle);

    jit_var_set_callback(
        accel_handle.index(),
        [](uint32_t /* index */, int should_free, void *payload) {
            if (should_free) {
                Log(Debug, "Free OptiX IAS..");
                auto *s = (MiOptixSceneState *) payload;
                jit_free(s->ias_data.buffer);
                jit_free(s->ias_data.inputs);
                for (void *buf : s->shape_data)
                    jit_free(buf);
                delete s;
            }
        },
        (void *) state);
}

template <typename Float, typename Spectrum>
void OptixAccel<Float, Spectrum>::init(Scene<Float, Spectrum> *scene,
                                       const Properties &props) {
    ScopedPhase phase(ProfilerPhase::InitAccel);
    Log(Info, "Building scene in OptiX ..");
    Timer timer;
    optix_initialize();

    state = new MiOptixSceneState();
    MiOptixSceneState &s = *state;

    // Check if another scene was passed to the constructor
    Scene<Float, Spectrum> *other_scene = nullptr;
    for (auto &prop : props.objects()) {
        other_scene = prop.try_get<Scene<Float, Spectrum>>();
        if (other_scene)
            break;
    }

    // A scene passed via props shares the host scene's configuration, pipeline,
    // and shader binding table so both scenes can be ray traced within one
    // megakernel.

    // The following manipulates global state placed within a critical section
    std::lock_guard<std::mutex> guard(optix_configs_lock);

    // Lower the scene once; the SBT records, GAS and IAS all read from this.
    SceneIR sd = SceneIRBuilder<Float, Spectrum>::build(scene);

    // Per-group GAS storage (index-aligned with m_shapegroups), sized once here
    // and reused across rebuilds.
    s.group_accel.resize(scene->m_shapegroups.size());

    // Fill the hit-group records: one per geom of every BLAS, packed in BLAS
    // order (top-level BLASes first, then groups) so the SBT offsets line up with
    // the IAS instances. Shared by the SBT-update and from-scratch paths below.
    auto fill_records = [&](HitGroupSbtRecord *records, size_t cursor,
                            const MiOptixConfig &config) {
        fill_hitgroup_records(sd.blases, records, cursor, config.pg,
                              config.pg_mapping, s.shape_data);
    };

    if (other_scene) {
        Log(Debug, "Re-use OptiX config, pipeline and update SBT ..");
        MiOptixSceneState &s2 = *other_scene->m_accel.state;
        if (scene->m_compact_accel != s2.compact_accel)
            Throw("OptiX scenes sharing one SBT must use the same "
                  "compact_acceleration_structures setting.");

        const MiOptixConfig &config = optix_configs[s2.config_key];

        HitGroupSbtRecord* prev_data
            = (HitGroupSbtRecord*) jit_malloc_migrate(s2.sbt.hitgroupRecordBase, JitBackend::None, 1);
        dr::sync_thread();

        // The updated SBT keeps the host scene's records up front, then appends
        // this scene's records in one shared buffer.
        size_t prev_count = s2.sbt.hitgroupRecordCount;
        size_t shapes_count = prev_count + count_hitgroup_records(sd.blases);

        // This scene's records follow the host scene's (see sbt_record_base).
        s.sbt_record_base = (uint32_t) prev_count;

        auto *records = (HitGroupSbtRecord *) jit_malloc(
            JitBackend::CUDA, shapes_count * sizeof(HitGroupSbtRecord),
            /* shared = */ 1);
        memcpy(records, prev_data, prev_count * sizeof(HitGroupSbtRecord));
        jit_free(prev_data);

        fill_records(records, prev_count, config);

        s2.sbt.hitgroupRecordCount = (unsigned int) shapes_count;
        s2.sbt.hitgroupRecordBase =
            jit_malloc_migrate(records, JitBackend::CUDA, 1);

        jit_optix_update_sbt(s2.sbt_jit_index, &s2.sbt);

        memcpy(&s.sbt, &s2.sbt, sizeof(OptixShaderBindingTable));

        s.sbt_jit_index = s2.sbt_jit_index;
        jit_var_inc_ref(s.sbt_jit_index);

        s.config_key = s2.config_key;
        s.compact_accel = s2.compact_accel;
        s.context = s2.context;
        s.pipeline_jit_index = s2.pipeline_jit_index;
    } else {
        // =====================================================
        //  Initialize OptiX configuration
        // =====================================================

        const MiOptixConfig &config =
            init_optix_config(scene->shape_types(), scene->m_compact_accel);

        // =====================================================
        //  Shader Binding Table generation
        // =====================================================

        s.sbt.missRecordBase =
            jit_malloc(JitBackend::CUDA, sizeof(MissSbtRecord), /* shared = */ 1);
        s.sbt.missRecordStrideInBytes = sizeof(MissSbtRecord);
        s.sbt.missRecordCount = 1;

        jit_optix_check(optixSbtRecordPackHeader(config.pg[0],
                                                 s.sbt.missRecordBase));

        // Pack the records straight into a host-visible shared buffer, then
        // upload it with a single queue-ordered migrate.
        size_t shapes_count = count_hitgroup_records(sd.blases);

        auto *records = (HitGroupSbtRecord *) jit_malloc(
            JitBackend::CUDA, shapes_count * sizeof(HitGroupSbtRecord),
            /* shared = */ 1);

        fill_records(records, 0, config);

        s.sbt.hitgroupRecordStrideInBytes = sizeof(HitGroupSbtRecord);
        s.sbt.hitgroupRecordCount = (unsigned int) shapes_count;

        s.sbt.missRecordBase =
            jit_malloc_migrate(s.sbt.missRecordBase, JitBackend::CUDA, 1);
        s.sbt.hitgroupRecordBase =
            jit_malloc_migrate(records, JitBackend::CUDA, 1);

        s.sbt_jit_index = jit_optix_configure_sbt(&s.sbt, config.pipeline_jit_index);
        s.config_key = config.key;
        s.compact_accel = scene->m_compact_accel;
        s.context = config.context;
        s.pipeline_jit_index = config.pipeline_jit_index;
    }

    // Expose the out-of-band shader binding table as a rebindable freeze input.
    // ``s.sbt_jit_index`` is stable for the lifetime of this scene (in-place
    // SBT updates keep the same struct pointer), so this handle is set once
    // here and traversed via fields_().
    sbt_handle = UInt64::steal(jit_optix_sbt_owner_handle(s.sbt_jit_index));

    // =====================================================
    //  Acceleration data structure building
    // =====================================================

    // Build the GAS/IAS from the SceneIR already lowered above for the SBT,
    // so each shape is described only once.
    optix_rebuild_accel<Float, Spectrum>(scene, state, accel_handle, sd);

    Log(Info, "OptiX ready. (took %s)", util::time_string((float) timer.value()));
}

template <typename Float, typename Spectrum>
void OptixAccel<Float, Spectrum>::rebuild(
    Scene<Float, Spectrum> *scene) {
    // Lower the scene once; the GAS and IAS phases read from this.
    SceneIR sd = SceneIRBuilder<Float, Spectrum>::build(scene);

    optix_rebuild_accel<Float, Spectrum>(scene, state, accel_handle, sd);
}

template <typename Float, typename Spectrum>
void OptixAccel<Float, Spectrum>::release() {
    if (!state)
        return;
    Log(Debug, "Scene GPU acceleration release ..");

    // Ensure all ray tracing kernels are terminated before releasing the scene
    dr::sync_thread();

    // Drop the rebindable SBT handle. It only maps the SBT struct pointer
    // (free=0) and does not own it, so this just releases the handle variable.
    // The SBT itself is released below via ``sbt_jit_index``.
    sbt_handle = 0;

    // This will decrease the reference count of the shader binding table JIT
    // variable which might trigger the release of the OptiX SBT if no ray
    // tracing calls are pending.
    (void) UInt32::steal(state->sbt_jit_index);

    // Decrease the reference count of the IAS handle variable. This will
    // trigger the (deferred) release of the OptiX acceleration data structure
    // and the MiOptixSceneState if no ray tracing calls are pending. This
    // needs to be done after decreasing the SBT index.
    accel_handle = 0;

    state = nullptr;
}

template <typename Float, typename Spectrum>
void OptixAccel<Float, Spectrum>::static_shutdown() {
    Log(Debug, "Scene static GPU acceleration shutdown ..");
    for (auto &kv : optix_configs) {
        // Decrease the reference count of the pipeline JIT variable.
        // This will trigger the release of the OptiX pipeline data
        // structure if no ray tracing calls are pending.
        jit_var_dec_ref(kv.second.pipeline_jit_index);
    }
    tsl::robin_map<uint32_t, MiOptixConfig> empty_map;
    optix_configs.swap(empty_map);
}

// -----------------------------------------------------------------------
//  OptixAccel<Float, Spectrum> -- ray queries
// -----------------------------------------------------------------------

template <typename Float, typename Spectrum>
typename OptixAccel<Float, Spectrum>::PreliminaryIntersection3f
OptixAccel<Float, Spectrum>::ray_intersect_preliminary(
    const Scene<Float, Spectrum> *scene, const Ray3f &ray, Mask /*coherent*/,
    bool reorder, UInt32 reorder_hint, uint32_t reorder_hint_bits,
    Mask active) const {
    MiOptixSceneState &s = *state;

    UInt32 ray_mask(255),
           ray_flags(OPTIX_RAY_FLAG_DISABLE_ANYHIT |
                     OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT),
           sbt_offset(0), sbt_stride(1), miss_sbt_index(0);

    // Enforce backface culling, which is only enabled on EllipsoidsMesh
    // instances.
    ray_flags |= OPTIX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    bool has_instances = !scene->m_shapegroups.empty();

    using Single = dr::float32_array_t<Float>;
    dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
    Single ray_mint(0.f), ray_maxt(ray.maxt), ray_time(ray.time);

    // Be careful with 'ray.maxt' in double precision variants
    if constexpr (!std::is_same_v<Single, Float>)
        ray_maxt = dr::minimum(ray_maxt, dr::Largest<Single>);

    uint32_t trace_args[] {
        accel_handle.index(),
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

    // Scene property takes precedence
    reorder &= scene->m_thread_reordering;

    jit_optix_ray_trace(sizeof(trace_args) / sizeof(uint32_t), trace_args,
                        has_instances ? 7 : 6, fields, hitobject_out,
                        reorder, reorder_hint.index(), reorder_hint_bits,
                        false, active.index(),
                        s.pipeline_jit_index, s.sbt_jit_index);

    Mask valid = UInt32::steal(hitobject_out[0]) != 0;
    active &= valid;

    // Get shape registry ID from SBT data (first 32 bits of `OptixHitGroupData`)
    UInt64 hitobject_sbt_ptr = UInt64::steal(hitobject_out[5]);
    uint32_t shape_id_idx = jit_optix_sbt_data_load(
        hitobject_sbt_ptr.index(), VarType::UInt32, 0, active.index());

    PreliminaryIntersection3f pi;
    pi.valid      = valid;
    pi.t          = dr::reinterpret_array<Single, UInt32>(UInt32::steal(hitobject_out[1]));
    pi.prim_uv[0] = dr::reinterpret_array<Single, UInt32>(UInt32::steal(hitobject_out[2]));
    pi.prim_uv[1] = dr::reinterpret_array<Single, UInt32>(UInt32::steal(hitobject_out[3]));
    pi.prim_index = UInt32::steal(hitobject_out[4]);
    pi.shape      = dr::reinterpret_array<ShapePtr, UInt32>(UInt32::steal(shape_id_idx));
    pi.instance   = has_instances ? ShapePtr::steal(hitobject_out[6]) : dr::zeros<ShapePtr>();

    // This field is only used by embree, but we still need to initialize it for vcalls
    pi.shape_index = dr::zeros<UInt32>();

    return pi;
}

template <typename Float, typename Spectrum>
typename OptixAccel<Float, Spectrum>::Mask
OptixAccel<Float, Spectrum>::ray_test(const Scene<Float, Spectrum> * /*scene*/,
                                      const Ray3f &ray, Mask /*coherent*/,
                                      Mask active) const {
    MiOptixSceneState &s = *state;

    UInt32 ray_mask(255),
           ray_flags(OPTIX_RAY_FLAG_DISABLE_ANYHIT |
                     OPTIX_RAY_FLAG_TERMINATE_ON_FIRST_HIT |
                     OPTIX_RAY_FLAG_DISABLE_CLOSESTHIT),
           sbt_offset(0), sbt_stride(1), miss_sbt_index(0);

    // Enforce backface culling, which is only enabled on EllipsoidsMesh
    // instances.
    ray_flags |= OPTIX_RAY_FLAG_CULL_BACK_FACING_TRIANGLES;

    using Single = dr::float32_array_t<Float>;
    dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
    Single ray_mint(0.f), ray_maxt(ray.maxt), ray_time(ray.time);

    // Be careful with 'ray.maxt' in double precision variants
    if constexpr (!std::is_same_v<Single, Float>)
        ray_maxt = dr::minimum(ray_maxt, dr::Largest<Single>);

    uint32_t trace_args[] {
        accel_handle.index(),
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
                        false, 0, 0, false, active.index(),
                        s.pipeline_jit_index, s.sbt_jit_index);

    return UInt32::steal(hitobject_out) != 0;
}

template <typename Float, typename Spectrum>
typename OptixAccel<Float, Spectrum>::SurfaceInteraction3f
OptixAccel<Float, Spectrum>::ray_intersect_naive(
    const Scene<Float, Spectrum> * /*scene*/, const Ray3f & /*ray*/,
    Mask /*active*/) const {
    NotImplementedError("ray_intersect_naive");
}

NAMESPACE_END(mitsuba)
