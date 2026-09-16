#include <embree3/rtcore.h>
#include <nanothread/nanothread.h>
#include <tsl/robin_map.h>
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

template <size_t N>
static void embree_null_filter_packet(const RTCFilterFunctionNArguments *args) {
    using UInt32P = dr::Packet<uint32_t, N>;

    uint32_t *flags_ptr = &RTCRayN_flags(args->ray, N, 0),
             *valid_ptr = (uint32_t *) args->valid;

    UInt32P flags = dr::load_aligned<UInt32P>(flags_ptr),
            valid = dr::load_aligned<UInt32P>(valid_ptr);

    auto null = (valid != 0u) && ((flags & (uint32_t) CPURayFlags::SkipNull) != 0u);
    dr::store_aligned(flags_ptr, dr::select(null, flags | (uint32_t) CPURayFlags::HasNull, flags));
    dr::store_aligned(valid_ptr, dr::select(null, UInt32P(0u), valid));
}

// Filter that skips null intersections and records their presence
static void embree_null_filter(const RTCFilterFunctionNArguments *args) {
    switch (args->N) {
        case 1: {
            unsigned int &flags = RTCRayN_flags(args->ray, 1, 0);
            if (*args->valid && (flags & CPURayFlags::SkipNull)) {
                flags |= CPURayFlags::HasNull;
                *args->valid = 0;
            }
            break;
        }
        case 4:  embree_null_filter_packet<4>(args); break;
        case 8:  embree_null_filter_packet<8>(args); break;
        case 16: embree_null_filter_packet<16>(args); break;
        default: Throw("embree_null_filter(): unsupported packet size!");
    }
}

static void embree_backface_cull_null(const RTCFilterFunctionNArguments *args) {
    embree_backface_cull(args);
    embree_null_filter(args);
}

/// Embree entry points and device property for a Dr.Jit vector width
struct EmbreeWidthInfo {
    RTCDeviceProperty native;
    void *intersect, *occluded;
};

static bool embree_width_info(uint32_t width, EmbreeWidthInfo &info) {
    switch (width) {
        case 4:  info = { RTC_DEVICE_PROPERTY_NATIVE_RAY4_SUPPORTED,  (void *) rtcIntersect4,  (void *) rtcOccluded4 };  return true;
        case 8:  info = { RTC_DEVICE_PROPERTY_NATIVE_RAY8_SUPPORTED,  (void *) rtcIntersect8,  (void *) rtcOccluded8 };  return true;
        case 16: info = { RTC_DEVICE_PROPERTY_NATIVE_RAY16_SUPPORTED, (void *) rtcIntersect16, (void *) rtcOccluded16 }; return true;
        default: return false;
    }
}

/// Release the bindings of a JIT variant's scene, then the scene itself once
/// the ray tracing kernels in flight have completed
static void embree_release_state(EmbreeSceneState *s) {
    for (JitIsectBinding *binding : s->isect_bindings)
        jit_isect_unbind(binding);
    s->isect_bindings.clear();

    jit_enqueue_host_func(
        JitBackend::LLVM,
        [](void *p) {
            auto *s = (EmbreeSceneState *) p;
            rtcReleaseScene(s->scene);
            delete s;
        },
        s);
}

/// Build one Embree geometry from a `ShapeIR`.
template <typename Float, typename Spectrum>
static RTCGeometry
embree_make_geometry(RTCDevice device, const ShapeIR &g,
                     EmbreeAccel<Float, Spectrum> &accel) {
    using Shape = Shape<Float, Spectrum>;
    const Shape *shape = (const Shape *) g.ctx;
    bool null = shape->has_null();

    switch (g.kind) {
        case ShapeIR::Kind::Custom: {
            RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_USER);
            rtcSetGeometryMask(geom, g.visibility_mask);
            rtcSetGeometryUserPrimitiveCount(geom, (unsigned int) g.prim_count);
            if constexpr (dr::is_llvm_v<Float>) {
                // Dr.Jit compiles the shape's recorded intersection routine.
                JitIsectBinding *binding = jit_isect_bind(
                    g.isect_func, (uintptr_t) accel.accel, 0, (void *) shape);
                accel.state->isect_bindings.push_back(binding);

                rtcSetGeometryUserData(geom, binding);
                rtcSetGeometryBoundsFunction(geom, embree_bbox_jit<Float, Spectrum>, nullptr);
                rtcSetGeometryIntersectFunction(geom, embree_intersect_jit);
                rtcSetGeometryOccludedFunction(
                    geom, null ? embree_occluded_null_jit : embree_occluded_jit);
            } else {
                rtcSetGeometryUserData(geom, (void *) shape);
                rtcSetGeometryBoundsFunction(geom, embree_bbox<Float, Spectrum>, nullptr);
                rtcSetGeometryIntersectFunction(geom, embree_intersect<Float, Spectrum>);
                rtcSetGeometryOccludedFunction(geom, embree_occluded<Float, Spectrum>);
            }
            rtcCommitGeometry(geom);
            return geom;
        }

        case ShapeIR::Kind::Triangles:
        case ShapeIR::Kind::TrianglesCulled: {
            RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
            rtcSetGeometryMask(geom, g.visibility_mask);
            rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0,
                                       RTC_FORMAT_FLOAT3, g.vertex_ptr, 0,
                                       g.vertex_stride, g.vertex_count);
            rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0,
                                       RTC_FORMAT_UINT3, g.index_ptr, 0,
                                       g.index_stride, g.face_count);
            if (g.kind == ShapeIR::Kind::TrianglesCulled) {
                rtcSetGeometryIntersectFilterFunction(geom, embree_backface_cull);
                rtcSetGeometryOccludedFilterFunction(
                    geom, null ? embree_backface_cull_null
                               : embree_backface_cull);
            } else if (null) {
                rtcSetGeometryOccludedFilterFunction(geom, embree_null_filter);
            }
            rtcSetGeometryUserData(geom, (void *) shape);
            rtcCommitGeometry(geom);
            return geom;
        }

        case ShapeIR::Kind::BSplineCurve:
        case ShapeIR::Kind::LinearCurve: {
            RTCGeometry geom = rtcNewGeometry(
                device, g.kind == ShapeIR::Kind::BSplineCurve
                            ? RTC_GEOMETRY_TYPE_ROUND_BSPLINE_CURVE
                            : RTC_GEOMETRY_TYPE_ROUND_LINEAR_CURVE);
            rtcSetGeometryMask(geom, g.visibility_mask);
            rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0,
                                       RTC_FORMAT_FLOAT4, g.cp_ptr, 0,
                                       4 * sizeof(float), g.cp_count);
            rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0,
                                       RTC_FORMAT_UINT, g.seg_ptr, 0,
                                       sizeof(uint32_t), g.seg_count);
            if (null)
                rtcSetGeometryOccludedFilterFunction(geom, embree_null_filter);
            rtcSetGeometryUserData(geom, (void *) shape);
            rtcCommitGeometry(geom);
            return geom;
        }

        case ShapeIR::Kind::Instance: {
            RTCScene nested = accel.group_scenes.at(g.group_id);

            size_t n_keyframes = g.keyframes.size();
            if (n_keyframes > RTC_MAX_TIME_STEP_COUNT)
                Throw("embree_make_geometry(): an animated instance may span at most "
                      "%u time steps, but keyframes had %zu.",
                      (unsigned int) RTC_MAX_TIME_STEP_COUNT, n_keyframes);

            RTCGeometry inst = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_INSTANCE);
            rtcSetGeometryInstancedScene(inst, nested);

            if (n_keyframes > 1) {
                float t_min = (float) accel.time_min, t_max = (float) accel.time_max,
                      t_head = g.keyframes.front().time,
                      t_tail = g.keyframes.back().time;

                if (t_head > t_min || t_tail < t_max)
                    Log(Warn, "embree_make_geometry(): animated instance does not cover the "
                              "full time range [%f, %f] (instance time range: [%f, %f]). "
                              "Outside this range the geometry will not be visible in Embree.",
                              t_min, t_max, t_head, t_tail);

                rtcSetGeometryTimeStepCount(inst, (unsigned int) n_keyframes);

                float inv_time_range = t_max > t_min ? 1.f / (t_max - t_min) : 0.f;
                float start_time = t_head <= t_min ? 0.f : (t_head - t_min) * inv_time_range;
                float end_time   = t_tail >= t_max ? 1.f : (t_tail - t_min) * inv_time_range;
                rtcSetGeometryTimeRange(inst, start_time, end_time);

                for (size_t i = 0; i < n_keyframes; ++i) {
                    const auto &kf = g.keyframes[i];
                    RTCQuaternionDecomposition rtc_decomp;
                    rtcInitQuaternionDecomposition(&rtc_decomp);
                    // Embree expects the real part first
                    rtcQuaternionDecompositionSetQuaternion(
                        &rtc_decomp, kf.quat[3], kf.quat[0], kf.quat[1], kf.quat[2]);
                    rtcQuaternionDecompositionSetScale(
                        &rtc_decomp, kf.scale[0], kf.scale[1], kf.scale[2]);
                    rtcQuaternionDecompositionSetTranslation(
                        &rtc_decomp, kf.trans[0], kf.trans[1], kf.trans[2]);
                    rtcSetGeometryTransformQuaternion(inst, (unsigned int) i, &rtc_decomp);
                }
            } else {
                // Static case is a simple transform
                rtcSetGeometryTimeStepCount(inst, 1);
                float M[16];
                for (int col = 0; col < 4; ++col) {
                    for (int row = 0; row < 3; ++row)
                        M[col * 4 + row] = g.to_world[col * 3 + row];
                    M[col * 4 + 3] = (col == 3) ? 1.f : 0.f;
                }
                rtcSetGeometryTransform(inst, 0, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, M);
            }
            // Scalar-mode hits resolve nested geometry through this scene
            rtcSetGeometryUserData(inst, (void *) nested);
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

    if constexpr (dr::is_llvm_v<Float>) {
        uint32_t width = jit_llvm_vector_width();
        EmbreeWidthInfo info;
        if (!embree_width_info(width, info))
            Throw("EmbreeAccel::init(): Dr.Jit is configured for vectors "
                  "of width %u, but Embree only supports widths 4, 8, "
                  "and 16!", width);
        if (!rtcGetDeviceProperty(embree_device, info.native))
            Throw("EmbreeAccel::init(): Dr.Jit is configured for vectors of "
                  "width %u, but Embree was built without native support "
                  "for %u-wide ray packets on this machine!", width, width);
        func_ptr = info.intersect;
        occlude_func_ptr = info.occluded;
    }

    Timer timer;

    // A nested scene may be built while its host scene renders, which
    // decides how the commit below runs
    is_nested_scene = props.has_property("parent_scene");

    accel = rtcNewScene(embree_device);
    rtcSetSceneBuildQuality(accel, RTC_BUILD_QUALITY_HIGH);
    bool use_robust = props.get<bool>("embree_use_robust_intersections", false);
    rtcSetSceneFlags(accel, use_robust ? RTC_SCENE_FLAG_ROBUST : RTC_SCENE_FLAG_NONE);

    // The RTCScene pointer and entry points stay stable across rebuilds, so
    // the handles are initialized once. The cleanup callback releases the
    // state (bindings and native scene) once no pending ray tracing call
    // references the scene anymore.
    if constexpr (dr::is_llvm_v<Float>) {
        state = new EmbreeSceneState();
        state->scene = accel;
        init_mapped_handle(
            accel_handle, (void *) accel,
            [](uint32_t /* index */, int free, void *payload) {
                if (free)
                    embree_release_state((EmbreeSceneState *) payload);
            },
            (void *) state);
        map_func_handles(func_handle, occlude_handle, func_ptr,
                         occlude_func_ptr);
    }

    ScopedPhase phase(ProfilerPhase::InitAccel);
    rebuild(scene);

    Log(Info, "Embree ready. (took %s)",
        util::time_string((float) timer.value()));
}

template <typename Float, typename Spectrum>
void EmbreeAccel<Float, Spectrum>::rebuild(
    Scene<Float, Spectrum> *scene) {
    // Lower the scene once. Embree only needs the per-shape records, not the
    // BLAS partitioning, and keeps the instance records for its own instances
    SceneIR sd = SceneIRBuilder<Float, Spectrum>::build(scene);

    if constexpr (dr::is_llvm_v<Float>) {
        dr::sync_thread();

        // The bindings belong to the geometries detached below
        for (JitIsectBinding *binding : state->isect_bindings)
            jit_isect_unbind(binding);
        state->isect_bindings.clear();
    }

    for (unsigned int geo : geometries)
        rtcDetachGeometry(accel, geo);
    geometries.clear();

    time_min = sd.time_min;
    time_max = sd.time_max;

    // Rebuild nested scenes first so Instances can reference them. Attach all
    // geometry before the single LLVM sync below.
    for (auto &kv : group_scenes)
        rtcReleaseScene(kv.second);
    group_scenes.clear();

    const auto &groups = scene->shapegroups();
    std::vector<RTCScene> nested_scenes;
    nested_scenes.reserve(groups.size());
    for (size_t i = 0; i < groups.size(); ++i) {
        RTCScene nested = rtcNewScene(embree_device);
        for (uint32_t bi : sd.group_blases[i]) {
            for (const ShapeIR &g : sd.blases[bi].geoms) {
                RTCGeometry cg = embree_make_geometry<Float, Spectrum>(
                    embree_device, g, *this);
                if constexpr (dr::is_llvm_v<Float>)
                    // The child's registry id doubles as its geometry ID, so a
                    // nested hit directly reports the child shape
                    rtcAttachGeometryByID(nested, cg, jit_registry_id(g.ctx));
                else
                    rtcAttachGeometry(nested, cg);
                rtcReleaseGeometry(cg);
            }
        }
        // Publish the uncommitted nested scene for top-level Instances.
        group_scenes[(const void *) groups[i].get()] = nested;
        nested_scenes.push_back(nested);
    }

    // Attach top-level geometry using the explicit ID scheme described in
    // accel_embree.h
    auto attach = [&](const ShapeIR &g, unsigned int id) {
        RTCGeometry geom = embree_make_geometry<Float, Spectrum>(
            embree_device, g, *this);
        rtcAttachGeometryByID(accel, geom, id);
        geometries.push_back(id);
        rtcReleaseGeometry(geom);
    };

    instance_count = (uint32_t) sd.instance_shapes.size();
    for (uint32_t i = 0; i < instance_count; ++i)
        attach(sd.instance_shapes[i], i);

    uint32_t scalar_id = instance_count;
    for (uint32_t bi : sd.top_blases) {
        for (const ShapeIR &g : sd.blases[bi].geoms) {
            unsigned int id;
            if constexpr (dr::is_llvm_v<Float>)
                id = instance_count + jit_registry_id(g.ctx);
            else
                id = scalar_id++;
            attach(g, id);
        }
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

}

template <typename Float, typename Spectrum>
void EmbreeAccel<Float, Spectrum>::release() {
    if (!accel)
        return; // already released
    if constexpr (dr::is_llvm_v<Float>) {
        // Ensure all ray tracing kernels are terminated before releasing the
        // scene.
        dr::sync_thread();

        // Drop the reference count of the handle variable. Its callback
        // releases the state (bindings and native scene) once no ray tracing
        // call referencing the scene is pending anymore.
        accel_handle = 0;
        state = nullptr;
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
    Mask active, UInt32 ray_mask) const {
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

        Single ray_time = normalize_ray_time(Single(ray.time));
        RTCRayHit rh;
        dr::store(&rh.ray.org_x, dr::concat(Vector3s(ray.o), float(0.f)));
        dr::store(&rh.ray.dir_x, dr::concat(Vector3s(ray.d), float(ray_time)));
        rh.ray.tfar = ray_maxt;
        rh.ray.mask = ray_mask;
        rh.ray.id = 0;
        rh.ray.flags = 0;
        rh.hit.geomID = (uint32_t) -1;

        rtcIntersect1(accel, &context, &rh);

        if (rh.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
            uint32_t geom_id = rh.hit.geomID;

            // We get level 0 because we only support one level of instancing
            uint32_t inst_index = rh.hit.instID[0];

            if (inst_index != RTC_INVALID_GEOMETRY_ID) {
                // Instanced hit: the top-level ID is the instance index, whose
                // geometry stores the nested scene that reports the leaf shape
                RTCScene nested = (RTCScene) rtcGetGeometryUserData(
                    rtcGetGeometry(accel, inst_index));
                pi.shape = (const Shape *) rtcGetGeometryUserData(
                    rtcGetGeometry(nested, geom_id));
                pi.instance_index = inst_index + 1;
            } else {
                pi.shape = (const Shape *) rtcGetGeometryUserData(
                    rtcGetGeometry(accel, geom_id));
            }

            pi.valid = true;
            pi.t = rh.ray.tfar;
            pi.prim_index = rh.hit.primID;
            pi.prim_uv = Point2f(rh.hit.u, rh.hit.v);
        }

        return pi;
    } else if constexpr (dr::is_llvm_v<Float>) {
        dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_time = normalize_ray_time(Single(ray.time));
        uint32_t out[8] { };
        cpu_llvm_ray_trace<Float>((void *) func_ptr, func_handle.index(),
                                  (void *) accel, accel_handle.index(), ray_o,
                                  ray_d, ray_time, ray_maxt, coherent, active,
                                  ray_mask, 0, out);

        // Embree traces in float32, so the hit fields are stolen as ``Single``.
        PreliminaryIntersection3f pi;
        pi.valid      = Mask::steal(out[0]);
        pi.t          = Float(Single::steal(out[1]));
        pi.prim_uv    = Vector2f(Single::steal(out[2]), Single::steal(out[3]));
        pi.prim_index = UInt32::steal(out[4]);

        UInt32 geom_id    = UInt32::steal(out[5]),
               inst_index = UInt32::steal(out[6]);
        Mask hit_inst = Mask::steal(out[7]);

        // Undo the geometry ID encoding (see accel_embree.h). The shape
        // pointer of missed lanes must be masked (stale IDs would index
        // dispatch tables out of bounds). ``inst_index`` is -1 unless the
        // lane has an instanced hit; adding 1 therefore needs no mask.
        if (instance_count > 0) {
            UInt32 shape_id =
                dr::select(hit_inst, geom_id, geom_id - instance_count);
            pi.shape = dr::reinterpret_array<ShapePtr, UInt32>(
                dr::select(pi.valid, shape_id, 0u));
            pi.instance_index = inst_index + 1u;
        } else {
            pi.shape = dr::reinterpret_array<ShapePtr, UInt32>(
                dr::select(pi.valid, geom_id, 0u));
            pi.instance_index = dr::zeros<UInt32>();
        }

        return pi;
    } else {
        DRJIT_MARK_USED(ray);
        DRJIT_MARK_USED(coherent);
        DRJIT_MARK_USED(active);
        Throw("EmbreeAccel::ray_intersect_preliminary() should only be called in CPU mode.");
    }
}

template <typename Float, typename Spectrum>
ShadowTest<typename EmbreeAccel<Float, Spectrum>::Mask>
EmbreeAccel<Float, Spectrum>::ray_test(const Scene<Float, Spectrum> * /*scene*/,
                                       const Ray3f &ray, Mask coherent,
                                       Mask active,
                                       UInt32 ray_mask,
                                       bool skip_null) const {
    using Single = dr::float32_array_t<Float>;

    // Rays with SkipNull pass through shapes with null transmission
    // and record the encounter in the flags word
    uint32_t ray_flags = skip_null ? (uint32_t) CPURayFlags::SkipNull : 0;

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

        Single ray_time = normalize_ray_time(Single(ray.time));
        RTCRay ray2;
        dr::store(&ray2.org_x, dr::concat(Vector3s(ray.o), float(0.f)));
        dr::store(&ray2.dir_x, dr::concat(Vector3s(ray.d), float(ray_time)));
        ray2.tfar = (float) ray_maxt;
        ray2.mask = ray_mask;
        ray2.id = 0;
        ray2.flags = ray_flags;

        rtcOccluded1(accel, &context, &ray2);

        return { ray2.tfar < 0.f,
                 (ray2.flags & CPURayFlags::HasNull) != 0 };
    } else if constexpr (dr::is_llvm_v<Float>) {
        // Conversion, in case this is a double precision build
        dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_time = normalize_ray_time(Single(ray.time));

        // Shadow ray: trace against rtcOccludedN, which accepts any hit and
        // terminates traversal early.
        uint32_t out[2] { };
        cpu_llvm_ray_trace<Float>((void *) occlude_func_ptr,
                                  occlude_handle.index(), (void *) accel,
                                  accel_handle.index(), ray_o, ray_d,
                                  ray_time, ray_maxt, coherent, active,
                                  ray_mask, 1, out, ray_flags);

        return { Mask::steal(out[0]),
                 (UInt32::steal(out[1]) & (uint32_t) CPURayFlags::HasNull) != 0u };
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
