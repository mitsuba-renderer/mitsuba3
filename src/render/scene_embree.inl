#include <embree3/rtcore.h>
#include <nanothread/nanothread.h>
#include <thread>

NAMESPACE_BEGIN(mitsuba)

static_assert(sizeof(RTCIntersectContext) == 24 /* Dr.Jit assumes this */);

static uint32_t embree_threads = 0;
static RTCDevice embree_device = nullptr;

template <typename Float>
struct EmbreeState {
    MI_IMPORT_CORE_TYPES()
    RTCScene accel;
    std::vector<int> geometries;
    DynamicBuffer<UInt32> shapes_registry_ids;
    bool is_nested_scene = false;
};

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

MI_VARIANT void
Scene<Float, Spectrum>::accel_init_cpu(const Properties &props) {
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

    m_accel = new EmbreeState<Float>();
    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;

    // Check if another scene was passed to the constructor
    for (auto &[k, v] : props.objects()) {
        if (dynamic_cast<Scene *>(v.get())) {
            s.is_nested_scene = true;
            break;
        }
    }

    s.accel = rtcNewScene(embree_device);
    rtcSetSceneBuildQuality(s.accel, RTC_BUILD_QUALITY_HIGH);
    bool use_robust = props.get<bool>("embree_use_robust_intersections", false);
    rtcSetSceneFlags(s.accel, use_robust ? RTC_SCENE_FLAG_ROBUST : RTC_SCENE_FLAG_NONE);

    ScopedPhase phase(ProfilerPhase::InitAccel);
    accel_parameters_changed_cpu();

    Log(Info, "Embree ready. (took %s)",
        util::time_string((float) timer.value()));

    if constexpr (dr::is_llvm_v<Float>) {
        // Get shapes registry ids
        if (!m_shapes.empty()) {
            std::unique_ptr<uint32_t[]> data(new uint32_t[m_shapes.size()]);
            for (size_t i = 0; i < m_shapes.size(); i++)
                data[i] = jit_registry_id(m_shapes[i]);
            s.shapes_registry_ids
                = dr::load<DynamicBuffer<UInt32>>(data.get(), m_shapes.size());
        } else {
            s.shapes_registry_ids = dr::zeros<DynamicBuffer<UInt32>>();
        }
    }
}

MI_VARIANT void Scene<Float, Spectrum>::accel_parameters_changed_cpu() {
    if constexpr (dr::is_llvm_v<Float>)
        dr::sync_thread();

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
    if constexpr (dr::is_llvm_v<Float>)
        dr::sync_thread();

    // Avoid getting in a deadlock when building a nested scene while rendering
    if (s.is_nested_scene) {
        rtcCommitScene(s.accel);
    } else {
        dr::parallel_for(
            dr::blocked_range<size_t>(0, embree_threads, 1),
            [&](const dr::blocked_range<size_t> &) {
                rtcJoinCommitScene(s.accel);
            }
        );
    }

    /* Set up a callback on the handle variable to release the Embree
       acceleration data structure (IAS) when this variable is freed. This
       ensures that the lifetime of the IAS goes beyond the one of the Scene
       instance if there are still some pending ray tracing calls (e.g. non
       evaluated variables depending on a ray tracing call). */
    if constexpr (dr::is_llvm_v<Float>) {
        // Prevents the IAS to be released when updating the scene parameters
        if (m_accel_handle.index())
            jit_var_set_callback(m_accel_handle.index(), nullptr, nullptr);
        m_accel_handle = dr::opaque<UInt64>(s.accel);
        jit_var_set_callback(
            m_accel_handle.index(),
            [](uint32_t /* index */, int free, void *payload) {
                if (free) {
                    // Enqueue delayed function to ensure all ray tracing
                    // kernels are terminated before releasing the scene. This
                    // is needed in the scenario where we record a ray-tracing
                    // operation, the scene is destroyed, and we only trigger an
                    // evaluation afterwards.

                    jit_enqueue_host_func(
                        JitBackend::LLVM,
                        [](void *p) {
                            EmbreeState<Float> *s = (EmbreeState<Float> *) p;
                            rtcReleaseScene(s->accel);
                            delete s;
                        },
                        payload
                    );
                }
            },
            (void *) m_accel
        );
    }

    clear_shapes_dirty();
}

MI_VARIANT void Scene<Float, Spectrum>::accel_release_cpu() {
    if constexpr (dr::is_llvm_v<Float>) {
        // Ensure all ray tracing kernels are terminated before releasing the scene
        dr::sync_thread();

        /* Decrease the reference count of the handle variable. This will
           trigger the release of the Embree acceleration data structure if no
           ray tracing calls are pending. */
        m_accel_handle = 0;
        m_accel = nullptr;
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_cpu(const Ray3f &ray,
                                                      Mask coherent,
                                                      Mask active) const {
    using Single = dr::float32_array_t<Float>;
    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;

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

        rtcIntersect1(s.accel, &context, &rh);

        if (rh.ray.tfar != ray_maxt) {
            uint32_t shape_index = rh.hit.geomID;
            uint32_t prim_index  = rh.hit.primID;

            // We get level 0 because we only support one level of instancing
            uint32_t inst_index = rh.hit.instID[0];

            // If the hit is not on an instance
            bool hit_instance = inst_index != RTC_INVALID_GEOMETRY_ID;
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
    } else if constexpr (dr::is_llvm_v<Float>) {
        uint32_t jit_width = jit_llvm_vector_width();

        void *scene_ptr = (void *) s.accel,
             *func_ptr  = nullptr;

        switch (jit_width) {
            case 1:  func_ptr = (void *) rtcIntersect1;  break;
            case 4:  func_ptr = (void *) rtcIntersect4;  break;
            case 8:  func_ptr = (void *) rtcIntersect8;  break;
            case 16: func_ptr = (void *) rtcIntersect16; break;
            case 32: func_ptr = (void *) rtcIntersect32; break;
            default:
                Throw("ray_intersect_preliminary_cpu(): Dr.Jit is "
                      "configured for vectors of width %u, which is not "
                      "supported by Embree!", jit_width);
        }

        UInt64 func_v  = UInt64::steal(jit_var_pointer(
                   JitBackend::LLVM, func_ptr, m_accel_handle.index(), 0)),
               scene_v = UInt64::steal(
                   jit_var_pointer(JitBackend::LLVM, scene_ptr, 0, 0));

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

        ShapePtr shape = dr::gather<UInt32>(s.shapes_registry_ids, index, hit);

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

MI_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_cpu(const Ray3f &ray, uint32_t ray_flags, Mask coherent, Mask active) const {
    if constexpr (!dr::is_cuda_v<Float>) {
        PreliminaryIntersection3f pi = ray_intersect_preliminary_cpu(ray, coherent, active);
        return pi.compute_surface_interaction(ray, ray_flags, active);
    } else {
        DRJIT_MARK_USED(ray);
        DRJIT_MARK_USED(ray_flags);
        DRJIT_MARK_USED(coherent);
        DRJIT_MARK_USED(active);
        Throw("ray_intersect_cpu() should only be called in CPU mode.");
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_cpu(const Ray3f &ray, Mask coherent, Mask active) const {
    using Single = dr::float32_array_t<Float>;
    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;

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

        rtcOccluded1(s.accel, &context, &ray2);

        return ray2.tfar != ray_maxt;
    } else if constexpr (dr::is_llvm_v<Float>) {
        uint32_t jit_width = jit_llvm_vector_width();

        void *scene_ptr = (void *) s.accel,
             *func_ptr  = nullptr;

        switch (jit_width) {
            case 1:  func_ptr = (void *) rtcOccluded1;  break;
            case 4:  func_ptr = (void *) rtcOccluded4;  break;
            case 8:  func_ptr = (void *) rtcOccluded8;  break;
            case 16: func_ptr = (void *) rtcOccluded16; break;
            case 32: func_ptr = (void *) rtcOccluded32; break;
            default:
                Throw("ray_test_cpu(): Dr.Jit is configured for vectors of "
                      "width %u, which is not supported by Embree!", jit_width);
        }

        UInt64 func_v  = UInt64::steal(jit_var_pointer(
                   JitBackend::LLVM, func_ptr, m_accel_handle.index(), 0)),
               scene_v = UInt64::steal(
                   jit_var_pointer(JitBackend::LLVM, scene_ptr, 0, 0));

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

MI_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_naive_cpu(const Ray3f &ray,
                                                Mask active) const {
    return ray_intersect_cpu(ray, +RayFlags::All, false, active);
}

NAMESPACE_END(mitsuba)
