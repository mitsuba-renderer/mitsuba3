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
    m_accel = embree_scene;
    for (Shape *shape : m_shapes)
        rtcAttachGeometry(embree_scene, shape->embree_geometry(__embree_device));
    rtcCommitScene(embree_scene);
    Log(Info, "Embree ready. (took %s)", util::time_string(timer.value()));
}

MTS_VARIANT void Scene<Float, Spectrum>::accel_release_cpu() {
    rtcReleaseScene((RTCScene) m_accel);
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_cpu(const Ray3f &ray, Mask active) const {
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
                uint32_t prim_index = rh.hit.primID;

                // Fill in basic information common to all shapes
                si.t = rh.ray.tfar;
                si.time = ray.time;
                si.wavelengths = ray.wavelengths;
                si.shape = m_shapes[shape_index];
                si.prim_index = prim_index;

                // Create the cache for the Mesh shape
                Float cache[2] = { rh.hit.u, rh.hit.v };

                // Ask shape to fill in the rest
                si.shape->fill_surface_interaction(ray, cache, si);

                // Gram-schmidt orthogonalization to compute local shading frame
                si.sh_frame.s = normalize(
                    fnmadd(si.sh_frame.n, dot(si.sh_frame.n, si.dp_du), si.dp_du));
                si.sh_frame.t = cross(si.sh_frame.n, si.sh_frame.s);

                // Incident direction in local coordinates
                si.wi = si.to_local(-ray.d);
            } else {
                si.wavelengths = ray.wavelengths;
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

            Mask hit = active && neq(load<Float>(rh.ray.tfar), ray.maxt);

            if (likely(any(hit))) {
                using ShapePtr = replace_scalar_t<Float, const Shape *>;
                ScopedPhase sp(ProfilerPhase::CreateSurfaceInteraction);
                UInt32 shape_index = load<UInt32>(rh.hit.geomID);
                UInt32 prim_index  = load<UInt32>(rh.hit.primID);

                // Fill in basic information common to all shapes
                si.t = load<Float>(rh.ray.tfar);
                si.time = ray.time;
                si.wavelengths = ray.wavelengths;
                si.shape = gather<ShapePtr>(m_shapes.data(), shape_index, hit);
                si.prim_index = prim_index;

                // Create the cache for the Mesh shapes
                Float cache[2] = { load<Float>(rh.hit.u), load<Float>(rh.hit.v) };

                // Ask shape(s) to fill in the rest
                si.shape->fill_surface_interaction(ray, cache, si, hit);

                // Gram-schmidt orthogonalization to compute local shading frame
                si.sh_frame.s = normalize(
                    fnmadd(si.sh_frame.n, dot(si.sh_frame.n, si.dp_du), si.dp_du));
                si.sh_frame.t = cross(si.sh_frame.n, si.sh_frame.s);

                // Incident direction in local coordinates
                si.wi = select(hit, si.to_local(-ray.d), -ray.d);
            } else {
                si.wavelengths = ray.wavelengths;
                si.wi = -ray.d;
            }
            si.t[!hit] = math::Infinity<Float>;
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
