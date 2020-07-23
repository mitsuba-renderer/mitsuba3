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
#if defined(MTS_ENABLE_OPTIX)
#  include <mitsuba/render/optix/shapes.h>
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

    for (auto &[name, obj] : props.objects(false)) {
        Emitter *emitter = dynamic_cast<Emitter *>(obj.get());
        Sensor *sensor = dynamic_cast<Sensor *>(obj.get());
        BSDF *bsdf = dynamic_cast<BSDF *>(obj.get());
        Medium *medium = dynamic_cast<Medium *>(obj.get());

        if (emitter) {
            if (m_emitter)
                Throw("Only a single Emitter child object can be specified per shape.");
            m_emitter = emitter;
        } else if (sensor) {
            if (m_sensor)
                Throw("Only a single Sensor child object can be specified per shape.");
            m_sensor = sensor;
        } else if (bsdf) {
            if (m_bsdf)
                Throw("Only a single BSDF child object can be specified per shape.");
            m_bsdf = bsdf;
        } else if (medium) {
            if (name == "interior") {
                if (m_interior_medium)
                    Throw("Only a single interior medium can be specified per shape.");
                m_interior_medium = medium;
            } else if (name == "exterior") {
                if (m_exterior_medium)
                    Throw("Only a single exterior medium can be specified per shape.");
                m_exterior_medium = medium;
            }
        } else {
            continue;
        }

        props.mark_queried(name);
    }

    // Create a default diffuse BSDF if needed.
    if (!m_bsdf) {
        Properties props2("diffuse");
        if (m_emitter)
            props2.set_float("reflectance", 0.f);
        m_bsdf = PluginManager::instance()->create_object<BSDF>(props2);
    }
}

MTS_VARIANT Shape<Float, Spectrum>::~Shape() {
#if defined(MTS_ENABLE_OPTIX)
    if constexpr (is_cuda_array_v<Float>)
        cuda_free(m_optix_data_ptr);
#endif
}

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
                             unsigned int instID,
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
        auto pi = shape->ray_intersect_preliminary(ray);
        if (pi.is_valid()) {
            rtc_ray->tfar = pi.t;
            rtc_hit->u = pi.prim_uv.x();
            rtc_hit->v = pi.prim_uv.y();
            rtc_hit->geomID = geomID;
            rtc_hit->primID = 0;
            rtc_hit->instID[0] = instID;
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
                             unsigned int instID,
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
        auto pi = shape->ray_intersect_preliminary(ray, active);
        active &= pi.is_valid();
        store(rays->tfar,   pi.t, active);
        store(hits->u,      pi.prim_uv.x(), active);
        store(hits->v,      pi.prim_uv.y(), active);
        store(hits->geomID, Int(geomID), active);
        store(hits->primID, Int(0), active);
        store(hits->instID[0], Int(instID), active);
    } else {
        active &= shape->ray_test(ray);
        store(rays->tfar, Float(-math::Infinity<Float>), active);
    }
}

template <typename Float, typename Spectrum>
void embree_intersect(const RTCIntersectFunctionNArguments* args) {
    if constexpr (!is_array_v<Float>) {
        RTCRayHit *rh = (RTCRayHit *) args->rayhit;
        embree_intersect_scalar<Float, Spectrum>(
            args->valid, args->geometryUserPtr, args->geomID,
            args->context->instID[0], (RTCRay *) &rh->ray, (RTCHit *) &rh->hit);
    } else {
        RTCRayHitW *rh = (RTCRayHitW *) args->rayhit;
        embree_intersect_packet<Float, Spectrum>(
            args->valid, args->geometryUserPtr, args->geomID,
            args->context->instID[0], (RTCRayW *) &rh->ray,
            (RTCHitW *) &rh->hit);
    }
}

template <typename Float, typename Spectrum>
void embree_occluded(const RTCOccludedFunctionNArguments* args) {
    if constexpr (!is_array_v<Float>) {
        embree_intersect_scalar<Float, Spectrum>(
            args->valid, args->geometryUserPtr, args->geomID,
            args->context->instID[0], (RTCRay *) args->ray, nullptr);
    } else {
        embree_intersect_packet<Float, Spectrum>(
            args->valid, args->geometryUserPtr, args->geomID,
            args->context->instID[0], (RTCRayW *) args->ray, nullptr);
    }
}

MTS_VARIANT RTCGeometry Shape<Float, Spectrum>::embree_geometry(RTCDevice device) {
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

MTS_VARIANT void Shape<Float, Spectrum>::init_embree_scene(RTCDevice /*device*/){
   NotImplementedError("init_embree_scene");
}

MTS_VARIANT void Shape<Float, Spectrum>::release_embree_scene(){
   NotImplementedError("release_embree_scene");
}
#endif

#if defined(MTS_ENABLE_OPTIX)
static const uint32_t optix_geometry_flags[1] = { OPTIX_GEOMETRY_FLAG_NONE };

MTS_VARIANT void Shape<Float, Spectrum>::optix_prepare_geometry() {
    NotImplementedError("optix_prepare_geometry");
}

MTS_VARIANT
void Shape<Float, Spectrum>::optix_fill_hitgroup_records(std::vector<HitGroupSbtRecord> &hitgroup_records,
                                                         const OptixProgramGroup *program_groups) {
    optix_prepare_geometry();
    // Set hitgroup record data
    hitgroup_records.push_back(HitGroupSbtRecord());
    hitgroup_records.back().data = { (uintptr_t) this, m_optix_data_ptr };

    size_t program_group_idx = (is_mesh() ? 2 : 3 + get_shape_descr_idx(this));
    // Setup the hitgroup record and copy it to the hitgroup records array
    rt_check(optixSbtRecordPackHeader(program_groups[program_group_idx], &hitgroup_records.back()));
}

MTS_VARIANT void Shape<Float, Spectrum>::optix_prepare_ias(const OptixDeviceContext& /*context*/,
                                                           std::vector<OptixInstance>& /*instances*/,
                                                           uint32_t /*instance_id*/,
                                                           const ScalarTransform4f& /*transf*/) {
    NotImplementedError("optix_prepare_ias");
}

MTS_VARIANT void Shape<Float, Spectrum>::optix_build_input(OptixBuildInput &build_input) const {
    build_input.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
    // Assumes the aabb is always the first member of the data struct
    build_input.aabbArray.aabbBuffers   = (CUdeviceptr*) &m_optix_data_ptr;
    build_input.aabbArray.numPrimitives = 1;
    build_input.aabbArray.strideInBytes = sizeof(OptixAabb);
    build_input.aabbArray.flags         = optix_geometry_flags;
    build_input.aabbArray.numSbtRecords = 1;
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

MTS_VARIANT typename Shape<Float, Spectrum>::PreliminaryIntersection3f
Shape<Float, Spectrum>::ray_intersect_preliminary(const Ray3f & /*ray*/, Mask /*active*/) const {
    NotImplementedError("ray_intersect_preliminary");
}

MTS_VARIANT typename Shape<Float, Spectrum>::Mask
Shape<Float, Spectrum>::ray_test(const Ray3f &ray, Mask active) const {
    MTS_MASK_ARGUMENT(active);
    return ray_intersect_preliminary(ray, active).is_valid();
}

MTS_VARIANT typename Shape<Float, Spectrum>::SurfaceInteraction3f
Shape<Float, Spectrum>::compute_surface_interaction(const Ray3f & /*ray*/,
                                                    PreliminaryIntersection3f /*pi*/,
                                                    HitComputeFlags /*flags*/,
                                                    Mask /*active*/) const {
    NotImplementedError("compute_surface_interaction");
}

MTS_VARIANT typename Shape<Float, Spectrum>::SurfaceInteraction3f
Shape<Float, Spectrum>::ray_intersect(const Ray3f &ray, HitComputeFlags flags, Mask active) const {
    MTS_MASK_ARGUMENT(active);

    auto pi = ray_intersect_preliminary(ray, active);
    active &= pi.is_valid();

    return pi.compute_surface_interaction(ray, flags, active);
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
    callback->put_parameter("to_world", m_to_world);

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

MTS_VARIANT
void Shape<Float, Spectrum>::parameters_changed(const std::vector<std::string> &/*keys*/) {
    if (m_emitter)
        m_emitter->parameters_changed({"parent"});
    if (m_sensor)
        m_sensor->parameters_changed({"parent"});
}

MTS_VARIANT bool Shape<Float, Spectrum>::parameters_grad_enabled() const {
    return false;
}

MTS_VARIANT void Shape<Float, Spectrum>::set_children() {
    if (m_emitter)
        m_emitter->set_shape(this);
    if (m_sensor)
        m_sensor->set_shape(this);
}

MTS_VARIANT
typename Shape<Float, Spectrum>::SurfaceInteraction3f
Shape<Float, Spectrum>::eval_parameterization(const Point2f &, Mask) const {
    NotImplementedError("eval_parameterization");
}

MTS_VARIANT std::string Shape<Float, Spectrum>::get_children_string() const {
    std::vector<std::pair<std::string, const Object*>> children;
    children.push_back({ "bsdf", m_bsdf });
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
