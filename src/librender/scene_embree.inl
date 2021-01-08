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
    rtcSetSceneFlags(embree_scene, RTC_SCENE_FLAG_DYNAMIC);

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
    Log(Info, "Embree ready. (took %s)", util::time_string((float) timer.value()));
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_release_cpu() {
    EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;
    rtcReleaseScene((RTCScene) s.accel);
}

template <typename Ray, typename Float, typename Vector3u>
RTCRayNp bind_ray(Ray& ray, Float& t, Vector3u& extra) {
    RTCRayNp r;
    r.org_x = ray.o.x().data();
    r.org_y = ray.o.y().data();
    r.org_z = ray.o.z().data();
    r.dir_x = ray.d.x().data();
    r.dir_y = ray.d.y().data();
    r.dir_z = ray.d.z().data();
    r.tnear = ray.mint.data();
    r.tfar  = t.data();
    r.time  = ray.time.data();
    r.mask  = extra.x().data();
    r.id    = extra.y().data();
    r.flags = extra.z().data();
    return r;
}

template <typename PreliminaryIntersection3f, typename Vector3f, typename UInt32>
RTCHitNp bind_hit(PreliminaryIntersection3f& pi, Vector3f& ng, UInt32& inst_index) {
    RTCHitNp hit;
    hit.Ng_x = ng.x().data();
    hit.Ng_y = ng.y().data();
    hit.Ng_z = ng.z().data();
    hit.u    = pi.prim_uv.x().data();
    hit.v    = pi.prim_uv.y().data();
    hit.geomID    = pi.shape_index.data();
    hit.primID    = pi.prim_index.data();
    hit.instID[0] = inst_index.data();
    return hit;
}

RTCRayNp offset_rtc_ray(const RTCRayNp& r, size_t offset) {
    RTCRayNp ray;
    ray.org_x = r.org_x + offset;
    ray.org_y = r.org_y + offset;
    ray.org_z = r.org_z + offset;
    ray.dir_x = r.dir_x + offset;
    ray.dir_y = r.dir_y + offset;
    ray.dir_z = r.dir_z + offset;
    ray.tnear = r.tnear + offset;
    ray.tfar  = r.tfar + offset;
    ray.time  = r.time + offset;
    ray.mask  = r.mask + offset;
    ray.id    = r.id + offset;
    ray.flags = r.flags + offset;
    return ray;
}

RTCHitNp offset_rtc_hit(const RTCHitNp& h, size_t offset) {
    RTCHitNp hit;
    hit.Ng_x      = h.Ng_x + offset;
    hit.Ng_y      = h.Ng_y + offset;
    hit.Ng_z      = h.Ng_z + offset;
    hit.u         = h.u + offset;
    hit.v         = h.v + offset;
    hit.geomID    = h.geomID + offset;
    hit.primID    = h.primID + offset;
    hit.instID[0] = h.instID[0] + offset;
    return hit;
}

MTS_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_cpu(const Ray3f &ray_, Mask active) const {
    if constexpr (!ek::is_cuda_array_v<Float>) {
        EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;

        Ray3f ray = ray_;
        uint32_t N = (uint32_t) ek::width(ray);
        PreliminaryIntersection3f pi = ek::zero<PreliminaryIntersection3f>();

        if (!ek::is_array_v<Float> || N == 1) {
            RTCRayHit rh = {};
            rh.ray.org_x = ek::hsum(ray.o.x());
            rh.ray.org_y = ek::hsum(ray.o.y());
            rh.ray.org_z = ek::hsum(ray.o.z());
            rh.ray.dir_x = ek::hsum(ray.d.x());
            rh.ray.dir_y = ek::hsum(ray.d.y());
            rh.ray.dir_z = ek::hsum(ray.d.z());
            rh.ray.tnear = ek::hsum(ray.mint);
            rh.ray.tfar  = ek::hsum(ray.maxt);

            RTCIntersectContext context;
            rtcInitIntersectContext(&context);
            rtcIntersect1(s.accel, &context, &rh);

            if (ek::all(ek::neq(rh.ray.tfar, ray.maxt))) {
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
        } else {
            if constexpr (ek::is_array_v<Float>) {
                ek::resize(ray, N);
                ek::eval(ray);

                pi = ek::empty<PreliminaryIntersection3f>(N);

                // A ray is considered inactive if its tnear value is larger than its tfar value
                pi.t = ek::empty<Float>(N);
                jit_memcpy_async(JitBackend::LLVM, pi.t.data(), ray.maxt.data(),
                                sizeof(ScalarFloat) * N);
                pi.t = ek::select(active, pi.t, ray.mint - ek::Epsilon<Float>);

                Vector3f ng       = ek::empty<Vector3f>(N);
                UInt32 inst_index = ek::empty<UInt32>(N);
                Vector3u extra    = ek::zero<Vector3u>(N);

                ek::eval(extra, ng, inst_index, pi);
                ek::sync_device();

                RTCRayHitNp rh;
                rh.ray = bind_ray(ray, pi.t, extra);
                rh.hit = bind_hit(pi, ng, inst_index);

#ifndef PARALLEL_LLVM_RAY_INTERSECT
                RTCIntersectContext context;
                rtcInitIntersectContext(&context);
                // Force embree to use maximum packet width when dispatching rays
                context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;
                rtcIntersectNp(s.accel, &context, &rh, N);
#else
                tbb::parallel_for(
                    tbb::blocked_range<uint32_t>(0, N, 1024),
                    [&](const tbb::blocked_range<uint32_t> &range) {
                        ScopedPhase sp(ProfilerPhase::RayIntersect);
                        uint32_t offset = range.begin();
                        uint32_t size   = range.end() - offset;

                        RTCRayHitNp rh_;
                        rh_.ray = offset_rtc_ray(rh.ray, offset);
                        rh_.hit = offset_rtc_hit(rh.hit, offset);

                        RTCIntersectContext context;
                        rtcInitIntersectContext(&context);
                        // Force embree to use maximum packet width when dispatching rays
                        context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;
                        rtcIntersectNp(s.accel, &context, &rh_, size);
                    }
                );
#endif
                Mask hit = active && ek::neq(pi.t, ray.maxt);
                ek::masked(pi.t, !hit) = ek::Infinity<Float>;

                // Set si.instance and si.shape
                Mask hit_not_inst = hit &&  ek::eq(inst_index, RTC_INVALID_GEOMETRY_ID);
                Mask hit_inst     = hit && ek::neq(inst_index, RTC_INVALID_GEOMETRY_ID);
                UInt32 index      = ek::select(hit_inst, inst_index, pi.shape_index);

                ShapePtr shape = ek::gather<UInt32>(s.shapes_registry_ids, index, hit);
                pi.instance = ek::select(hit_inst, shape, nullptr);
                pi.shape    = ek::select(hit_not_inst, shape, nullptr);
            }
        }
        return pi;
    } else {
        Throw("ray_intersect_preliminary_cpu() should only be called in CPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_cpu(const Ray3f &ray, uint32_t hit_flags, Mask active) const {
    if constexpr (!ek::is_cuda_array_v<Float>) {
        PreliminaryIntersection3f pi = ray_intersect_preliminary_cpu(ray, active);
        return pi.compute_surface_interaction(ray, hit_flags, active);
    } else {
        Throw("ray_intersect_cpu() should only be called in CPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_cpu(const Ray3f &ray_, Mask active) const {
    if constexpr (!ek::is_cuda_array_v<Float>) {
        EmbreeState<Float> &s = *(EmbreeState<Float> *) m_accel;

        Ray3f ray = ray_;
        uint32_t N = (uint32_t) ek::width(ray);

        if (!ek::is_array_v<Float> || N == 1) {
            ENOKI_MARK_USED(active);

            RTCRay ray2 = {};
            ray2.org_x = ek::hsum(ray.o.x());
            ray2.org_y = ek::hsum(ray.o.y());
            ray2.org_z = ek::hsum(ray.o.z());
            ray2.dir_x = ek::hsum(ray.d.x());
            ray2.dir_y = ek::hsum(ray.d.y());
            ray2.dir_z = ek::hsum(ray.d.z());
            ray2.tnear = ek::hsum(ray.mint);
            ray2.tfar  = ek::hsum(ray.maxt);

            RTCIntersectContext context;
            rtcInitIntersectContext(&context);
            rtcOccluded1(s.accel, &context, &ray2);

            return ek::neq(ray2.tfar, ray.maxt);
        } else {
            if constexpr (ek::is_array_v<Float>) {
                ek::resize(ray, N);
                ek::eval(ray);

                // A ray is considered inactive if its tnear value is larger than its tfar value
                Float t = ek::empty<Float>(N);
                jit_memcpy_async(JitBackend::LLVM, t.data(), ray.maxt.data(),
                                sizeof(ScalarFloat) * N);
                t = ek::select(active, t, ray.mint - ek::Epsilon<Float>);

                Vector3u extra = ek::zero<Vector3u>(N);

                ek::eval(extra, t);
                ek::sync_device();

                RTCRayNp ray2 = bind_ray(ray, t, extra);

#ifndef PARALLEL_LLVM_RAY_INTERSECT
                RTCIntersectContext context;
                rtcInitIntersectContext(&context);
                // Force embree to use maximum packet width when dispatching rays
                context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;
                rtcOccludedNp(s.accel, &context, &ray2, N);
#else
                tbb::parallel_for(
                    tbb::blocked_range<uint32_t>(0, N, 1024),
                    [&](const tbb::blocked_range<uint32_t> &range) {
                        ScopedPhase sp(ProfilerPhase::RayTest);
                        uint32_t offset = range.begin();
                        uint32_t size   = range.end() - offset;
                        RTCRayNp ray_ = offset_rtc_ray(ray2, offset);

                        RTCIntersectContext context;
                        rtcInitIntersectContext(&context);
                        // Force embree to use maximum packet width when dispatching rays
                        context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;
                        rtcOccludedNp(s.accel, &context, &ray_, size);
                    }
                );
#endif
                return active && ek::neq(t, ray.maxt);
            }
        }
    } else {
        Throw("ray_test_cpu() should only be called in CPU mode.");
    }
}

NAMESPACE_END(mitsuba)
