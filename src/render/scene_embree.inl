#include <embree3/rtcore.h>
#include <nanothread/nanothread.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/accel_cpu_common.h>
#include <thread>

NAMESPACE_BEGIN(mitsuba)

static_assert(sizeof(RTCIntersectContext) == 24 /* Dr.Jit assumes this */);

static uint32_t embree_threads = 0;
static RTCDevice embree_device = nullptr;

static void embree_error_callback(void * /*user_ptr */, RTCError code, const char *str) {
    Log(Warn, "Embree device error %i: %s.", (int) code, str);
}

/// Wraps rtcOccluded16 when Dr.Jit operates on vectors of length 32
void rtcOccluded32(const int *valid, RTCScene scene,
                   RTCIntersectContext *context, uint32_t *in) {
    constexpr size_t N = 16, M = 2;

    RTC_ALIGN(N * 4) uint32_t tmp[N * 12];

    for (size_t i = 0; i < M; ++i) {
        uint32_t *ptr_in  = in + N * i,
                 *ptr_tmp = tmp;

        for (size_t j = 0; j < 12; ++j) {
            memcpy(ptr_tmp, ptr_in, N * sizeof(uint32_t));
            ptr_in += N * M;
            ptr_tmp += N;
        }

        static_assert(sizeof(tmp) == sizeof(RTCRay16));
        rtcOccluded16(valid + N * i, scene, context, (RTCRay16 *) tmp);

        memcpy(in + N * (i + M * 8), tmp + N * 8, N * sizeof(uint32_t));
    }
}

/// Wraps rtcIntersect16 when Dr.Jit operates on vectors of length 32
void rtcIntersect32(const int *valid, RTCScene scene,
                    RTCIntersectContext *context, uint32_t *in) {
    constexpr size_t N = 16, M = 2;

    RTC_ALIGN(N * 4) uint32_t tmp[N * 20];

    for (size_t i = 0; i < M; ++i) {
        uint32_t *ptr_in  = in + N * i,
                 *ptr_tmp = tmp;

        for (size_t j = 0; j < 20; ++j) {
            memcpy(ptr_tmp, ptr_in, N * sizeof(uint32_t));
            ptr_in += N * M;
            ptr_tmp += N;
        }

        memcpy(tmp + N * 17, in + N * (i + M * 17), N * sizeof(uint32_t));

        static_assert(sizeof(tmp) == sizeof(RTCRayHit16));
        rtcIntersect16(valid + N * i, scene, context, (RTCRayHit16 *) tmp);

        memcpy(in + N * (i + M * 8), tmp + N * 8, N * sizeof(uint32_t));

        ptr_in  = in + N * (i + M * 12);
        ptr_tmp = tmp + N * 12;

        for (int j = 0; j < 8; ++j) {
            memcpy(ptr_in, ptr_tmp, N * sizeof(uint32_t));
            ptr_in += N * 2;
            ptr_tmp += N;
        }
    }
}

// -----------------------------------------------------------------------
//  EmbreeAccel<Float, Spectrum> -- lifecycle
// -----------------------------------------------------------------------

template <typename Float, typename Spectrum>
void EmbreeAccel<Float, Spectrum>::init(Scene<Float, Spectrum> *scene,
                                        const Properties &props) {
    if (!embree_device) {
        // Tricky: Embree allows at most 2*hardware_concurrency() builder
        // threads due to allocation of a thread-local data structure in
        // taskschedulerinternal.h:233
        uint32_t hw_concurrency = (uint32_t) std::thread::hardware_concurrency(),
                 pool_size = (uint32_t) ::pool_size();
        embree_threads = std::max((uint32_t) 1, std::min(pool_size, hw_concurrency*2));

        std::string config_str = tfm::format(
            "threads=%i,user_threads=%i", embree_threads, embree_threads);
        embree_device = rtcNewDevice(config_str.c_str());
        rtcSetDeviceErrorFunction(embree_device, embree_error_callback, nullptr);
    }

    Timer timer;

    // Check if another scene was passed to the constructor
    for (auto &prop : props.objects()) {
        if (prop.try_get<Scene<Float, Spectrum>>()) {
            is_nested_scene = true;
            break;
        }
    }

    accel = rtcNewScene(embree_device);
    rtcSetSceneBuildQuality(accel, RTC_BUILD_QUALITY_HIGH);
    bool use_robust = props.get<bool>("embree_use_robust_intersections", false);
    rtcSetSceneFlags(accel, use_robust ? RTC_SCENE_FLAG_ROBUST : RTC_SCENE_FLAG_NONE);

    ScopedPhase phase(ProfilerPhase::InitAccel);
    parameters_changed(scene);

    Log(Info, "Embree ready. (took %s)",
        util::time_string((float) timer.value()));

    if constexpr (dr::is_llvm_v<Float>)
        shapes_registry_ids = build_registry_ids<Float, Spectrum>(scene->m_shapes);
}

template <typename Float, typename Spectrum>
void EmbreeAccel<Float, Spectrum>::parameters_changed(
    Scene<Float, Spectrum> *scene) {
    if constexpr (dr::is_llvm_v<Float>)
        dr::sync_thread();

    for (int geo : geometries)
        rtcDetachGeometry(accel, geo);
    geometries.clear();

    for (Shape *shape : scene->m_shapes) {
        RTCGeometry geom = shape->embree_geometry(embree_device);
        geometries.push_back(rtcAttachGeometry(accel, geom));
        rtcReleaseGeometry(geom);
    }

    // Ensure shape data pointers are fully evaluated before building the BVH
    if constexpr (dr::is_llvm_v<Float>)
        dr::sync_thread();

    // Avoid getting in a deadlock when building a nested scene while rendering
    if (is_nested_scene) {
        rtcCommitScene(accel);
    } else {
        dr::parallel_for(
            dr::blocked_range<size_t>(0, embree_threads, 1),
            [&](const dr::blocked_range<size_t> &) {
                rtcJoinCommitScene(accel);
            }
        );
    }

    /* Set up a callback on the handle variable to release the Embree
       acceleration data structure (IAS) when this variable is freed. This
       ensures that the lifetime of the IAS goes beyond the one of the Scene
       instance if there are still some pending ray tracing calls (e.g. non
       evaluated variables depending on a ray tracing call). The by-value
       EmbreeAccel itself dies with the Scene, so the callback frees only the
       native RTCScene it captures by pointer. */
    if constexpr (dr::is_llvm_v<Float>) {
        /* Swap the handle to the (rebuilt) Embree scene. The callback frees the
           native RTCScene lazily so it outlives the Scene while ray-tracing
           kernels are still pending (e.g. recorded by a frozen function). */
        reset_mapped_handle(
            accel_handle, (void *) accel,
            [](uint32_t /* index */, int free, void *payload) {
                if (free)
                    jit_enqueue_host_func(
                        JitBackend::LLVM,
                        [](void *p) { rtcReleaseScene((RTCScene) p); },
                        payload);
            },
            (void *) accel);

        // To support frozen functions the func_ptr has to exist as a variable
        // when the scene is traversed.
        // Since the LLVM vector width should not change over the lifetime of
        // the scene, we determine the intersection function here.
        uint32_t jit_width = jit_llvm_vector_width();
        switch (jit_width) {
            case 1:  func_ptr = (void *) rtcIntersect1;  break;
            case 4:  func_ptr = (void *) rtcIntersect4;  break;
            case 8:  func_ptr = (void *) rtcIntersect8;  break;
            case 16: func_ptr = (void *) rtcIntersect16; break;
            case 32: func_ptr = (void *) rtcIntersect32; break;
            default:
                Throw("accel_init_cpu(): Dr.Jit is configured for vectors of "
                      "width %u, which is not supported by Embree!",
                      jit_width);
        }

        func_handle = UInt64::map_(func_ptr, 1, false);
    }

    scene->clear_shapes_dirty();
}

template <typename Float, typename Spectrum>
void EmbreeAccel<Float, Spectrum>::release() {
    if (!accel)
        return; // already released
    if constexpr (dr::is_llvm_v<Float>) {
        // Ensure all ray tracing kernels are terminated before releasing the
        // scene.
        dr::sync_thread();

        /* Drop the reference count of the handle variable. This will trigger
           the deferred release of the Embree scene if no ray tracing calls are
           pending. */
        accel_handle = 0;
    } else {
        // Immediately release Embree structures in scalar mode.
        rtcReleaseScene(accel);
    }
    accel = nullptr;
}

// -----------------------------------------------------------------------
//  EmbreeAccel<Float, Spectrum> -- ray queries
// -----------------------------------------------------------------------

template <typename Float, typename Spectrum>
typename EmbreeAccel<Float, Spectrum>::PreliminaryIntersection3f
EmbreeAccel<Float, Spectrum>::ray_intersect_preliminary(
    const Scene<Float, Spectrum> *scene, const Ray3f &ray, Mask coherent,
    bool /*reorder*/, UInt32 /*reorder_hint*/, uint32_t /*reorder_hint_bits*/,
    Mask active) const {
    using Single = dr::float32_array_t<Float>;
    DRJIT_MARK_USED(scene);

    // Be careful with 'ray.maxt' in double precision variants
    Single ray_maxt = Single(ray.maxt);
    if constexpr (!std::is_same_v<Single, Float>)
        ray_maxt = dr::minimum(ray_maxt, dr::Largest<Single>);

    if constexpr (!dr::is_jit_v<Float>) {
        DRJIT_MARK_USED(active);
        DRJIT_MARK_USED(coherent);

        RTCIntersectContext context;
        rtcInitIntersectContext(&context);

        PreliminaryIntersection3f pi = dr::zeros<PreliminaryIntersection3f>();

        using Vector3s = Vector<Single, 3>;

        RTCRayHit rh;
        dr::store(&rh.ray.org_x, dr::concat(Vector3s(ray.o), float(0.f)));
        dr::store(&rh.ray.dir_x, dr::concat(Vector3s(ray.d), float(ray.time)));
        rh.ray.tfar = ray_maxt;
        rh.ray.mask = 0;
        rh.ray.id = 0;
        rh.ray.flags = 0;
        rh.hit.geomID = (uint32_t) -1;

        rtcIntersect1(accel, &context, &rh);

        if (rh.ray.tfar != ray_maxt) {
            uint32_t shape_index = rh.hit.geomID;
            uint32_t prim_index  = rh.hit.primID;

            // We get level 0 because we only support one level of instancing
            uint32_t inst_index = rh.hit.instID[0];

            // If the hit is not on an instance
            bool hit_instance = inst_index != RTC_INVALID_GEOMETRY_ID;
            uint32_t index = hit_instance ? inst_index : shape_index;

            ShapePtr shape = scene->m_shapes[index];
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
    } else if constexpr (dr::is_llvm_v<Float>) {
        void *scene_ptr = (void *) accel,
             *fptr      = func_ptr;

        UInt64 func_v  = UInt64::steal(jit_var_pointer(
                   JitBackend::LLVM, fptr, func_handle.index(), 0)),
               scene_v = UInt64::steal(jit_var_pointer(
                   JitBackend::LLVM, scene_ptr, accel_handle.index(), 0));

        UInt32 zero = dr::zeros<UInt32>();

        dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_mint(0.f), ray_time(ray.time);

        uint32_t in[14] = { coherent.index(),  active.index(),
                            ray_o.x().index(), ray_o.y().index(),
                            ray_o.z().index(), ray_mint.index(),
                            ray_d.x().index(), ray_d.y().index(),
                            ray_d.z().index(), ray_time.index(),
                            ray_maxt.index(),  zero.index(),
                            zero.index(),      zero.index() };

        uint32_t out[6] { };

        jit_llvm_ray_trace(func_v.index(), scene_v.index(), 0, in, out);

        PreliminaryIntersection3f pi;

        Float t(Single::steal(out[0]));

        pi.prim_uv = Vector2f(Single::steal(out[1]),
                              Single::steal(out[2]));

        pi.prim_index  = UInt32::steal(out[3]);
        pi.shape_index = UInt32::steal(out[4]);

        UInt32 inst_index = UInt32::steal(out[5]);

        Mask hit = active && (t != ray_maxt);

        pi.t = dr::select(hit, t, dr::Infinity<Float>);

        // Set si.instance and si.shape
        Mask hit_inst = hit && (inst_index != RTC_INVALID_GEOMETRY_ID);
        UInt32 index = dr::select(hit_inst, inst_index, pi.shape_index);

        ShapePtr shape = dr::gather<UInt32>(shapes_registry_ids, index, hit);

        pi.instance = shape & hit_inst;
        pi.shape    = shape & !hit_inst;

        return pi;
    } else {
        DRJIT_MARK_USED(ray);
        DRJIT_MARK_USED(coherent);
        DRJIT_MARK_USED(active);
        Throw("ray_intersect_preliminary_cpu() should only be called in CPU mode.");
    }
}

template <typename Float, typename Spectrum>
typename EmbreeAccel<Float, Spectrum>::SurfaceInteraction3f
EmbreeAccel<Float, Spectrum>::ray_intersect(
    const Scene<Float, Spectrum> *scene, const Ray3f &ray, uint32_t ray_flags,
    Mask coherent, bool reorder, UInt32 reorder_hint,
    uint32_t reorder_hint_bits, Mask active) const {
    PreliminaryIntersection3f pi = ray_intersect_preliminary(
        scene, ray, coherent, reorder, reorder_hint, reorder_hint_bits, active);
    return pi.compute_surface_interaction(ray, ray_flags, active);
}

template <typename Float, typename Spectrum>
typename EmbreeAccel<Float, Spectrum>::Mask
EmbreeAccel<Float, Spectrum>::ray_test(const Scene<Float, Spectrum> * /*scene*/,
                                       const Ray3f &ray, Mask coherent,
                                       Mask active) const {
    using Single = dr::float32_array_t<Float>;

    // Be careful with 'ray.maxt' in double precision variants
    Single ray_maxt = Single(ray.maxt);
    if constexpr (!std::is_same_v<Single, Float>)
        ray_maxt = dr::minimum(ray_maxt, dr::Largest<Single>);

    if constexpr (!dr::is_jit_v<Float>) {
        DRJIT_MARK_USED(active);
        DRJIT_MARK_USED(coherent);

        RTCIntersectContext context;
        rtcInitIntersectContext(&context);

        using Vector3s = Vector<Single, 3>;

        RTCRay ray2;
        dr::store(&ray2.org_x, dr::concat(Vector3s(ray.o), float(0.f)));
        dr::store(&ray2.dir_x, dr::concat(Vector3s(ray.d), float(ray.time)));
        ray2.tfar = (float) ray_maxt;
        ray2.mask = 0;
        ray2.id = 0;
        ray2.flags = 0;

        rtcOccluded1(accel, &context, &ray2);

        return ray2.tfar != ray_maxt;
    } else if constexpr (dr::is_llvm_v<Float>) {
        void *scene_ptr = (void *) accel,
             *fptr      = func_ptr;

        UInt64 func_v  = UInt64::steal(jit_var_pointer(
                   JitBackend::LLVM, fptr, func_handle.index(), 0)),
               scene_v = UInt64::steal(jit_var_pointer(
                   JitBackend::LLVM, scene_ptr, accel_handle.index(), 0));

        UInt32 zero = dr::zeros<UInt32>();

        // Conversion, in case this is a double precision build
        dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_mint(0.f), ray_time(ray.time);

        uint32_t in[14] = { coherent.index(),  active.index(),
                            ray_o.x().index(), ray_o.y().index(),
                            ray_o.z().index(), ray_mint.index(),
                            ray_d.x().index(), ray_d.y().index(),
                            ray_d.z().index(), ray_time.index(),
                            ray_maxt.index(),  zero.index(),
                            zero.index(),      zero.index() };

        uint32_t out[1] { };

        jit_llvm_ray_trace(func_v.index(), scene_v.index(), 1, in, out);

        return active && (Single::steal(out[0]) != ray_maxt);
    } else {
        DRJIT_MARK_USED(ray);
        DRJIT_MARK_USED(coherent);
        DRJIT_MARK_USED(active);
        Throw("ray_test_cpu() should only be called in CPU mode.");
    }
}

template <typename Float, typename Spectrum>
typename EmbreeAccel<Float, Spectrum>::SurfaceInteraction3f
EmbreeAccel<Float, Spectrum>::ray_intersect_naive(
    const Scene<Float, Spectrum> * /*scene*/, const Ray3f & /*ray*/,
    Mask /*active*/) const {
    NotImplementedError("ray_intersect_naive");
}

NAMESPACE_END(mitsuba)
