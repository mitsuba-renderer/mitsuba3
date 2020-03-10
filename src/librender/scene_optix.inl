#include <optix.h>
#include <optix_cuda_interop.h>
#include "librender_ptx.h"

NAMESPACE_BEGIN(mitsuba)

#define rt_check(err)  __rt_check(s.context, err, __FILE__, __LINE__)

void __rt_check(RTcontext context, RTresult errval, const char *file, const int line) {
    if (errval != RT_SUCCESS) {
        const char *message;
        rtContextGetErrorString(context, errval, &message);
        if (errval == 1546)
            message = "Failed to load OptiX library! Very likely, your NVIDIA graphics "
                "driver is too old and not compatible with the version of OptiX that is "
                "being used. In particular, OptiX 6.5 requires driver revision R435.80 or newer.";
        fprintf(stderr,
                "rt_check(): OptiX API error = %04d (%s) in "
                "%s:%i.\n", (int) errval, message, file, line);
        exit(EXIT_FAILURE);
    }
}

constexpr size_t kOptixVariableCount = 30;
constexpr int kDeviceID = 0;

struct OptixState {
    RTcontext context;
    RTbuffer var_buf[kOptixVariableCount];
    RTmaterial material;
    RTprogram attr_prog;
    RTacceleration accel;
    RTgeometrygroup group;
};

MTS_VARIANT void Scene<Float, Spectrum>::accel_init_gpu(const Properties &/*props*/) {
    m_accel = new OptixState();
    OptixState &s = *(OptixState *) m_accel;

    rt_check(rtContextCreate(&s.context));
    int devices = 0;
    rt_check(rtContextSetDevices(s.context, 1, &devices));

    rt_check(rtContextSetRayTypeCount(s.context, 1));
    rt_check(rtContextSetEntryPointCount(s.context, 2));
    rt_check(rtContextSetStackSize(s.context, 0));
    rt_check(rtContextSetMaxTraceDepth(s.context, 1));
    rt_check(rtContextSetMaxCallableProgramDepth(s.context, 0));

    const char *var_names[kOptixVariableCount] = {
        "in_mask",          "in_ox",       "in_oy",       "in_oz",
        "in_dx",            "in_dy",       "in_dz",       "in_mint",
        "in_maxt",          "out_t",       "out_p_x",     "out_p_y",
        "out_p_z",          "out_u",       "out_v",       "out_ng_x",
        "out_ng_y",         "out_ng_z",    "out_ns_x",    "out_ns_y",
        "out_ns_z",         "out_dp_du_x", "out_dp_du_y", "out_dp_du_z",
        "out_dp_dv_x",      "out_dp_dv_y", "out_dp_dv_z", "out_shape_ptr",
        "out_primitive_id", "out_hit"
    };

    RTvariable var_obj[kOptixVariableCount];
    for (size_t i = 0; i < kOptixVariableCount; ++i) {
        RTformat fmt = RT_FORMAT_FLOAT;
        if (strstr(var_names[i], "_id") != nullptr)
            fmt =  RT_FORMAT_UNSIGNED_INT;
        else if (strstr(var_names[i], "_ptr") != nullptr)
            fmt =  RT_FORMAT_UNSIGNED_LONG_LONG;
        else if (strstr(var_names[i], "_mask") != nullptr ||
                 strstr(var_names[i], "_hit") != nullptr)
            fmt = RT_FORMAT_UNSIGNED_BYTE;
        rt_check(rtBufferCreate(s.context, RT_BUFFER_INPUT | RT_BUFFER_COPY_ON_DIRTY, &s.var_buf[i]));
        rt_check(rtBufferSetFormat(s.var_buf[i], fmt));
        rt_check(rtBufferSetSize1D(s.var_buf[i], 0));
        rt_check(rtBufferSetDevicePointer(s.var_buf[i], kDeviceID, (void *) 8));
        rt_check(rtContextDeclareVariable(s.context, var_names[i], &var_obj[i]));
        rt_check(rtVariableSetObject(var_obj[i], s.var_buf[i]));
    }

    RTprogram prog[6];
    rt_check(rtProgramCreateFromPTXString(s.context, (const char *) optix_rt_ptx,   "ray_gen_closest", &prog[0]));
    rt_check(rtProgramCreateFromPTXString(s.context, (const char *) optix_rt_ptx,   "ray_gen_any",     &prog[1]));
    rt_check(rtProgramCreateFromPTXString(s.context, (const char *) optix_rt_ptx,   "ray_miss",        &prog[2]));
    rt_check(rtProgramCreateFromPTXString(s.context, (const char *) optix_rt_ptx,   "ray_hit",         &prog[3]));
    rt_check(rtProgramCreateFromPTXString(s.context, (const char *) optix_attr_ptx, "ray_attr",        &prog[4]));

#if !defined(MTS_OPTIX_DEBUG)
        rt_check(rtContextSetExceptionEnabled(s.context, RT_EXCEPTION_ALL, 0));
        rt_check(rtContextSetPrintEnabled(s.context, 0));
#else
        rt_check(rtProgramCreateFromPTXString(s.context, (const char *) optix_rt_ptx, "ray_err", &prog[5]));
        rt_check(rtContextSetExceptionEnabled(s.context, RT_EXCEPTION_ALL, 1));
        rt_check(rtContextSetPrintEnabled(s.context, 1));
        rt_check(rtContextSetPrintBufferSize(s.context, 4096));
        rt_check(rtContextSetUsageReportCallback(
            s.context,
            (RTusagereportcallback)([](int, const char *descr, const char *msg, void *) -> void {
                std::cout << descr << " " << msg;
            }), 3, nullptr));
        rt_check(rtContextSetExceptionProgram(s.context, 0, prog[5]));
        rt_check(rtContextSetExceptionProgram(s.context, 1, prog[5]));
#endif

    rt_check(rtContextSetRayGenerationProgram(s.context, 0, prog[0]));
    rt_check(rtContextSetRayGenerationProgram(s.context, 1, prog[1]));
    rt_check(rtContextSetMissProgram(s.context, 0, prog[2]));

    rt_check(rtMaterialCreate(s.context, &s.material));
    rt_check(rtMaterialSetClosestHitProgram(s.material, 0, prog[3]));
    s.attr_prog = prog[4];

    rt_check(rtAccelerationCreate(s.context, &s.accel));
    rt_check(rtAccelerationSetBuilder(s.accel, "Trbvh"));

    rt_check(rtGeometryGroupCreate(s.context, &s.group));
    rt_check(rtGeometryGroupSetAcceleration(s.group, s.accel));
    rt_check(rtGeometryGroupSetChildCount(s.group, (uint32_t) m_shapes.size()));

    RTvariable top_object;
    rt_check(rtContextDeclareVariable(s.context, "top_object", &top_object));
    rt_check(rtVariableSetObject(top_object, s.group));

    RTvariable accel;
    rt_check(rtContextDeclareVariable(s.context, "accel", &accel));
    rt_check(rtVariableSetUserData(accel, sizeof(void *), &s.accel));

    uint32_t shape_index = 0;
    for (Shape *shape : m_shapes) {
        RTgeometrytriangles tri = shape->optix_geometry(s.context);
        rt_check(rtGeometryTrianglesSetAttributeProgram(tri, s.attr_prog));

        RTgeometryinstance tri_inst;
        rt_check(rtGeometryInstanceCreate(s.context, &tri_inst));
        rt_check(rtGeometryInstanceSetGeometryTriangles(tri_inst, tri));

        rt_check(rtGeometryInstanceSetMaterialCount(tri_inst, 1));
        rt_check(rtGeometryInstanceSetMaterial(tri_inst, 0, s.material));
        rt_check(rtGeometryGroupSetChild(s.group, shape_index, tri_inst));

        RTvariable shape_ptr_var;
        rt_check(rtGeometryInstanceDeclareVariable(tri_inst, "shape_ptr", &shape_ptr_var));
        rt_check(rtVariableSet1ull(shape_ptr_var, (uintptr_t) shape));

        shape_index++;
    }

    // This will trigger the scatter calls to upload geometry to the device
    cuda_eval();

    Log(Info, "Validating and building scene in OptiX.");
    rt_check(rtContextValidate(s.context));
    RTresult rt = rtContextLaunch1D(s.context, 0, 0);
    if (rt == RT_ERROR_MEMORY_ALLOCATION_FAILED) {
        cuda_malloc_trim();
        rt = rtContextLaunch1D(s.context, 0, 0);
    }
    rt_check(rt);
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_release_gpu() {
    OptixState &s = *(OptixState *) m_accel;
    rt_check(rtContextDestroy(s.context));
    delete (OptixState *) m_accel;
    m_accel = nullptr;
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_gpu(const Ray3f &ray_, Mask active) const {
    if constexpr (is_cuda_array_v<Float>) {
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
            si.t    = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.time = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.p.x() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.p.y() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.p.z() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.uv.x() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.uv.y() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.n.x() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.n.y() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.n.z() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.sh_frame.n.x() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.sh_frame.n.y() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.sh_frame.n.z() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.dp_du.x() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.dp_du.y() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.dp_du.z() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.dp_dv.x() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.dp_dv.y() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
            si.dp_dv.z() = full<Float>(std::numeric_limits<scalar_t<Float>>::quiet_NaN(), ray_count);
#endif  // !defined(NDEBUG)

        cuda_eval();

        const void *cuda_ptr[kOptixVariableCount] = {
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
            // Out: Intersection position
            si.p.x().data(), si.p.y().data(), si.p.z().data(),
            // Out: UV coordinates
            si.uv.x().data(), si.uv.y().data(),
            // Out: Geometric normal
            si.n.x().data(), si.n.y().data(), si.n.z().data(),
            // Out: Shading normal
            si.sh_frame.n.x().data(), si.sh_frame.n.y().data(), si.sh_frame.n.z().data(),
            // Out: Texture space derivative (U)
            si.dp_du.x().data(), si.dp_du.y().data(), si.dp_du.z().data(),
            // Ovt: Texture space derivative (V)
            si.dp_dv.x().data(), si.dp_dv.y().data(), si.dp_dv.z().data(),
            // Out: Shape pointer (on host)
            si.shape.data(),
            // Out: Primitive index
            si.prim_index.data(),
            // Out: Hit flag
            nullptr
        };

        OptixState &s = *(OptixState *) m_accel;
        for (size_t i = 0; i < kOptixVariableCount; ++i) {
            if (cuda_ptr[i]) {
                rt_check(rtBufferSetSize1D(s.var_buf[i], ray_count));
                rt_check(rtBufferSetDevicePointer(s.var_buf[i], kDeviceID, (void *) cuda_ptr[i]));
            } else {
                rt_check(rtBufferSetSize1D(s.var_buf[i], 0));
                rt_check(rtBufferSetDevicePointer(s.var_buf[i], kDeviceID, (void *) 8));
            }
        }

        RTresult rt = rtContextLaunch1D(s.context, 0, ray_count);
        if (rt == RT_ERROR_MEMORY_ALLOCATION_FAILED) {
            cuda_malloc_trim();
            rt = rtContextLaunch1D(s.context, 0, ray_count);
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
        Ray3f ray(ray_);
        size_t ray_count = std::max(slices(ray.o), slices(ray.d));
        Mask hit = empty<Mask>(ray_count);

        set_slices(ray, ray_count);
        set_slices(active, ray_count);

        cuda_eval();

        const void *cuda_ptr[kOptixVariableCount] = {
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
            // Out: Intersection position
            nullptr, nullptr, nullptr,
            // Out: UV coordinates
            nullptr, nullptr,
            // Out: Geometric normal
            nullptr, nullptr, nullptr,
            // Out: Shading normal
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
            hit.data()
        };

        OptixState &s = *(OptixState *) m_accel;
        for (size_t i = 0; i < kOptixVariableCount; ++i) {
            if (cuda_ptr[i]) {
                rt_check(rtBufferSetSize1D(s.var_buf[i], ray_count));
                rt_check(rtBufferSetDevicePointer(s.var_buf[i], kDeviceID,
                                                (void *) cuda_ptr[i]));
            } else {
                rt_check(rtBufferSetSize1D(s.var_buf[i], 0));
                rt_check(
                    rtBufferSetDevicePointer(s.var_buf[i], kDeviceID, (void *) 8));
            }
        }

        RTresult rt = rtContextLaunch1D(s.context, 1, ray_count);
        if (rt == RT_ERROR_MEMORY_ALLOCATION_FAILED) {
            cuda_malloc_trim();
            rt = rtContextLaunch1D(s.context, 1, ray_count);
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
