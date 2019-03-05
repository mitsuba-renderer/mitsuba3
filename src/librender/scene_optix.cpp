#include <mitsuba/render/scene.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/autodiff.h>

#include <optix.h>
#include <optix_cuda_interop.h>

#include "librender_ptx.h"

NAMESPACE_BEGIN(mitsuba)

#define rt_check(err)  __rt_check (s.context, err, __FILE__, __LINE__)

static void __rt_check(RTcontext context, RTresult errval, const char *file,
                       const int line) {
    if (errval != RT_SUCCESS) {
        const char *message;
        rtContextGetErrorString(context, errval, &message);
        fprintf(stderr,
                "rt_check(): OptiX API error = %04d (%s) in "
                "%s:%i.\n", (int) errval, message, file, line);
        exit(EXIT_FAILURE);
    }
}

constexpr size_t kOptixVariableCount = 30;
constexpr int kDeviceID = 0;

struct Scene::OptixState {
    RTcontext context;
    RTbuffer var_buf[kOptixVariableCount];
    RTmaterial material;
    RTprogram attr_prog;
    RTgeometrygroup group;
    uint32_t shape_index = 0, n_shapes = 0;
};

void Scene::optix_init(uint32_t n_shapes) {
    m_optix_state = new Scene::OptixState();
    auto &s = *m_optix_state;

    rt_check(rtContextCreate(&s.context));

    rt_check(rtContextSetRayTypeCount(s.context, 1));
    rt_check(rtContextSetEntryPointCount(s.context, 2));
    rt_check(rtContextSetStackSize(s.context, 0));
    rt_check(rtContextSetMaxTraceDepth(s.context, 0));
    rt_check(rtContextSetMaxCallableProgramDepth(s.context, 0));

    const char *var_names[kOptixVariableCount] = {
        "in_mask",         "in_ox",       "in_oy",       "in_oz",
        "in_dx",           "in_dy",       "in_dz",       "in_mint",
        "in_maxt",         "out_t",       "out_p_x",     "out_p_y",
        "out_p_z",         "out_u",       "out_v",       "out_ng_x",
        "out_ng_y",        "out_ng_z",    "out_ns_x",    "out_ns_y",
        "out_ns_z",        "out_dp_du_x", "out_dp_du_y", "out_dp_du_z",
        "out_dp_dv_x",     "out_dp_dv_y", "out_dp_dv_z", "out_shape_ptr",
        "out_primitive_id","out_hit"
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
        rt_check(rtBufferCreate(s.context, RT_BUFFER_INPUT, &s.var_buf[i]));
        rt_check(rtBufferSetFormat(s.var_buf[i], fmt));
        rt_check(rtBufferSetSize1D(s.var_buf[i], 0));
        rt_check(rtBufferSetDevicePointer(s.var_buf[i], kDeviceID, (void *) 8));
        rt_check(rtContextDeclareVariable(s.context, var_names[i], &var_obj[i]));
        rt_check(rtVariableSetObject(var_obj[i], s.var_buf[i]));
    }

    RTprogram prog[6];
    rt_check(rtProgramCreateFromPTXString(s.context, (const char *) optix_rt_ptx, "ray_gen_closest", &prog[0]));
    rt_check(rtProgramCreateFromPTXString(s.context, (const char *) optix_rt_ptx, "ray_gen_any", &prog[1]));
    rt_check(rtProgramCreateFromPTXString(s.context, (const char *) optix_rt_ptx, "ray_miss", &prog[2]));
    rt_check(rtProgramCreateFromPTXString(s.context, (const char *) optix_rt_ptx, "ray_hit", &prog[3]));
    rt_check(rtProgramCreateFromPTXString(s.context, (const char *) optix_attr_ptx, "ray_attr", &prog[4]));

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

    RTacceleration accel;
    rt_check(rtAccelerationCreate(s.context, &accel));
    rt_check(rtAccelerationSetBuilder(accel, "Trbvh"));

    rt_check(rtGeometryGroupCreate(s.context, &s.group));
    rt_check(rtGeometryGroupSetAcceleration(s.group, accel));
    rt_check(rtGeometryGroupSetChildCount(s.group, n_shapes));

    RTvariable top_object;
    rt_check(rtContextDeclareVariable(s.context, "top_object", &top_object));
    rt_check(rtVariableSetObject(top_object, s.group));
    s.n_shapes = n_shapes;
}

void Scene::optix_release() {
    auto &s = *m_optix_state;
    rt_check(rtContextDestroy(s.context));
    delete m_optix_state;
    m_optix_state = nullptr;
}

void Scene::optix_register(Shape *shape) {
    auto &s = *m_optix_state;
    RTgeometrytriangles tri = shape->optix_geometry(s.context);
    rt_check(rtGeometryTrianglesSetAttributeProgram(tri, s.attr_prog));

    RTgeometryinstance tri_inst;
    rt_check(rtGeometryInstanceCreate(s.context, &tri_inst));
    rt_check(rtGeometryInstanceSetGeometryTriangles(tri_inst, tri));

    RTvariable shape_ptr_var;
    rt_check(rtGeometryInstanceSetMaterialCount(tri_inst, 1));
    rt_check(rtGeometryInstanceSetMaterial(tri_inst, 0, s.material));
    rt_check(rtGeometryGroupSetChild(s.group, s.shape_index, tri_inst));

    rt_check(rtGeometryInstanceDeclareVariable(tri_inst, "shape_ptr", &shape_ptr_var));
    rt_check(rtVariableSet1ull(shape_ptr_var, (uintptr_t) shape));
    s.shape_index++;
}

void Scene::optix_build() {
    auto &s = *m_optix_state;
    Log(EInfo, "Validating and building scene in OptiX.");
    Assert(s.shape_index == s.n_shapes);
    rt_check(rtContextValidate(s.context));
    rt_check(rtContextLaunch1D(s.context, 0, 0));
}

SurfaceInteraction3fD Scene::ray_intersect(const Ray3fD &ray_, MaskD active) const {
    Ray3fD ray(ray_);
    size_t ray_count = std::max(slices(ray.o), slices(ray.d));
    set_slices(ray, ray_count);
    set_slices(active, ray_count);
    SurfaceInteraction3fD si = empty<SurfaceInteraction3fD>(ray_count);

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
        // Ovt: Textvre space derivative (V)
        si.dp_dv.x().data(), si.dp_dv.y().data(), si.dp_dv.z().data(),
        // Out: Shape pointer (on host)
        si.shape.data(),
        // Out: Primitive index
        si.prim_index.data(),
        // Out: Hit flag
        nullptr
    };

    auto &s = *m_optix_state;
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

    rt_check(rtContextLaunch1D(s.context, 0, ray_count));

    si.time = ray.time;
    si.wavelengths = ray.wavelengths;
    si.instance = nullptr;

    // Gram-schmidt orthogonalization to compute local shading frame
    si.sh_frame.s = normalize(
        fnmadd(si.sh_frame.n, dot(si.sh_frame.n, si.dp_du), si.dp_du));
    si.sh_frame.t = cross(si.sh_frame.n, si.sh_frame.s);

    // Incident direction in local coordinates
    si.wi = select(si.is_valid(), si.to_local(-ray.d), -ray.d);

    return si;
}

MaskD Scene::ray_test(const Ray3fD &ray_, MaskD active) const {
    Ray3fD ray(ray_);
    size_t ray_count = std::max(slices(ray.o), slices(ray.d));
    MaskD hit = empty<MaskD>(ray_count);

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
        // Ovt: Textvre space derivative (V)
        nullptr, nullptr, nullptr,
        // Out: Shape pointer (on host)
        nullptr,
        // Out: Primitive index
        nullptr,
        // Out: Hit flag
        hit.data()
    };

    auto &s = *m_optix_state;
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

    rt_check(rtContextLaunch1D(s.context, 1, ray_count));

    return hit;
}

NAMESPACE_END(msiuba)
