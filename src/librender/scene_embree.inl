#include <embree3/rtcore.h>

NAMESPACE_BEGIN(mitsuba)

#if defined(ENOKI_X86_AVX512)
#  define MTS_RAY_WIDTH 16
#elif defined(ENOKI_X86_AVX2)
#  define MTS_RAY_WIDTH 8
#elif defined(ENOKI_X86_SSE42)
#  define MTS_RAY_WIDTH 4
#else
#  define MTS_RAY_WIDTH 4
// #  error Expected to use vectorization
#endif

#define JOIN(x, y)        JOIN_AGAIN(x, y)
#define JOIN_AGAIN(x, y)  x ## y
#define RTCRayHitW        JOIN(RTCRayHit,    MTS_RAY_WIDTH)
#define RTCRayW           JOIN(RTCRay,       MTS_RAY_WIDTH)
#define rtcIntersectW     JOIN(rtcIntersect, MTS_RAY_WIDTH)
#define rtcOccludedW      JOIN(rtcOccluded,  MTS_RAY_WIDTH)

static RTCDevice __embree_device = nullptr;

template <typename Float>
struct EmbreeState {
    RTCScene accel;
    std::vector<uint32_t> shapes_registry_ids;
    uint8_t* buffer = nullptr;
    size_t wavefront_size = 0;
};

MTS_VARIANT void Scene<Float, Spectrum>::accel_init_cpu(const Properties &/*props*/) {
    static_assert(std::is_same_v<ek::scalar_t<Float>, float>, "Embree is not supported in double precision mode.");
    if (!__embree_device)
        __embree_device = rtcNewDevice("");

    Timer timer;
    RTCScene embree_scene = rtcNewScene(__embree_device);
    rtcSetSceneFlags(embree_scene, RTC_SCENE_FLAG_DYNAMIC);

    m_accel = new EmbreeState<Float>();
    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;
    s.accel = embree_scene;

    for (Shape *shape : m_shapes)
         rtcAttachGeometry(embree_scene, shape->embree_geometry(__embree_device));

    if constexpr (ek::is_llvm_array_v<Float>) {
        // Copy shapes registry ids to the GPU
        s.shapes_registry_ids.resize(m_shapes.size());
        for (size_t i = 0; i < m_shapes.size(); i++)
            s.shapes_registry_ids[i] = jitc_registry_get_id(m_shapes[i]);
    }

    rtcCommitScene(embree_scene);
    Log(Info, "Embree ready. (took %s)", util::time_string(timer.value()));
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_release_cpu() {
    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;
    rtcReleaseScene((RTCScene) s.accel);
    std::free(s.buffer);
}

template <typename State>
void check_buffer_allocation(State& s, size_t N) {
    if (!s.buffer || s.wavefront_size < N) {
        s.wavefront_size = N;

        if (s.buffer)
            std::free(s.buffer);

        s.buffer = (uint8_t *) std::aligned_alloc(
            16, N * (14 * sizeof(float) + 6 * sizeof(unsigned int)));
    }
}

template <typename State, typename Ray>
RTCRayNp bind_ray_buffer_and_copy(const State& s, const Ray& ray) {
    size_t size_f = sizeof(float) * s.wavefront_size;
    size_t size_u = sizeof(unsigned int) * s.wavefront_size;

    size_t offset_f = 0;
    size_t offset_u = 0;

    RTCRayNp r;
    r.org_x = (float*) (s.buffer + offset_f++ * size_f + offset_u * size_u);
    r.org_y = (float*) (s.buffer + offset_f++ * size_f + offset_u * size_u);
    r.org_z = (float*) (s.buffer + offset_f++ * size_f + offset_u * size_u);
    r.dir_x = (float*) (s.buffer + offset_f++ * size_f + offset_u * size_u);
    r.dir_y = (float*) (s.buffer + offset_f++ * size_f + offset_u * size_u);
    r.dir_z = (float*) (s.buffer + offset_f++ * size_f + offset_u * size_u);
    r.tnear = (float*) (s.buffer + offset_f++ * size_f + offset_u * size_u);
    r.tfar  = (float*) (s.buffer + offset_f++ * size_f + offset_u * size_u);
    r.time  = (float*) (s.buffer + offset_f++ * size_f + offset_u * size_u);
    r.mask  = (unsigned int*) (s.buffer + offset_f * size_f + offset_u++ * size_u);
    r.id    = (unsigned int*) (s.buffer + offset_f * size_f + offset_u++ * size_u);
    r.flags = (unsigned int*) (s.buffer + offset_f * size_f + offset_u++ * size_u);

    ek::store(r.org_x, ray.o.x());
    ek::store(r.org_y, ray.o.y());
    ek::store(r.org_z, ray.o.z());
    ek::store(r.dir_x, ray.d.x());
    ek::store(r.dir_y, ray.d.y());
    ek::store(r.dir_z, ray.d.z());
    ek::store(r.tnear, ray.mint);
    ek::store(r.tfar,  ray.maxt);
    ek::store(r.time,  ray.time);
    std::memset(r.mask,  0, size_u);
    std::memset(r.id,    0, size_u);
    std::memset(r.flags, 0, size_u);

    return r;
}

template <typename State>
RTCHitNp bind_hit_buffer(const State& s) {
    size_t size_f = sizeof(float) * s.wavefront_size;
    size_t size_u = sizeof(unsigned int) * s.wavefront_size;

    // Already account for the ray offset
    size_t offset_f = 9;
    size_t offset_u = 3;

    RTCHitNp hit;
    hit.Ng_x = (float*) (s.buffer + offset_f++ * size_f + offset_u * size_u);
    hit.Ng_y = (float*) (s.buffer + offset_f++ * size_f + offset_u * size_u);
    hit.Ng_z = (float*) (s.buffer + offset_f++ * size_f + offset_u * size_u);
    hit.u    = (float*) (s.buffer + offset_f++ * size_f + offset_u * size_u);
    hit.v    = (float*) (s.buffer + offset_f++ * size_f + offset_u * size_u);
    hit.geomID    = (unsigned int*) (s.buffer + offset_f * size_f + offset_u++ * size_u);
    hit.primID    = (unsigned int*) (s.buffer + offset_f * size_f + offset_u++ * size_u);
    hit.instID[0] = (unsigned int*) (s.buffer + offset_f * size_f + offset_u++ * size_u);
    return hit;
}

MTS_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_cpu(const Ray3f &ray_, Mask active) const {
    Ray3f ray = ray_;

    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;

    if constexpr (!ek::is_cuda_array_v<Float>) {
        RTCIntersectContext context;
        rtcInitIntersectContext(&context);

        PreliminaryIntersection3f pi = ek::zero<PreliminaryIntersection3f>();

        if constexpr (!ek::is_array_v<Float>) {
            RTCRayHit rh;
            rh.ray.org_x = ray.o.x();
            rh.ray.org_y = ray.o.y();
            rh.ray.org_z = ray.o.z();
            rh.ray.tnear = ray.mint;
            rh.ray.dir_x = ray.d.x();
            rh.ray.dir_y = ray.d.y();
            rh.ray.dir_z = ray.d.z();
            rh.ray.time = 0;
            rh.ray.tfar = ray.maxt;
            rh.ray.mask = 0;
            rh.ray.id = 0;
            rh.ray.flags = 0;
            rtcIntersect1(s.accel, &context, &rh);

            if (rh.ray.tfar != ray.maxt) {
                ScopedPhase sp(ProfilerPhase::CreateSurfaceInteraction);
                uint32_t shape_index = rh.hit.geomID;
                uint32_t prim_index  = rh.hit.primID;

                // We get level 0 because we only support one level of instancing
                uint32_t inst_index = rh.hit.instID[0];

                // If the hit is not on an instance
                if (inst_index == RTC_INVALID_GEOMETRY_ID) {
                    // If the hit is not on an instance
                    pi.shape = m_shapes[shape_index];
                } else {
                    // If the hit is on an instance
                    pi.instance = m_shapes[inst_index];
                    pi.shape_index = shape_index;
                }

                pi.t = rh.ray.tfar;
                pi.prim_index = prim_index;
                pi.prim_uv = Point2f(rh.hit.u, rh.hit.v);
            }
        } else {
            // TODO refactoring: is this necessary?
            // context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;

            size_t N = ek::max(ek::width(ray.o), ek::width(ray.d));

            if (ek::width(ray.o) < N) {
                ray.o.x().resize(N);
                ray.o.y().resize(N);
                ray.o.z().resize(N);
            }

            if (ek::width(ray.d) < N) {
                ray.d.x().resize(N);
                ray.d.y().resize(N);
                ray.d.z().resize(N);
            }
            if (ek::width(ray.time) < N)
                ray.time.resize(N);
            if (ek::width(ray.mint) < N)
                ray.mint.resize(N);
            if (ek::width(ray.maxt) < N)
                ray.maxt.resize(N);

            // A ray is considered inactive if its tnear value is larger than its tfar value
            ek::masked(ray.maxt, !active) = ray.mint - 1.f;

            ek::schedule(ray);
            jitc_eval();
            jitc_sync_device();

            check_buffer_allocation(s, N);

            RTCRayHitNp rh;
            rh.ray = bind_ray_buffer_and_copy(s, ray);
            rh.hit = bind_hit_buffer(s);

            rtcIntersectNp(s.accel, &context, &rh, N);

            Float t = ek::load_unaligned<Float>(rh.ray.tfar, N);
            Mask hit = active && ek::neq(t, ray.maxt);

            if (likely(ek::any(hit))) {
                ScopedPhase sp(ProfilerPhase::CreateSurfaceInteraction);

                UInt32 shape_index = ek::load_unaligned<UInt32>(rh.hit.geomID, N);
                UInt32 prim_index  = ek::load_unaligned<UInt32>(rh.hit.primID, N);

                // We get level 0 because we only support one level of instancing
                UInt32 inst_index  = ek::load_unaligned<UInt32>(rh.hit.instID[0], N);

                Mask hit_not_inst = hit &&  ek::eq(inst_index, RTC_INVALID_GEOMETRY_ID);
                Mask hit_inst     = hit && ek::neq(inst_index, RTC_INVALID_GEOMETRY_ID);

                PreliminaryIntersection3f pi = ek::zero<PreliminaryIntersection3f>();
                pi.t = ek::select(hit, t, ek::Infinity<Float>);

                // Set si.instance and si.shape
                UInt32 index   = ek::select(hit_inst, inst_index, shape_index);
                ShapePtr shape = ek::gather<ShapePtr>(s.shapes_registry_ids.data(), index, hit);
                ek::masked(pi.instance, hit_inst)  = shape;
                ek::masked(pi.shape, hit_not_inst) = shape;

                pi.prim_index = prim_index;

                Float u = ek::load_unaligned<Float>(rh.hit.u, N);
                Float v = ek::load_unaligned<Float>(rh.hit.v, N);
                pi.prim_uv = { u, v };
            }
        }
        return pi;
    } else {
        Throw("ray_intersect_preliminary_cpu() should only be called in CPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_cpu(const Ray3f &ray_, HitComputeFlags flags, Mask active) const {
    Ray3f ray = ray_;

    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;

    if constexpr (!ek::is_cuda_array_v<Float>) {
        RTCIntersectContext context;
        rtcInitIntersectContext(&context);
        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();

        if constexpr (!ek::is_array_v<Float>) {
            RTCRayHit rh;
            rh.ray.org_x = ray.o.x();
            rh.ray.org_y = ray.o.y();
            rh.ray.org_z = ray.o.z();
            rh.ray.tnear = ray.mint;
            rh.ray.dir_x = ray.d.x();
            rh.ray.dir_y = ray.d.y();
            rh.ray.dir_z = ray.d.z();
            rh.ray.time = 0;
            rh.ray.tfar = ray.maxt;
            rh.ray.mask = 0;
            rh.ray.id = 0;
            rh.ray.flags = 0;
            rtcIntersect1(s.accel, &context, &rh);

            if (rh.ray.tfar != ray.maxt) {
                ScopedPhase sp(ProfilerPhase::CreateSurfaceInteraction);
                uint32_t shape_index = rh.hit.geomID;
                uint32_t prim_index  = rh.hit.primID;

                // We get level 0 because we only support one level of instancing
                uint32_t inst_index = rh.hit.instID[0];

                PreliminaryIntersection3f pi = ek::zero<PreliminaryIntersection3f>();

                // If the hit is not on an instance
                if (inst_index == RTC_INVALID_GEOMETRY_ID) {
                    // If the hit is not on an instance
                    pi.shape = m_shapes[shape_index];
                } else {
                    // If the hit is on an instance
                    pi.instance = m_shapes[inst_index];
                    pi.shape_index = shape_index;
                }

                pi.t = rh.ray.tfar;
                pi.prim_index = prim_index;
                pi.prim_uv = Point2f(rh.hit.u, rh.hit.v);

                si = pi.compute_surface_interaction(ray, flags, active);
            } else {
                si.wavelengths = ray.wavelengths;
                si.time = ray.time;
                si.wi = -ray.d;
                si.t = ek::Infinity<Float>;
            }
        } else {
            size_t N = ek::max(ek::width(ray.o), ek::width(ray.d));

            // TODO refactoring: add this to array_router.h and struct.h
            if (ek::width(ray.o) < N) {
                ray.o.x().resize(N);
                ray.o.y().resize(N);
                ray.o.z().resize(N);
            }

            if (ek::width(ray.d) < N) {
                ray.d.x().resize(N);
                ray.d.y().resize(N);
                ray.d.z().resize(N);
            }

            if (ek::width(ray.time) < N)
                ray.time.resize(N);
            if (ek::width(ray.mint) < N)
                ray.mint.resize(N);
            if (ek::width(ray.maxt) < N)
                ray.maxt.resize(N);

            // A ray is considered inactive if its tnear value is larger than its tfar value
            ek::masked(ray.maxt, !active) = ray.mint - 1.f;

            ek::schedule(ray);
            jitc_eval();
            jitc_sync_device();

            check_buffer_allocation(s, N);

            RTCRayHitNp rh;
            rh.ray = bind_ray_buffer_and_copy(s, ray);
            rh.hit = bind_hit_buffer(s);

            rtcIntersectNp(s.accel, &context, &rh, N);

            Float t = ek::load_unaligned<Float>(rh.ray.tfar, N);
            Mask hit = active && ek::neq(t, ray.maxt);

            // TODO refactoring: remove this horizontal reduction
            if (likely(ek::any(hit))) {
                using ShapePtr = ek::replace_scalar_t<Float, const Shape *>;
                ScopedPhase sp(ProfilerPhase::CreateSurfaceInteraction);

                UInt32 shape_index = ek::load_unaligned<UInt32>(rh.hit.geomID, N);
                UInt32 prim_index  = ek::load_unaligned<UInt32>(rh.hit.primID, N);
                // We get level 0 because we only support one level of instancing
                UInt32 inst_index  = ek::load_unaligned<UInt32>(rh.hit.instID[0], N);

                Mask hit_not_inst = hit &&  ek::eq(inst_index, RTC_INVALID_GEOMETRY_ID);
                Mask hit_inst     = hit && ek::neq(inst_index, RTC_INVALID_GEOMETRY_ID);

                PreliminaryIntersection3f pi;
                pi.t = ek::select(hit, t, ek::Infinity<Float>);

                // Set si.instance and si.shape
                UInt32 index   = ek::select(hit_inst, inst_index, shape_index);
                ShapePtr shape = ek::gather<ShapePtr>(s.shapes_registry_ids.data(), index, hit);
                ek::masked(pi.instance, hit_inst)  = shape;
                ek::masked(pi.shape, hit_not_inst) = shape;

                pi.prim_index = prim_index;
                pi.shape_index = shape_index;

                Float u = ek::load_unaligned<Float>(rh.hit.u, N);
                Float v = ek::load_unaligned<Float>(rh.hit.v, N);
                pi.prim_uv = { u, v };

                si = pi.compute_surface_interaction(ray, flags, hit);
            } else {
                si.wavelengths = ray.wavelengths;
                si.time = ray.time;
                si.wi = -ray.d;
                si.t = ek::Infinity<Float>;
            }
        }

        return si;
    } else {
        Throw("ray_intersect_cpu() should only be called in CPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_cpu(const Ray3f &ray_, Mask active) const {
    Ray3f ray = ray_;
    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;
    if constexpr (!ek::is_cuda_array_v<Float>) {
        RTCIntersectContext context;
        rtcInitIntersectContext(&context);

        if constexpr (!ek::is_array_v<Float>) {
            RTCRay ray2;
            ray2.org_x = ray.o.x();
            ray2.org_y = ray.o.y();
            ray2.org_z = ray.o.z();
            ray2.tnear = ray.mint;
            ray2.dir_x = ray.d.x();
            ray2.dir_y = ray.d.y();
            ray2.dir_z = ray.d.z();
            ray2.time = 0;
            ray2.tfar = ray.maxt;
            ray2.mask = 0;
            ray2.id = 0;
            ray2.flags = 0;
            rtcOccluded1(s.accel, &context, &ray2);
            return ray2.tfar != ray.maxt;
        } else {
            size_t N = ek::max(ek::width(ray.o), ek::width(ray.d));

            if (ek::width(ray.o) < N) {
                ray.o.x().resize(N);
                ray.o.y().resize(N);
                ray.o.z().resize(N);
            }

            if (ek::width(ray.d) < N) {
                ray.d.x().resize(N);
                ray.d.y().resize(N);
                ray.d.z().resize(N);
            }

            if (ek::width(ray.time) < N)
                ray.time.resize(N);
            if (ek::width(ray.mint) < N)
                ray.mint.resize(N);
            if (ek::width(ray.maxt) < N)
                ray.maxt.resize(N);

            // A ray is considered inactive if its tnear value is larger than its tfar value
            ek::masked(ray.maxt, !active) = ray.mint - 1.f;

            ek::schedule(ray);
            jitc_eval();
            jitc_sync_device();

            check_buffer_allocation(s, N);

            RTCRayNp ray2 = bind_ray_buffer_and_copy(s, ray);

            rtcOccludedNp(s.accel, &context, &ray2, N);

            Float t = ek::load_unaligned<Float>(ray2.tfar, N);
            return active && ek::neq(t, ray.maxt);
        }
    } else {
        Throw("ray_test_cpu() should only be called in CPU mode.");
    }
}

NAMESPACE_END(mitsuba)
