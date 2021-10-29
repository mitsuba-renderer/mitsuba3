#include <embree3/rtcore.h>
#include <enoki-thread/thread.h>

NAMESPACE_BEGIN(mitsuba)
#if defined(ENOKI_DISABLE_VECTORIZATION)
#  define MTS_RAY_WIDTH 1
#elif defined(__AVX512F__)
#  define MTS_RAY_WIDTH 16
#elif defined(__AVX2__)
#  define MTS_RAY_WIDTH 8
#elif defined(__SSE4_2__) || defined(__ARM_NEON)
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

static uint32_t embree_threads = 0;
static RTCDevice embree_device = nullptr;

template <typename Float>
struct EmbreeState {
    MTS_IMPORT_CORE_TYPES()
    RTCScene accel;
    std::vector<int> geometries;
    DynamicBuffer<UInt32> shapes_registry_ids;
};

static void embree_error_callback(void * /*user_ptr */, RTCError code, const char *str) {
    Log(Warn, "Embree device error %i: %s.", (int) code, str);
}

MTS_VARIANT void
Scene<Float, Spectrum>::accel_init_cpu(const Properties & /*props*/) {
    if (!embree_device) {
        embree_threads = std::max((uint32_t) 1, pool_size());
        std::string config_str = tfm::format(
            "threads=%i,user_threads=%i", embree_threads, embree_threads);
        embree_device = rtcNewDevice(config_str.c_str());
        rtcSetDeviceErrorFunction(embree_device, embree_error_callback, nullptr);
    }

    Timer timer;

    m_accel = new EmbreeState<Float>();
    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;

    s.accel = rtcNewScene(embree_device);
    rtcSetSceneBuildQuality(s.accel, RTC_BUILD_QUALITY_HIGH);
    rtcSetSceneFlags(s.accel, RTC_SCENE_FLAG_NONE);

    ScopedPhase phase(ProfilerPhase::InitAccel);
    accel_parameters_changed_cpu();

    Log(Info, "Embree ready. (took %s)",
        util::time_string((float) timer.value()));

    if constexpr (ek::is_llvm_array_v<Float>) {
        // Get shapes registry ids
        if (!m_shapes.empty()) {
            std::unique_ptr<uint32_t[]> data(new uint32_t[m_shapes.size()]);
            for (size_t i = 0; i < m_shapes.size(); i++)
                data[i] = jit_registry_get_id(JitBackend::LLVM, m_shapes[i]);
            s.shapes_registry_ids
                = ek::load<DynamicBuffer<UInt32>>(data.get(), m_shapes.size());
        } else {
            s.shapes_registry_ids = ek::zero<DynamicBuffer<UInt32>>();
        }
    }
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_parameters_changed_cpu() {
    if constexpr (ek::is_llvm_array_v<Float>)
        ek::sync_thread();

    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;
    for (int geo : s.geometries)
        rtcDetachGeometry(s.accel, geo);
    s.geometries.clear();

    for (Shape *shape : m_shapes) {
        RTCGeometry geom = shape->embree_geometry(embree_device);
        s.geometries.push_back(rtcAttachGeometry(s.accel, geom));
        rtcReleaseGeometry(geom);
    }

    // Ensure shape data pointers are fully evaluated before building the BVH
    if constexpr (ek::is_llvm_array_v<Float>)
        ek::sync_thread();

    ek::parallel_for(
        ek::blocked_range<size_t>(0, embree_threads, 1),
        [&](const ek::blocked_range<size_t> &) {
            rtcJoinCommitScene(s.accel);
        }
    );

    /* Set up a callback on the handle variable to release the Embree
       acceleration data structure (IAS) when this variable is freed. This
       ensures that the lifetime of the IAS goes beyond the one of the Scene
       instance if there are still some pending ray tracing calls (e.g. non
       evaluated variables depending on a ray tracing call). */
    if constexpr (ek::is_llvm_array_v<Float>) {
        // Prevents the IAS to be released when updating the scene parameters
        if (m_accel_handle.index())
            jit_var_set_callback(m_accel_handle.index(), nullptr, nullptr);
        m_accel_handle = ek::opaque<UInt64>(s.accel);
        jit_var_set_callback(
            m_accel_handle.index(),
            [](uint32_t /* index */, int free, void *payload) {
                EmbreeState<Float> *s = (EmbreeState<Float> *) payload;
                if (free) {
                    rtcReleaseScene(s->accel);
                    delete s;
                }
            },
            (void *) m_accel
        );
    }
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_release_cpu() {
    if constexpr (ek::is_llvm_array_v<Float>) {
        // Ensure all ray tracing kernels are terminated before releasing the scene
        ek::sync_thread();

        uint32_t handle_index = m_accel_handle.index();

        /* Decrease the reference count of the handle variable. This will
           trigger the release of the Embree acceleration data structure if no
           ray tracing calls are pending. */
        m_accel_handle = 0;

        bool scene_contains_others = false;
        for (auto& shape : m_shapes)
            scene_contains_others |= !shape->is_mesh();
        if (scene_contains_others && jit_var_exists(handle_index) && jit_var_ref_ext(handle_index) != 0) {
            Throw("accel_release_cpu(): Scene deletion with pending ray tracing "
                  "calls is only supported with Embree if the scene contains any "
                  "non mesh shapes (e.g. instances, sphere, ...)!");
        }
    }

    m_accel = nullptr;
}

template <bool ShadowRay, bool Coherent>
void embree_func_wrapper(const int* valid, void* ptr, uint8_t* args) {
    RTCIntersectContext context;
    rtcInitIntersectContext(&context);
    if (Coherent)
        context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;

    if constexpr (ShadowRay)
        rtcOccludedW(valid, (RTCScene) ptr, &context, (RTCRayW*) args);
    else
        rtcIntersectW(valid, (RTCScene) ptr, &context, (RTCRayHitW*) args);
}

MTS_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_cpu(const Ray3f &ray,
                                                      uint32_t hit_flags,
                                                      Mask active) const {
    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;

    // Embree doesn't support double precision maxt
    Float32 ray_maxt = ek::min((Float32) ray.maxt, ek::Largest<float>);

    if constexpr (!ek::is_jit_array_v<Float>) {
        ENOKI_MARK_USED(active);

        RTCIntersectContext context;
        rtcInitIntersectContext(&context);
        if (hit_flags & (uint32_t) HitComputeFlags::Coherent)
            context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;

        PreliminaryIntersection3f pi = ek::zero<PreliminaryIntersection3f>();

        RTCRayHit rh = {};
        rh.ray.org_x = (float) ray.o.x();
        rh.ray.org_y = (float) ray.o.y();
        rh.ray.org_z = (float) ray.o.z();
        rh.ray.dir_x = (float) ray.d.x();
        rh.ray.dir_y = (float) ray.d.y();
        rh.ray.dir_z = (float) ray.d.z();
        rh.ray.tnear = (float) 0.f;
        rh.ray.tfar  = ray_maxt;
        rh.ray.time  = (float) ray.time;

        rtcIntersect1(s.accel, &context, &rh);

        if (rh.ray.tfar != ray_maxt) {
            uint32_t shape_index = rh.hit.geomID;
            uint32_t prim_index  = rh.hit.primID;

            // We get level 0 because we only support one level of instancing
            uint32_t inst_index = rh.hit.instID[0];

            // If the hit is not on an instance
            bool hit_instance = (inst_index != RTC_INVALID_GEOMETRY_ID);
            uint32_t index = hit_instance ? inst_index : shape_index;

            ShapePtr shape = m_shapes[index];
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
        uint32_t jit_width = jit_llvm_vector_width();
        if (unlikely(jit_width != MTS_RAY_WIDTH))
            Throw("ray_intersect_preliminary_cpu(): LLVM backend and "
                  "Mitsuba/Embree don't have matching vector widths! "
                  "(Embree: %u vs LLVM: %u)",
                  MTS_RAY_WIDTH, jit_width);

        void *scene_ptr = (void *) s.accel,
             *func_ptr = nullptr;

        if (hit_flags & (uint32_t) HitComputeFlags::Coherent)
            func_ptr = (void *) embree_func_wrapper<false, true>;
        else
            func_ptr = (void *) embree_func_wrapper<false, false>;

        UInt64 func_v  = UInt64::steal(jit_var_new_pointer(
                   JitBackend::LLVM, func_ptr, m_accel_handle.index(), 0)),
               scene_v = UInt64::steal(
                   jit_var_new_pointer(JitBackend::LLVM, scene_ptr, 0, 0));

        Int32 valid = ek::select(active, (int32_t) -1, 0);
        UInt32 zero = ek::zero<UInt32>();

        using Single = ek::float32_array_t<Float>;
        ek::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_mint(0.f), ray_maxt_(ray_maxt), ray_time(ray.time);

        uint32_t in[13] = { valid.index(),      ray_o.x().index(),
                            ray_o.y().index(),  ray_o.z().index(),
                            ray_mint.index(),   ray_d.x().index(),
                            ray_d.y().index(),  ray_d.z().index(),
                            ray_time.index(),   ray_maxt_.index(),
                            zero.index(),       zero.index(),
                            zero.index() };

        uint32_t out[6] { };

        jit_llvm_ray_trace(func_v.index(), scene_v.index(), 0, in, out);

        PreliminaryIntersection3f pi;

        Float t(Single::steal(out[0]));
        pi.prim_uv = Vector2f(Single::steal(out[1]),
                              Single::steal(out[2]));

        pi.prim_index  = UInt32::steal(out[3]);
        pi.shape_index = UInt32::steal(out[4]);

        UInt32 inst_index = UInt32::steal(out[5]);

        Mask hit = active && ek::neq(t, ray_maxt_);

        pi.t = ek::select(hit, t, ek::Infinity<Float>);

        // Set si.instance and si.shape
        Mask hit_inst = hit && ek::neq(inst_index, RTC_INVALID_GEOMETRY_ID);
        UInt32 index = ek::select(hit_inst, inst_index, pi.shape_index);

        ShapePtr shape = ek::gather<UInt32>(s.shapes_registry_ids, index, hit);

        pi.instance = shape & hit_inst;
        pi.shape    = shape & !hit_inst;

        return pi;
    } else {
        ENOKI_MARK_USED(ray);
        ENOKI_MARK_USED(hit_flags);
        ENOKI_MARK_USED(active);
        Throw("ray_intersect_preliminary_cpu() should only be called in CPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_cpu(const Ray3f &ray, uint32_t hit_flags, Mask active) const {
    if constexpr (!ek::is_cuda_array_v<Float>) {
        PreliminaryIntersection3f pi = ray_intersect_preliminary_cpu(ray, hit_flags, active);
        return pi.compute_surface_interaction(ray, hit_flags, active);
    } else {
        ENOKI_MARK_USED(ray);
        ENOKI_MARK_USED(hit_flags);
        ENOKI_MARK_USED(active);
        Throw("ray_intersect_cpu() should only be called in CPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_cpu(const Ray3f &ray, uint32_t hit_flags,
                                     Mask active) const {
    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;

    // Embree doesn't support double precision maxt
    Float32 ray_maxt = ek::min((Float32) ray.maxt, ek::Largest<float>);

    if constexpr (!ek::is_jit_array_v<Float>) {
        ENOKI_MARK_USED(active);

        RTCIntersectContext context;
        rtcInitIntersectContext(&context);
        if (hit_flags & (uint32_t) HitComputeFlags::Coherent)
            context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;

        RTCRay ray2 = {};
        ray2.org_x = (float) ray.o.x();
        ray2.org_y = (float) ray.o.y();
        ray2.org_z = (float) ray.o.z();
        ray2.dir_x = (float) ray.d.x();
        ray2.dir_y = (float) ray.d.y();
        ray2.dir_z = (float) ray.d.z();
        ray2.tnear = (float) 0.f;
        ray2.tfar  = ray_maxt;
        ray2.time  = (float) ray.time;

        rtcOccluded1(s.accel, &context, &ray2);

        return ray2.tfar != ray_maxt;
    } else if constexpr (ek::is_llvm_array_v<Float>) {
        uint32_t jit_width = jit_llvm_vector_width();
        if (unlikely(jit_width != MTS_RAY_WIDTH))
            Throw("ray_test_cpu(): LLVM backend and Mitsuba/Embree don't "
                  "have matching vector widths! (Embree: %u vs LLVM: %u)",
                  MTS_RAY_WIDTH, jit_width);

        void *scene_ptr = (void *) s.accel,
             *func_ptr  = nullptr;

        if (hit_flags & (uint32_t) HitComputeFlags::Coherent)
            func_ptr = (void *) embree_func_wrapper<true, true>;
        else
            func_ptr = (void *) embree_func_wrapper<true, false>;

        UInt64 func_v  = UInt64::steal(jit_var_new_pointer(
                   JitBackend::LLVM, func_ptr, m_accel_handle.index(), 0)),
               scene_v = UInt64::steal(
                   jit_var_new_pointer(JitBackend::LLVM, scene_ptr, 0, 0));

        Int32 valid = ek::select(active, (int32_t) -1, 0);
        UInt32 zero = ek::zero<UInt32>();

        // Conversion, in case this is a double precision build
        using Single = ek::float32_array_t<Float>;
        ek::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_mint(0.f), ray_maxt_(ray_maxt), ray_time(ray.time);

        uint32_t in[13] = { valid.index(),      ray_o.x().index(),
                            ray_o.y().index(),  ray_o.z().index(),
                            ray_mint.index(),   ray_d.x().index(),
                            ray_d.y().index(),  ray_d.z().index(),
                            ray_time.index(),   ray_maxt_.index(),
                            zero.index(),       zero.index(),
                            zero.index() };

        uint32_t out[1] { };

        jit_llvm_ray_trace(func_v.index(), scene_v.index(), 1, in, out);

        return active && ek::neq(Single::steal(out[0]), ray_maxt_);
    } else {
        ENOKI_MARK_USED(ray);
        ENOKI_MARK_USED(hit_flags);
        ENOKI_MARK_USED(active);
        Throw("ray_test_cpu() should only be called in CPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_naive_cpu(const Ray3f &ray,
                                                Mask active) const {
    return ray_intersect_cpu(ray, +HitComputeFlags::All, active);
}

NAMESPACE_END(mitsuba)
