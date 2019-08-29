#include <mitsuba/render/scene.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/timer.h>

NAMESPACE_BEGIN(mitsuba)

static RTCDevice __embree_device = nullptr;

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


#if defined(MTS_USE_EMBREE)
    #include <embree3/rtcore.h>
#endif

void Scene::embree_init() {
    if (!__embree_device)
        __embree_device = rtcNewDevice("");

    m_embree_scene = rtcNewScene(__embree_device);
}

void Scene::embree_release() {
    rtcReleaseScene(m_embree_scene);
}

void Scene::embree_register(Shape *shape) {
    rtcAttachGeometry(m_embree_scene, shape->embree_geometry(__embree_device));
}

void Scene::embree_build() {
    Timer timer;
    rtcCommitScene(m_embree_scene);
    Log(EInfo, "Embree finished. (took %s)", util::time_string(timer.value()));
}

SurfaceInteraction3f Scene::ray_intersect(const Ray3f &ray) const {
    ScopedPhase sp(EProfilerPhase::ERayIntersect);
    RTCIntersectContext context;
    rtcInitIntersectContext(&context);

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
    rtcIntersect1(m_embree_scene, &context, &rh);

    SurfaceInteraction3f si = zero<SurfaceInteraction3f>();
    if (rh.ray.tfar != ray.maxt) {
        ScopedPhase sp2(EProfilerPhase::ECreateSurfaceInteraction);
        uint32_t shape_index = rh.hit.geomID;
        uint32_t prim_index = rh.hit.primID;

        // Fill in basic information common to all shapes
        si.t = rh.ray.tfar;
        si.time = ray.time;
        si.wavelengths = ray.wavelengths;
        si.shape = m_shapes[shape_index];
        si.prim_index = prim_index;

        float cache[2] = { rh.hit.u, rh.hit.v };

        // Ask shape(s) to fill in the rest using the cache
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
        si.t = math::Infinity;
    }
    return si;
}

bool Scene::ray_test(const Ray3f &ray) const {
    ScopedPhase p(EProfilerPhase::ERayTest);

    RTCIntersectContext context;
    rtcInitIntersectContext(&context);

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
    rtcOccluded1(m_embree_scene, &context, &ray2);
    return ray2.tfar != ray.maxt;
}

SurfaceInteraction3fP Scene::ray_intersect(const Ray3fP &ray, MaskP active) const {
    ScopedPhase sp(EProfilerPhase::ERayIntersectP);
    RTCIntersectContext context;
    rtcInitIntersectContext(&context);
    context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;

    alignas(alignof(FloatP)) int valid[MTS_RAY_WIDTH];
    store(valid, select(active, Int32P(-1), Int32P(0)));

    RTCRayHitW rh;
    store(rh.ray.org_x, ray.o.x());
    store(rh.ray.org_y, ray.o.y());
    store(rh.ray.org_z, ray.o.z());
    store(rh.ray.tnear, ray.mint);
    store(rh.ray.dir_x, ray.d.x());
    store(rh.ray.dir_y, ray.d.y());
    store(rh.ray.dir_z, ray.d.z());
    store(rh.ray.time, FloatP(0));
    store(rh.ray.tfar, ray.maxt);
    store(rh.ray.mask, UInt32P(0));
    store(rh.ray.id, UInt32P(0));
    store(rh.ray.flags, UInt32P(0));

    rtcIntersectW(valid, m_embree_scene, &context, &rh);

    SurfaceInteraction3fP si = zero<SurfaceInteraction3fP>();
    MaskP hit = active && neq(load<FloatP>(rh.ray.tfar), ray.maxt);

    if (likely(any(hit))) {
        using ShapePtr = Array<const Shape *, PacketSize>;

        ScopedPhase sp2(EProfilerPhase::ECreateSurfaceInteractionP);
        UInt32P shape_index = load<UInt32P>(rh.hit.geomID);
        UInt32P prim_index = load<UInt32P>(rh.hit.primID);

        // Fill in basic information common to all shapes
        si.t = load<FloatP>(rh.ray.tfar);
        si.time = ray.time;
        si.wavelengths = ray.wavelengths;
        si.shape = gather<ShapePtr>(m_shapes.data(), shape_index, hit);
        si.prim_index = prim_index;

        FloatP cache[2] = { load<FloatP>(rh.hit.u), load<FloatP>(rh.hit.v) };

        // Ask shape(s) to fill in the rest using the cache
        si.shape->fill_surface_interaction(ray, cache, si, active);

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
    si.t[!hit] = math::Infinity;

    return si;
}

MaskP Scene::ray_test(const Ray3fP &ray, MaskP active) const {
    ScopedPhase p(EProfilerPhase::ERayTestP);

    RTCIntersectContext context;
    rtcInitIntersectContext(&context);
    context.flags = RTC_INTERSECT_CONTEXT_FLAG_COHERENT;

    alignas(alignof(FloatP)) int valid[MTS_RAY_WIDTH];
    store(valid, select(active, Int32P(-1), Int32P(0)));

    RTCRayW ray2;
    store(ray2.org_x, ray.o.x());
    store(ray2.org_y, ray.o.y());
    store(ray2.org_z, ray.o.z());
    store(ray2.tnear, ray.mint);
    store(ray2.dir_x, ray.d.x());
    store(ray2.dir_y, ray.d.y());
    store(ray2.dir_z, ray.d.z());
    store(ray2.time, FloatP(0));
    store(ray2.tfar, ray.maxt);
    store(ray2.mask, UInt32P(0));
    store(ray2.id, UInt32P(0));
    store(ray2.flags, UInt32P(0));
    rtcOccludedW(valid, m_embree_scene, &context, &ray2);
    return active && neq(load<FloatP>(ray2.tfar), ray.maxt);
}

NAMESPACE_END(mitsuba)
