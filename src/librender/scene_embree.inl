#include <embree3/rtcore.h>

NAMESPACE_BEGIN(mitsuba)

#if defined(ENOKI_X86_AVX512)
#  define MTS_RAY_WIDTH 16
#elif defined(ENOKI_X86_AVX2)
#  define MTS_RAY_WIDTH 8
#elif defined(ENOKI_X86_SSE42)
#  define MTS_RAY_WIDTH 4
#else
#  error Expected to use vectorization
#endif

#define JOIN(x, y)        JOIN_AGAIN(x, y)
#define JOIN_AGAIN(x, y)  x ## y
#define RTCRayHitW        JOIN(RTCRayHit,    MTS_RAY_WIDTH)
#define RTCRayW           JOIN(RTCRay,       MTS_RAY_WIDTH)
#define rtcIntersectW     JOIN(rtcIntersect, MTS_RAY_WIDTH)
#define rtcOccludedW      JOIN(rtcOccluded,  MTS_RAY_WIDTH)

#define PARALLEL_LLVM_RAY_INTERSECT 1

static RTCDevice __embree_device = nullptr;

template <typename Float>
struct EmbreeState {
    MTS_IMPORT_CORE_TYPES()
    RTCScene accel;
    DynamicBuffer<UInt32> shapes_registry_ids;
};

MTS_VARIANT void Scene<Float, Spectrum>::accel_init_cpu(const Properties &/*props*/) {
    static_assert(std::is_same_v<ek::scalar_t<Float>, float>, "Embree is not supported in double precision mode.");
    if (!__embree_device)
        __embree_device = rtcNewDevice("");

    Timer timer;
    RTCScene embree_scene = rtcNewScene(__embree_device);
    rtcSetSceneBuildQuality(embree_scene, RTC_BUILD_QUALITY_HIGH);
    rtcSetSceneFlags(embree_scene, RTC_SCENE_FLAG_NONE);

    m_accel = new EmbreeState<Float>();
    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;
    s.accel = embree_scene;

    ScopedPhase phase(ProfilerPhase::InitAccel);
    for (Shape *shape : m_shapes)
         rtcAttachGeometry(embree_scene, shape->embree_geometry(__embree_device));

    if constexpr (ek::is_llvm_array_v<Float>) {
        // Get shapes registry ids
        std::unique_ptr<uint32_t[]> data(new uint32_t[m_shapes.size()]);
        for (size_t i = 0; i < m_shapes.size(); i++)
            data[i] = jit_registry_get_id(m_shapes[i]);
        s.shapes_registry_ids
            = ek::load<DynamicBuffer<UInt32>>(data.get(), m_shapes.size());
    }

    rtcCommitScene(embree_scene);
    Log(Info, "Embree ready. (took %s)",
        util::time_string((float) timer.value()));
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_release_cpu() {
    EmbreeState<Float> *s = (EmbreeState<Float> *) m_accel;
    rtcReleaseScene((RTCScene) s->accel);
    delete s;
    m_accel = nullptr;
}

MTS_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_cpu(const Ray3f &ray, uint32_t hit_flags, Mask active) const {
    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;

    RTCIntersectContext context;
    rtcInitIntersectContext(&context);
    if (hit_flags & (uint32_t) HitComputeFlags::Coherent)
        context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;

    if constexpr (!ek::is_jit_array_v<Float>) {
        PreliminaryIntersection3f pi = ek::zero<PreliminaryIntersection3f>();

        RTCRayHit rh = {};
        rh.ray.org_x = ray.o.x();
        rh.ray.org_y = ray.o.y();
        rh.ray.org_z = ray.o.z();
        rh.ray.dir_x = ray.d.x();
        rh.ray.dir_y = ray.d.y();
        rh.ray.dir_z = ray.d.z();
        rh.ray.tnear = ray.mint;
        rh.ray.tfar  = ray.maxt;
        rh.ray.time  = ray.time;

        rtcIntersect1(s.accel, &context, &rh);

        if (rh.ray.tfar != ray.maxt) {
            uint32_t shape_index = rh.hit.geomID;
            uint32_t prim_index  = rh.hit.primID;

            // We get level 0 because we only support one level of instancing
            uint32_t inst_index = rh.hit.instID[0];

            // If the hit is not on an instance
            bool hit_instance = (inst_index != RTC_INVALID_GEOMETRY_ID);
            uint32_t index = hit_instance ? inst_index : shape_index;

            ShapePtr shape;
            if constexpr (!ek::is_array_v<Float>)
                shape = m_shapes[index];
            else
                shape = ek::gather<UInt32>(s.shapes_registry_ids, index);

            if (hit_instance)
                pi.instance = shape;
            else
                pi.shape = shape;

            pi.shape_index = shape_index;

            pi.t = rh.ray.tfar;
            pi.prim_index = prim_index;
            pi.prim_uv = Point2f(rh.hit.u, rh.hit.v);
        }
        return pi;
    } else if constexpr (ek::is_llvm_array_v<Float>) {
        if (jit_llvm_vector_width() != MTS_RAY_WIDTH)
            Throw("ray_intersect_preliminary_cpu(): LLVM backend and "
                  "Mitsuba/Embree don't have matching vector widths!");

        void *func_ptr  = (void *) rtcIntersectW,
             *ctx_ptr   = (void *) &context,
             *scene_ptr = (void *) s.accel;

        UInt64 func_v = UInt64::steal(
                   jit_var_new_pointer(JitBackend::LLVM, func_ptr, 0, 0)),
               ctx_v = UInt64::steal(
                   jit_var_new_pointer(JitBackend::LLVM, ctx_ptr, 0, 0)),
               scene_v = UInt64::steal(
                   jit_var_new_pointer(JitBackend::LLVM, scene_ptr, 0, 0));

        Int32 valid = ek::select(active, (int32_t) -1, 0);
        UInt32 zero = ek::zero<UInt32>();

        uint32_t in[13] = { valid.index(),      ray.o.x().index(),
                            ray.o.y().index(),  ray.o.z().index(),
                            ray.mint.index(),   ray.d.x().index(),
                            ray.d.y().index(),  ray.d.z().index(),
                            ray.time.index(),   ray.maxt.index(),
                            zero.index(),       zero.index(),
                            zero.index() };

        uint32_t out[6] { };

        jit_embree_trace(func_v.index(), ctx_v.index(), scene_v.index(),
                         0, in, out);

        PreliminaryIntersection3f pi;

        Float t(Float::steal(out[0]));
        pi.prim_uv = Vector2f(Float::steal(out[1]),
                              Float::steal(out[2]));

        pi.prim_index = UInt32::steal(out[3]);
        pi.shape_index = UInt32::steal(out[4]);

        UInt32 inst_index = UInt32::steal(out[5]);

        Mask hit = active && ek::neq(t, ray.maxt);

        pi.t = select(hit, t, ek::Infinity<Float>);

        // Set si.instance and si.shape
        Mask hit_inst = hit && ek::neq(inst_index, RTC_INVALID_GEOMETRY_ID);
        UInt32 index = ek::select(hit_inst, inst_index, pi.shape_index);

        ShapePtr shape = ek::gather<UInt32>(s.shapes_registry_ids, index, hit);
        pi.instance = ek::select(hit_inst, shape, nullptr);
        pi.shape    = ek::select(hit_inst, nullptr, shape);
        return pi;
    } else {
        Throw("ray_intersect_preliminary_cpu() should only be called in CPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_cpu(const Ray3f &ray, uint32_t hit_flags, Mask active) const {
    if constexpr (!ek::is_cuda_array_v<Float>) {
        PreliminaryIntersection3f pi = ray_intersect_preliminary_cpu(ray, hit_flags, active);
        return pi.compute_surface_interaction(ray, hit_flags, active);
    } else {
        Throw("ray_intersect_cpu() should only be called in CPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_cpu(const Ray3f &ray, uint32_t hit_flags,
                                     Mask active) const {
    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;

    RTCIntersectContext context;
    rtcInitIntersectContext(&context);
    if (hit_flags & (uint32_t) HitComputeFlags::Coherent)
        context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;

    if constexpr (!ek::is_jit_array_v<Float>) {
        EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;

        ENOKI_MARK_USED(active);

        RTCRay ray2 = {};
        ray2.org_x = ray.o.x();
        ray2.org_y = ray.o.y();
        ray2.org_z = ray.o.z();
        ray2.dir_x = ray.d.x();
        ray2.dir_y = ray.d.y();
        ray2.dir_z = ray.d.z();
        ray2.tnear = ray.mint;
        ray2.tfar  = ray.maxt;
        ray2.time  = ray.time;

        rtcOccluded1(s.accel, &context, &ray2);

        return ray2.tfar != ray.maxt;
    } else if constexpr (ek::is_llvm_array_v<Float>) {
        if (jit_llvm_vector_width() != MTS_RAY_WIDTH)
            Throw("ray_test_cpu(): LLVM backend and "
                  "Mitsuba/Embree don't have matching vector widths!");

        void *func_ptr  = (void *) rtcOccludedW,
             *ctx_ptr   = (void *) &context,
             *scene_ptr = (void *) s.accel;

        UInt64 func_v = UInt64::steal(
                   jit_var_new_pointer(JitBackend::LLVM, func_ptr, 0, 0)),
               ctx_v = UInt64::steal(
                   jit_var_new_pointer(JitBackend::LLVM, ctx_ptr, 0, 0)),
               scene_v = UInt64::steal(
                   jit_var_new_pointer(JitBackend::LLVM, scene_ptr, 0, 0));

        Int32 valid = ek::select(active, (int32_t) -1, 0);
        UInt32 zero = ek::zero<UInt32>();

        uint32_t in[13] = { valid.index(),      ray.o.x().index(),
                            ray.o.y().index(),  ray.o.z().index(),
                            ray.mint.index(),   ray.d.x().index(),
                            ray.d.y().index(),  ray.d.z().index(),
                            ray.time.index(),   ray.maxt.index(),
                            zero.index(),       zero.index(),
                            zero.index() };

        uint32_t out[1] { };

        jit_embree_trace(func_v.index(), ctx_v.index(), scene_v.index(),
                         1, in, out);

        return active && ek::neq(Float::steal(out[0]), ray.maxt);
    } else {
        Throw("ray_test_cpu() should only be called in CPU mode.");
    }
}

NAMESPACE_END(mitsuba)
