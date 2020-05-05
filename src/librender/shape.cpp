#include <mitsuba/render/kdtree.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/core/plugin.h>

#if defined(MTS_ENABLE_EMBREE)
    #include <embree3/rtcore.h>
#endif

NAMESPACE_BEGIN(mitsuba)

#if defined(MTS_ENABLE_EMBREE)
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
#define RTCHitW           JOIN(RTCHit,       MTS_RAY_WIDTH)
#endif

MTS_VARIANT Shape<Float, Spectrum>::Shape(const Properties &props) : m_id(props.id()) {
    m_to_world = props.transform("to_world", ScalarTransform4f());
    m_to_object = m_to_world.inverse();

    for (auto &kv : props.objects()) {
        Emitter *emitter = dynamic_cast<Emitter *>(kv.second.get());
        Sensor *sensor = dynamic_cast<Sensor *>(kv.second.get());
        BSDF *bsdf = dynamic_cast<BSDF *>(kv.second.get());
        Medium *medium = dynamic_cast<Medium *>(kv.second.get());

        if (emitter) {
            if (m_emitter)
                Throw("Only a single Emitter child object can be specified per shape.");
            m_emitter = emitter;
        } else if (bsdf) {
            if (m_bsdf)
                Throw("Only a single BSDF child object can be specified per shape.");
            m_bsdf = bsdf;
        } else if (medium) {
            if (kv.first == "interior") {
                if (m_interior_medium)
                    Throw("Only a single interior medium can be specified per shape.");
                m_interior_medium = medium;
            } else if (kv.first == "exterior") {
                if (m_exterior_medium)
                    Throw("Only a single exterior medium can be specified per shape.");
                m_exterior_medium = medium;
            }
        } else if (sensor) {
            if (m_sensor)
                Throw("Only a single Sensor child object can be specified per shape.");
            m_sensor = sensor;
        } else {
            Throw("Tried to add an unsupported object of type \"%s\"", kv.second);
        }
    }

    // Create a default diffuse BSDF if needed.
    if (!m_bsdf)
        m_bsdf = PluginManager::instance()->create_object<BSDF>(Properties("diffuse"));
}

MTS_VARIANT Shape<Float, Spectrum>::~Shape() {}

MTS_VARIANT std::string Shape<Float, Spectrum>::id() const {
    return m_id;
}

MTS_VARIANT typename Shape<Float, Spectrum>::PositionSample3f
Shape<Float, Spectrum>::sample_position(Float /*time*/, const Point2f & /*sample*/,
                                        Mask /*active*/) const {
    NotImplementedError("sample_position");
}

MTS_VARIANT Float Shape<Float, Spectrum>::pdf_position(const PositionSample3f & /*ps*/, Mask /*active*/) const {
    NotImplementedError("pdf_position");
}

#if defined(MTS_ENABLE_EMBREE)
template <typename Float, typename Spectrum>
void embree_bbox(const struct RTCBoundsFunctionArguments* args) {
    MTS_IMPORT_TYPES(Shape)
    const Shape* shape = (const Shape*) args->geometryUserPtr;
    ScalarBoundingBox3f bbox = shape->bbox();
    RTCBounds* bounds_o = args->bounds_o;
    bounds_o->lower_x = bbox.min.x();
    bounds_o->lower_y = bbox.min.y();
    bounds_o->lower_z = bbox.min.z();
    bounds_o->upper_x = bbox.max.x();
    bounds_o->upper_y = bbox.max.y();
    bounds_o->upper_z = bbox.max.z();
}

template <typename Float, typename Spectrum>
void embree_intersect_scalar(int* valid,
                             void* geometryUserPtr,
                             unsigned int geomID,
                             RTCRay* rtc_ray,
                             RTCHit* rtc_hit) {
    MTS_IMPORT_TYPES(Shape)

    const Shape* shape = (const Shape*) geometryUserPtr;

    if (!valid[0])
        return;

    // Create a Mitsuba ray
    Ray3f ray;
    ray.o.x() = rtc_ray->org_x;
    ray.o.y() = rtc_ray->org_y;
    ray.o.z() = rtc_ray->org_z;
    ray.d.x() = rtc_ray->dir_x;
    ray.d.y() = rtc_ray->dir_y;
    ray.d.z() = rtc_ray->dir_z;
    ray.mint  = rtc_ray->tnear;
    ray.maxt  = rtc_ray->tfar;
    ray.time  = rtc_ray->time;
    ray.update();

    // Check whether this is a shadow ray or not
    if (rtc_hit) {
        auto [success, tt] = shape->ray_intersect(ray, nullptr);
        if (success) {
            rtc_ray->tfar = tt;
            rtc_hit->geomID = geomID;
        }
    } else {
        if (shape->ray_test(ray))
            rtc_ray->tfar = -math::Infinity<Float>;
    }
}

template <typename Float, typename Spectrum>
void embree_intersect_packet(int* valid,
                             void* geometryUserPtr,
                             unsigned int geomID,
                             RTCRayW* rays,
                             RTCHitW* hits) {
    MTS_IMPORT_TYPES(Shape)
    using Int = replace_scalar_t<Float, int>;

    const Shape* shape = (const Shape*) geometryUserPtr;

    Mask active = neq(load<Int>(valid), 0);
    if (none(active))
        return;

    // Create Mitsuba ray
    Ray3f ray;
    ray.o.x() = load<Float>(rays->org_x);
    ray.o.y() = load<Float>(rays->org_y);
    ray.o.z() = load<Float>(rays->org_z);
    ray.d.x() = load<Float>(rays->dir_x);
    ray.d.y() = load<Float>(rays->dir_y);
    ray.d.z() = load<Float>(rays->dir_z);
    ray.mint  = load<Float>(rays->tnear);
    ray.maxt  = load<Float>(rays->tfar);
    ray.time  = load<Float>(rays->time);
    ray.update();

    // Check whether this is a shadow ray or not
    if (hits) {
        auto [success, tt] = shape->ray_intersect(ray, nullptr, active);
        active &= success;
        store(rays->tfar, tt, active);
        store(hits->geomID, Int(geomID), active);
    } else {
        active &= shape->ray_test(ray);
        store(rays->tfar, Float(-math::Infinity<Float>), active);
    }
}

template <typename Float, typename Spectrum>
void embree_intersect(const RTCIntersectFunctionNArguments* args) {
    if constexpr (!is_array_v<Float>) {
        RTCRayHit *rh = (RTCRayHit *) args->rayhit;
        embree_intersect_scalar<Float, Spectrum>(args->valid, args->geometryUserPtr, args->geomID,
                                                 (RTCRay*) &rh->ray, (RTCHit*) &rh->hit);
    } else {
        RTCRayHitW *rh = (RTCRayHitW *) args->rayhit;
        embree_intersect_packet<Float, Spectrum>(args->valid, args->geometryUserPtr, args->geomID,
                                                 (RTCRayW*) &rh->ray, (RTCHitW*) &rh->hit);
    }
}

template <typename Float, typename Spectrum>
void embree_occluded(const RTCOccludedFunctionNArguments* args) {
    if constexpr (!is_array_v<Float>) {
        embree_intersect_scalar<Float, Spectrum>(args->valid, args->geometryUserPtr, args->geomID,
                                                 (RTCRay*) args->ray, nullptr);
    } else {
        embree_intersect_packet<Float, Spectrum>(args->valid, args->geometryUserPtr, args->geomID,
                                                 (RTCRayW*) args->ray, nullptr);
    }
}

MTS_VARIANT RTCGeometry Shape<Float, Spectrum>::embree_geometry(RTCDevice device) const {
    if constexpr (!is_cuda_array_v<Float>) {
        RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_USER);
        rtcSetGeometryUserPrimitiveCount(geom, 1);
        rtcSetGeometryUserData(geom, (void *) this);
        rtcSetGeometryBoundsFunction(geom, embree_bbox<Float, Spectrum>, nullptr);
        rtcSetGeometryIntersectFunction(geom, embree_intersect<Float, Spectrum>);
        rtcSetGeometryOccludedFunction(geom, embree_occluded<Float, Spectrum>);
        rtcCommitGeometry(geom);
        return geom;
    } else {
        Throw("embree_geometry() should only be called in CPU mode.");
    }
}
#endif

#if defined(MTS_ENABLE_OPTIX)
MTS_VARIANT void Shape<Float, Spectrum>::optix_geometry() {
    NotImplementedError("optix_geometry");
}
MTS_VARIANT void Shape<Float, Spectrum>::optix_build_input(OptixBuildInput&) const {
    NotImplementedError("optix_build_input");
}
MTS_VARIANT void Shape<Float, Spectrum>::optix_hit_group_data(HitGroupData&) const {
    NotImplementedError("optix_hit_group_data");
}
#endif

MTS_VARIANT typename Shape<Float, Spectrum>::DirectionSample3f
Shape<Float, Spectrum>::sample_direction(const Interaction3f &it,
                                         const Point2f &sample,
                                         Mask active) const {
    MTS_MASK_ARGUMENT(active);

    DirectionSample3f ds(sample_position(it.time, sample, active));
    ds.d = ds.p - it.p;

    Float dist_squared = squared_norm(ds.d);
    ds.dist = sqrt(dist_squared);
    ds.d /= ds.dist;

    Float dp = abs_dot(ds.d, ds.n);
    ds.pdf *= select(neq(dp, 0.f), dist_squared / dp, 0.f);
    ds.object = (const Object *) this;

    return ds;
}

MTS_VARIANT Float Shape<Float, Spectrum>::pdf_direction(const Interaction3f & /*it*/,
                                                        const DirectionSample3f &ds,
                                                        Mask active) const {
    MTS_MASK_ARGUMENT(active);

    Float pdf = pdf_position(ds, active),
           dp = abs_dot(ds.d, ds.n);

    pdf *= select(neq(dp, 0.f), (ds.dist * ds.dist) / dp, 0.f);

    return pdf;
}

MTS_VARIANT std::pair<typename Shape<Float, Spectrum>::Mask, Float>
Shape<Float, Spectrum>::ray_intersect(const Ray3f & /*ray*/, Float * /*cache*/,
                                      Mask /*active*/) const {
    NotImplementedError("ray_intersect");
}

MTS_VARIANT typename Shape<Float, Spectrum>::Mask Shape<Float, Spectrum>::ray_test(const Ray3f &ray,
                                                                                   Mask active) const {
    MTS_MASK_ARGUMENT(active);

    Float unused[MTS_KD_INTERSECTION_CACHE_SIZE];
    return ray_intersect(ray, unused).first;
}

MTS_VARIANT void Shape<Float, Spectrum>::fill_surface_interaction(const Ray3f & /*ray*/,
                                                                  const Float * /*cache*/,
                                                                  SurfaceInteraction3f & /*si*/,
                                                                  Mask /*active*/) const {
    NotImplementedError("fill_surface_interaction");
}

MTS_VARIANT typename Shape<Float, Spectrum>::SurfaceInteraction3f
Shape<Float, Spectrum>::ray_intersect(const Ray3f &ray, Mask active) const {
    MTS_MASK_ARGUMENT(active);

    SurfaceInteraction3f si = zero<SurfaceInteraction3f>();
    Float cache[MTS_KD_INTERSECTION_CACHE_SIZE];
    auto [success, t] = ray_intersect(ray, cache, active);
    active &= success;
    si.t = select(active, t,  math::Infinity<Float>);

    if (any(active))
        fill_surface_interaction(ray, cache, si, active);
    return si;
}

MTS_VARIANT std::pair<typename Shape<Float, Spectrum>::Vector3f, typename Shape<Float, Spectrum>::Vector3f>
Shape<Float, Spectrum>::normal_derivative(const SurfaceInteraction3f & /*si*/,
                                          bool /*shading_frame*/, Mask /*active*/) const {
    NotImplementedError("normal_derivative");
}

MTS_VARIANT typename Shape<Float, Spectrum>::UnpolarizedSpectrum
Shape<Float, Spectrum>::eval_attribute(const std::string & /*name*/,
                                       const SurfaceInteraction3f & /*si*/,
                                       Mask /*active*/) const {
    NotImplementedError("eval_attribute");
}

MTS_VARIANT Float
Shape<Float, Spectrum>::eval_attribute_1(const std::string& /*name*/,
                                         const SurfaceInteraction3f &/*si*/,
                                         Mask /*active*/) const {
    NotImplementedError("eval_attribute_1");
}
MTS_VARIANT typename Shape<Float, Spectrum>::Color3f
Shape<Float, Spectrum>::eval_attribute_3(const std::string& /*name*/,
                                         const SurfaceInteraction3f &/*si*/,
                                         Mask /*active*/) const {
    NotImplementedError("eval_attribute_3");
}

MTS_VARIANT typename Shape<Float, Spectrum>::ScalarFloat
Shape<Float, Spectrum>::surface_area() const {
    NotImplementedError("surface_area");
}

MTS_VARIANT typename Shape<Float, Spectrum>::ScalarBoundingBox3f
Shape<Float, Spectrum>::bbox(ScalarIndex) const {
    return bbox();
}

MTS_VARIANT typename Shape<Float, Spectrum>::ScalarBoundingBox3f
Shape<Float, Spectrum>::bbox(ScalarIndex index, const ScalarBoundingBox3f &clip) const {
    ScalarBoundingBox3f result = bbox(index);
    result.clip(clip);
    return result;
}

MTS_VARIANT typename Shape<Float, Spectrum>::ScalarSize
Shape<Float, Spectrum>::primitive_count() const {
    return 1;
}

MTS_VARIANT typename Shape<Float, Spectrum>::ScalarSize
Shape<Float, Spectrum>::effective_primitive_count() const {
    return primitive_count();
}

MTS_VARIANT void Shape<Float, Spectrum>::traverse(TraversalCallback *callback) {
    if (m_bsdf)
        callback->put_object("bsdf", m_bsdf.get());
    if (m_emitter)
        callback->put_object("emitter", m_emitter.get());
    if (m_sensor)
        callback->put_object("sensor", m_sensor.get());
    if (m_interior_medium)
        callback->put_object("interior_medium", m_interior_medium.get());
    if (m_exterior_medium)
        callback->put_object("exterior_medium", m_exterior_medium.get());
}

MTS_VARIANT void Shape<Float, Spectrum>::parameters_changed(const std::vector<std::string> &/*keys*/) { }

MTS_VARIANT void Shape<Float, Spectrum>::set_children() {
    if (m_emitter)
        m_emitter->set_shape(this);
    if (m_sensor)
        m_sensor->set_shape(this);
};

MTS_VARIANT std::string Shape<Float, Spectrum>::get_children_string() const {
    std::vector<std::pair<std::string, const Object*>> children;
    if (m_bsdf) children.push_back({ "bsdf", m_bsdf });
    if (m_emitter) children.push_back({ "emitter", m_emitter });
    if (m_sensor) children.push_back({ "sensor", m_sensor });
    if (m_interior_medium) children.push_back({ "interior_medium", m_interior_medium });
    if (m_exterior_medium) children.push_back({ "exterior_medium", m_exterior_medium });

    std::ostringstream oss;
    size_t i = 0;
    for (const auto& [name, child]: children)
        oss << name << " = " << child << (++i < children.size() ? ",\n" : "");

    return oss.str();
}

MTS_IMPLEMENT_CLASS_VARIANT(Shape, Object, "shape")
MTS_INSTANTIATE_CLASS(Shape)
NAMESPACE_END(mitsuba)
