#include <embree3/rtcore.h>

NAMESPACE_BEGIN(mitsuba)

#if defined(ENOKI_X86_AVX512F)
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

static RTCDevice __embree_device = nullptr;

MTS_VARIANT void Scene<Float, Spectrum>::accel_init_cpu(const Properties &/*props*/) {
    static_assert(is_float_v<scalar_t<Float>>, "Embree is not supported in double precision mode.");
    if (!__embree_device)
        __embree_device = rtcNewDevice("");

    Timer timer;
    RTCScene embree_scene = rtcNewScene(__embree_device);
    rtcSetSceneFlags(embree_scene, RTC_SCENE_FLAG_DYNAMIC);
    m_accel = embree_scene;

    for (Shape *shape : m_shapes)
         rtcAttachGeometry(embree_scene, shape->embree_geometry(__embree_device));

    rtcCommitScene(embree_scene);
    Log(Info, "Embree ready. (took %s)", util::time_string(timer.value()));
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_release_cpu() {
    for (Shape *shapegroup : m_shapegroups)
        shapegroup->release_embree_scene();
    rtcReleaseScene((RTCScene) m_accel);
}

MTS_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_cpu(const Ray3f &ray, Mask active) const {
    if constexpr (!is_cuda_array_v<Float>) {
        RTCIntersectContext context;
        rtcInitIntersectContext(&context);

        PreliminaryIntersection3f pi = zero<PreliminaryIntersection3f>();

        if constexpr (!is_array_v<Float>) {
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
            rtcIntersect1((RTCScene) m_accel, &context, &rh);

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
            context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;

            alignas(alignof(Float)) int valid[MTS_RAY_WIDTH];
            store(valid, select(active, Int32(-1), Int32(0)));

            RTCRayHitW rh;
            store(rh.ray.org_x, ray.o.x());
            store(rh.ray.org_y, ray.o.y());
            store(rh.ray.org_z, ray.o.z());
            store(rh.ray.tnear, ray.mint);
            store(rh.ray.dir_x, ray.d.x());
            store(rh.ray.dir_y, ray.d.y());
            store(rh.ray.dir_z, ray.d.z());
            store(rh.ray.time, Float(0));
            store(rh.ray.tfar, ray.maxt);
            store(rh.ray.mask, UInt32(0));
            store(rh.ray.id, UInt32(0));
            store(rh.ray.flags, UInt32(0));

            rtcIntersectW(valid, (RTCScene) m_accel, &context, &rh);

            Float t = load<Float>(rh.ray.tfar);
            Mask hit = active && neq(t, ray.maxt);

            if (likely(any(hit))) {
                using ShapePtr = replace_scalar_t<Float, const Shape *>;
                ScopedPhase sp(ProfilerPhase::CreateSurfaceInteraction);
                UInt32 shape_index = load<UInt32>(rh.hit.geomID);
                UInt32 prim_index  = load<UInt32>(rh.hit.primID);

                // We get level 0 because we only support one level of instancing
                UInt32 inst_index = load<UInt32>(rh.hit.instID[0]);

                Mask hit_not_inst = hit &&  eq(inst_index, RTC_INVALID_GEOMETRY_ID);
                Mask hit_inst     = hit && neq(inst_index, RTC_INVALID_GEOMETRY_ID);

                PreliminaryIntersection3f pi = zero<PreliminaryIntersection3f>();
                pi.t = select(hit, t, math::Infinity<Float>);

                // Set si.instance and si.shape
                UInt32 index   = select(hit_inst, inst_index, shape_index);
                ShapePtr shape = gather<ShapePtr>(m_shapes.data(), index, hit);
                masked(pi.instance, hit_inst)  = shape;
                masked(pi.shape, hit_not_inst) = shape;

                pi.prim_index = prim_index;
                pi.prim_uv = Point2f(load<Float>(rh.hit.u), load<Float>(rh.hit.v));
            }
        }
        return pi;
    } else {
        Throw("ray_intersect_preliminary_cpu() should only be called in CPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_cpu(const Ray3f &ray, HitComputeFlags flags, Mask active) const {
    if constexpr (!is_cuda_array_v<Float>) {
        RTCIntersectContext context;
        rtcInitIntersectContext(&context);
        SurfaceInteraction3f si = zero<SurfaceInteraction3f>();

        if constexpr (!is_array_v<Float>) {
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
            rtcIntersect1((RTCScene) m_accel, &context, &rh);

            if (rh.ray.tfar != ray.maxt) {
                ScopedPhase sp(ProfilerPhase::CreateSurfaceInteraction);
                uint32_t shape_index = rh.hit.geomID;
                uint32_t prim_index  = rh.hit.primID;

                // We get level 0 because we only support one level of instancing
                uint32_t inst_index = rh.hit.instID[0];

                PreliminaryIntersection3f pi = zero<PreliminaryIntersection3f>();

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
                si.t = math::Infinity<Float>;
            }
        } else {
            context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;

            alignas(alignof(Float)) int valid[MTS_RAY_WIDTH];
            store(valid, select(active, Int32(-1), Int32(0)));

            RTCRayHitW rh;
            store(rh.ray.org_x, ray.o.x());
            store(rh.ray.org_y, ray.o.y());
            store(rh.ray.org_z, ray.o.z());
            store(rh.ray.tnear, ray.mint);
            store(rh.ray.dir_x, ray.d.x());
            store(rh.ray.dir_y, ray.d.y());
            store(rh.ray.dir_z, ray.d.z());
            store(rh.ray.time, Float(0));
            store(rh.ray.tfar, ray.maxt);
            store(rh.ray.mask, UInt32(0));
            store(rh.ray.id, UInt32(0));
            store(rh.ray.flags, UInt32(0));

            rtcIntersectW(valid, (RTCScene) m_accel, &context, &rh);

            Float t = load<Float>(rh.ray.tfar);
            Mask hit = active && neq(t, ray.maxt);

            if (likely(any(hit))) {
                using ShapePtr = replace_scalar_t<Float, const Shape *>;
                ScopedPhase sp(ProfilerPhase::CreateSurfaceInteraction);
                UInt32 shape_index = load<UInt32>(rh.hit.geomID);
                UInt32 prim_index  = load<UInt32>(rh.hit.primID);

                // We get level 0 because we only support one level of instancing
                UInt32 inst_index = load<UInt32>(rh.hit.instID[0]);

                Mask hit_not_inst = hit &&  eq(inst_index, RTC_INVALID_GEOMETRY_ID);
                Mask hit_inst     = hit && neq(inst_index, RTC_INVALID_GEOMETRY_ID);

                PreliminaryIntersection3f pi;
                pi.t = select(hit, t, math::Infinity<Float>);

                // Set si.instance and si.shape
                UInt32 index   = select(hit_inst, inst_index, shape_index);
                ShapePtr shape = gather<ShapePtr>(m_shapes.data(), index, hit);
                masked(pi.instance, hit_inst)  = shape;
                masked(pi.shape, hit_not_inst) = shape;

                pi.prim_index = prim_index;
                pi.shape_index = shape_index;
                pi.prim_uv = Point2f(load<Float>(rh.hit.u), load<Float>(rh.hit.v));

                si = pi.compute_surface_interaction(ray, flags, active);
            } else {
                si.wavelengths = ray.wavelengths;
                si.time = ray.time;
                si.wi = -ray.d;
                si.t = math::Infinity<Float>;
            }
        }
        return si;
    } else {
        Throw("ray_intersect_cpu() should only be called in CPU mode.");
    }
}

MTS_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_cpu(const Ray3f &ray, Mask active) const {
    if constexpr (!is_cuda_array_v<Float>) {
        RTCIntersectContext context;
        rtcInitIntersectContext(&context);

        if constexpr (!is_array_v<Float>) {
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
            rtcOccluded1((RTCScene) m_accel, &context, &ray2);
            return ray2.tfar != ray.maxt;
        } else {
            context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;

            alignas(alignof(Float)) int valid[MTS_RAY_WIDTH];
            store(valid, select(active, Int32(-1), Int32(0)));

            RTCRayW ray2;
            store(ray2.org_x, ray.o.x());
            store(ray2.org_y, ray.o.y());
            store(ray2.org_z, ray.o.z());
            store(ray2.tnear, ray.mint);
            store(ray2.dir_x, ray.d.x());
            store(ray2.dir_y, ray.d.y());
            store(ray2.dir_z, ray.d.z());
            store(ray2.time, Float(0));
            store(ray2.tfar, ray.maxt);
            store(ray2.mask, UInt32(0));
            store(ray2.id, UInt32(0));
            store(ray2.flags, UInt32(0));
            rtcOccludedW(valid, (RTCScene) m_accel, &context, &ray2);
            return active && neq(load<Float>(ray2.tfar), ray.maxt);
        }
    } else {
        Throw("ray_intersect_cpu() should only be called in CPU mode.");
    }
}

NAMESPACE_END(mitsuba)
