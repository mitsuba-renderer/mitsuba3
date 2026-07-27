#include <embree3/rtcore.h>
#include <nanothread/nanothread.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/shapegroup.h>
#include <mitsuba/render/scene_ir.h>
#include "accel_cpu_common.h"
#include "embree.h"
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

// EllipsoidsMesh: drop back-facing triangle hits (d·Ng > 0).
template <size_t N, typename RTCRay_, typename RTCHit_>
static void embree_backface_cull_packet(const RTCFilterFunctionNArguments *args,
                                        RTCRay_ *ray, RTCHit_ *hit) {
    using FloatP = dr::Packet<float, N>;
    using Int32P = dr::Packet<int, N>;
    using Vector3fP = dr::Array<FloatP, 3>;

    Vector3fP d(dr::load_aligned<FloatP>(ray->dir_x),
                dr::load_aligned<FloatP>(ray->dir_y),
                dr::load_aligned<FloatP>(ray->dir_z));
    Vector3fP n(dr::load_aligned<FloatP>(hit->Ng_x),
                dr::load_aligned<FloatP>(hit->Ng_y),
                dr::load_aligned<FloatP>(hit->Ng_z));

    Int32P valid = dr::load_aligned<Int32P>(args->valid);
    valid = dr::select(dr::dot(d, n) > 0.f, 0, valid);
    dr::store_aligned(args->valid, valid);
}

static void embree_backface_cull(const RTCFilterFunctionNArguments *args) {
    switch (args->N) {
        case 1: {
            const RTCRay *ray = (const RTCRay *) args->ray;
            const RTCHit *hit = (const RTCHit *) args->hit;
            dr::Array<float, 3> d(ray->dir_x, ray->dir_y, ray->dir_z),
                                n(hit->Ng_x, hit->Ng_y, hit->Ng_z);
            if (dr::dot(d, n) > 0.f)
                *args->valid = 0;
            break;
        }
        case 4:  embree_backface_cull_packet<4>(args, (RTCRay4 *) args->ray, (RTCHit4 *) args->hit); break;
        case 8:  embree_backface_cull_packet<8>(args, (RTCRay8 *) args->ray, (RTCHit8 *) args->hit); break;
        case 16: embree_backface_cull_packet<16>(args, (RTCRay16 *) args->ray, (RTCHit16 *) args->hit); break;
        default: Throw("embree_backface_cull(): unsupported packet size!");
    }
}

/// Build one Embree geometry from a \ref ShapeIR.
template <typename Float, typename Spectrum>
static RTCGeometry
embree_make_geometry(RTCDevice device, const Shape<Float, Spectrum> *shape,
                     const tsl::robin_map<const void *, RTCSceneTy *,
                                          PointerHasher> &group_scenes) {
    ShapeIR g;
    shape->describe(g);

    switch (g.kind) {
        case ShapeIR::Kind::Custom: {
            RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_USER);
            rtcSetGeometryUserPrimitiveCount(geom, (unsigned int) g.prim_count);
            rtcSetGeometryUserData(geom, (void *) shape);
            rtcSetGeometryBoundsFunction(geom, embree_bbox<Float, Spectrum>, nullptr);
            rtcSetGeometryIntersectFunction(geom, embree_intersect<Float, Spectrum>);
            rtcSetGeometryOccludedFunction(geom, embree_occluded<Float, Spectrum>);
            rtcCommitGeometry(geom);
            return geom;
        }

        case ShapeIR::Kind::Triangles:
        case ShapeIR::Kind::TrianglesCulled: {
            RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
            rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0,
                                       RTC_FORMAT_FLOAT3, g.vertex_ptr, 0,
                                       3 * sizeof(float), g.vertex_count);
            rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0,
                                       RTC_FORMAT_UINT3, g.index_ptr, 0,
                                       3 * sizeof(uint32_t), g.face_count);
            if (g.kind == ShapeIR::Kind::TrianglesCulled) {
                rtcSetGeometryIntersectFilterFunction(geom, embree_backface_cull);
                rtcSetGeometryOccludedFilterFunction(geom, embree_backface_cull);
            }
            rtcCommitGeometry(geom);
            return geom;
        }

        case ShapeIR::Kind::BSplineCurve:
        case ShapeIR::Kind::LinearCurve: {
            RTCGeometry geom = rtcNewGeometry(
                device, g.kind == ShapeIR::Kind::BSplineCurve
                            ? RTC_GEOMETRY_TYPE_ROUND_BSPLINE_CURVE
                            : RTC_GEOMETRY_TYPE_ROUND_LINEAR_CURVE);
            rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0,
                                       RTC_FORMAT_FLOAT4, g.cp_ptr, 0,
                                       4 * sizeof(float), g.cp_count);
            rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0,
                                       RTC_FORMAT_UINT, g.seg_ptr, 0,
                                       sizeof(uint32_t), g.seg_count);
            rtcCommitGeometry(geom);
            return geom;
        }

        case ShapeIR::Kind::Instance: {
            RTCScene nested = group_scenes.at(g.group_id);

            RTCGeometry inst = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_INSTANCE);
            rtcSetGeometryInstancedScene(inst, nested);
            rtcSetGeometryTimeStepCount(inst, 1);
            // Column-major 3x4 (g.to_world[col*3+row]) -> column-major 4x4.
            float M[16];
            for (int col = 0; col < 4; ++col) {
                for (int row = 0; row < 3; ++row)
                    M[col * 4 + row] = g.to_world[col * 3 + row];
                M[col * 4 + 3] = (col == 3) ? 1.f : 0.f;
            }
            rtcSetGeometryTransform(inst, 0, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, M);
            rtcCommitGeometry(inst);
            return inst;
        }
    }
    return nullptr; // unreachable
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
    rebuild(scene);

    Log(Info, "Embree ready. (took %s)",
        util::time_string((float) timer.value()));

    if constexpr (dr::is_llvm_v<Float>)
        shapes_registry_ids = build_registry_ids<Float, Spectrum>(scene->m_shapes);
}

template <typename Float, typename Spectrum>
void EmbreeAccel<Float, Spectrum>::rebuild(
    Scene<Float, Spectrum> *scene) {
    if constexpr (dr::is_llvm_v<Float>)
        dr::sync_thread();

    for (int geo : geometries)
        rtcDetachGeometry(accel, geo);
    geometries.clear();

    // Rebuild nested scenes first so Instances can reference them. Attach all
    // geometry before the single LLVM sync below.
    for (auto &kv : group_scenes)
        rtcReleaseScene(kv.second);
    group_scenes.clear();

    std::vector<RTCScene> nested_scenes;
    nested_scenes.reserve(scene->m_shapegroups.size());
    for (auto &group : scene->m_shapegroups) {
        RTCScene nested = rtcNewScene(embree_device);
        for (const ref<Shape> &child : group->shapes()) {
            RTCGeometry cg = embree_make_geometry<Float, Spectrum>(
                embree_device, child.get(), group_scenes);
            rtcAttachGeometry(nested, cg);
            rtcReleaseGeometry(cg);
        }
        // Publish the uncommitted nested scene for top-level Instances.
        group_scenes[(const void *) group.get()] = nested;
        nested_scenes.push_back(nested);
    }

    for (Shape *shape : scene->m_shapes) {
        RTCGeometry geom = embree_make_geometry<Float, Spectrum>(
            embree_device, shape, group_scenes);
        geometries.push_back(rtcAttachGeometry(accel, geom));
        rtcReleaseGeometry(geom);
    }

    // One sync for the whole rebuild: all geometry (nested + top-level) is now
    // evaluated before any BVH build reads it.
    if constexpr (dr::is_llvm_v<Float>)
        dr::sync_thread();

    // Commit the nested scenes (each must be committed before the top-level
    // scene that instances it).
    for (RTCScene nested : nested_scenes)
        rtcCommitScene(nested);

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

    // The RTCScene pointer and entry points stay stable across rebuilds, so
    // initialize the handles only once. The cleanup callback keeps the native
    // scene alive until pending ray-tracing kernels finish.
    if constexpr (dr::is_llvm_v<Float>) {
        if (!accel_handle.index()) {
            init_mapped_handle(
                accel_handle, (void *) accel,
                [](uint32_t /* index */, int free, void *payload) {
                    if (free)
                        jit_enqueue_host_func(
                            JitBackend::LLVM,
                            [](void *p) { rtcReleaseScene((RTCScene) p); },
                            payload);
                },
                (void *) accel);

            // The LLVM vector width is fixed over the scene's lifetime.
            uint32_t jit_width = jit_llvm_vector_width();
            switch (jit_width) {
                case 1:  func_ptr = (void *) rtcIntersect1;  occlude_func_ptr = (void *) rtcOccluded1;  break;
                case 4:  func_ptr = (void *) rtcIntersect4;  occlude_func_ptr = (void *) rtcOccluded4;  break;
                case 8:  func_ptr = (void *) rtcIntersect8;  occlude_func_ptr = (void *) rtcOccluded8;  break;
                case 16: func_ptr = (void *) rtcIntersect16; occlude_func_ptr = (void *) rtcOccluded16; break;
                case 32: func_ptr = (void *) rtcIntersect32; occlude_func_ptr = (void *) rtcOccluded32; break;
                default:
                    Throw("EmbreeAccel::rebuild(): Dr.Jit is configured for "
                          "vectors of width %u, which is not supported by "
                          "Embree!", jit_width);
            }

            map_func_handles(func_handle, occlude_handle, func_ptr,
                             occlude_func_ptr);
        }
    }
}

template <typename Float, typename Spectrum>
void EmbreeAccel<Float, Spectrum>::release() {
    if (!accel)
        return; // already released
    if constexpr (dr::is_llvm_v<Float>) {
        // Ensure all ray tracing kernels are terminated before releasing the
        // scene.
        dr::sync_thread();

        // Drop the reference count of the handle variable. This will trigger
        // the deferred release of the Embree scene if no ray tracing calls are
        // pending.
        accel_handle = 0;
    } else {
        // Immediately release Embree structures in scalar mode.
        rtcReleaseScene(accel);
    }

    // Drop our nested-scene references. The top-level scene's instance
    // geometries keep them alive until it is itself released.
    for (auto &kv : group_scenes)
        rtcReleaseScene(kv.second);
    group_scenes.clear();

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

        if (rh.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
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

            pi.valid = true;
            pi.shape_index = shape_index;

            pi.t = rh.ray.tfar;
            pi.prim_index = prim_index;
            pi.prim_uv = Point2f(rh.hit.u, rh.hit.v);
        }

        return pi;
    } else if constexpr (dr::is_llvm_v<Float>) {
        dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_time(ray.time);

        uint32_t out[8] { };
        cpu_llvm_ray_trace<Float>((void *) func_ptr, func_handle.index(),
                                  (void *) accel, accel_handle.index(), ray_o,
                                  ray_d, ray_time, ray_maxt, coherent, active,
                                  0, out);

        // Embree traces in float32, so the hit fields are stolen as ``Single``.
        return decode_cpu_llvm_pi<Float, Spectrum, Single>(out,
                                                           shapes_registry_ids);
    } else {
        DRJIT_MARK_USED(ray);
        DRJIT_MARK_USED(coherent);
        DRJIT_MARK_USED(active);
        Throw("EmbreeAccel::ray_intersect_preliminary() should only be called in CPU mode.");
    }
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

        return ray2.tfar < 0.f;
    } else if constexpr (dr::is_llvm_v<Float>) {
        // Conversion, in case this is a double precision build
        dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_time(ray.time);

        // Shadow ray: trace against rtcOccludedN, which accepts any hit and
        // terminates traversal early.
        uint32_t out[1] { };
        cpu_llvm_ray_trace<Float>((void *) occlude_func_ptr,
                                  occlude_handle.index(), (void *) accel,
                                  accel_handle.index(), ray_o, ray_d, ray_time,
                                  ray_maxt, coherent, active, 1, out);

        return Mask::steal(out[0]);
    } else {
        DRJIT_MARK_USED(ray);
        DRJIT_MARK_USED(coherent);
        DRJIT_MARK_USED(active);
        Throw("EmbreeAccel::ray_test() should only be called in CPU mode.");
    }
}

template <typename Float, typename Spectrum>
typename EmbreeAccel<Float, Spectrum>::SurfaceInteraction3f
EmbreeAccel<Float, Spectrum>::ray_intersect_naive(
    const Scene<Float, Spectrum> *scene, const Ray3f &ray, Mask active) const {
    // Embree has no brute-force path; route through the accelerated query.
    return scene->ray_intersect(ray, active);
}

NAMESPACE_END(mitsuba)
