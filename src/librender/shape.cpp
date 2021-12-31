
#include <mitsuba/core/properties.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/core/plugin.h>

#if defined(MTS_ENABLE_EMBREE)
#  include <embree3/rtcore.h>
#else
#  include <mitsuba/render/kdtree.h>
#endif

#if defined(MTS_ENABLE_CUDA)
#  include <mitsuba/render/optix/shapes.h>
#endif

NAMESPACE_BEGIN(mitsuba)

#if defined(MTS_ENABLE_EMBREE)
#if defined(ENOKI_DISABLE_VECTORIZATION)
#  define MTS_RAY_WIDTH 1
#elif defined(__AVX512F__)
#  define MTS_RAY_WIDTH 16
#elif defined(__AVX2__)
#  define MTS_RAY_WIDTH 8
#elif defined(__SSE4_2__) || defined(__ARM_NEON)
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
    m_to_world =
        (ScalarTransform4f) props.get<ScalarTransform4f>("to_world", ScalarTransform4f());
    m_to_object = m_to_world.scalar().inverse();

    for (auto &[name, obj] : props.objects(false)) {
        Emitter *emitter = dynamic_cast<Emitter *>(obj.get());
        Sensor *sensor   = dynamic_cast<Sensor *>(obj.get());
        BSDF *bsdf       = dynamic_cast<BSDF *>(obj.get());
        Medium *medium   = dynamic_cast<Medium *>(obj.get());

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
            props2.set_float("reflectance", 0);
        m_bsdf = PluginManager::instance()->create_object<BSDF>(props2);
    }

    ek::set_attr(this, "emitter", m_emitter.get());
    ek::set_attr(this, "sensor", m_sensor.get());
    ek::set_attr(this, "bsdf", m_bsdf.get());
    ek::set_attr(this, "interior_medium", m_interior_medium.get());
    ek::set_attr(this, "exterior_medium", m_exterior_medium.get());
}

MTS_VARIANT Shape<Float, Spectrum>::~Shape() {
#if defined(MTS_ENABLE_CUDA)
    if constexpr (ek::is_cuda_array_v<Float>)
        jit_free(m_optix_data_ptr);
#endif
}

MTS_VARIANT bool Shape<Float, Spectrum>::is_mesh() const {
    return class_()->derives_from(Mesh<Float, Spectrum>::m_class);
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
    bounds_o->lower_x = (float) bbox.min.x();
    bounds_o->lower_y = (float) bbox.min.y();
    bounds_o->lower_z = (float) bbox.min.z();
    bounds_o->upper_x = (float) bbox.max.x();
    bounds_o->upper_y = (float) bbox.max.y();
    bounds_o->upper_z = (float) bbox.max.z();
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
    Ray3f ray = ek::zero<Ray3f>();
    ray.o.x() = rtc_ray->org_x;
    ray.o.y() = rtc_ray->org_y;
    ray.o.z() = rtc_ray->org_z;
    ray.d.x() = rtc_ray->dir_x;
    ray.d.y() = rtc_ray->dir_y;
    ray.d.z() = rtc_ray->dir_z;
    ray.time  = rtc_ray->time;

    ray.o += ray.d * rtc_ray->tnear;
    ray.maxt = rtc_ray->tfar - rtc_ray->tnear;

    // Check whether this is a shadow ray or not
    if (rtc_hit) {
        PreliminaryIntersection3f pi = shape->ray_intersect_preliminary(ray);
        if (ek::all(pi.is_valid())) {
            rtc_ray->tfar      = (float) ek::slice(pi.t);
            rtc_hit->u         = (float) ek::slice(pi.prim_uv.x());
            rtc_hit->v         = (float) ek::slice(pi.prim_uv.y());
            rtc_hit->geomID    = geomID;
            rtc_hit->primID    = 0;
            rtc_hit->instID[0] = instID;
        }
    } else {
        if (ek::all(shape->ray_test(ray)))
            rtc_ray->tfar = -ek::Infinity<float>;
    }
}

template <typename Float, typename Spectrum, size_t N, typename RTCRay_, typename RTCHit_>
static void embree_intersect_packet(int *valid, void *geometryUserPtr,
                                    unsigned int geomID, unsigned int instID,
                                    RTCRay_ *rtc_ray,
                                    RTCHit_ *rtc_hit) {
    MTS_IMPORT_TYPES(Shape)

    using FloatP   = ek::Packet<ek::scalar_t<Float>, N>;
    using MaskP    = ek::mask_t<FloatP>;
    using Point2fP = Point<FloatP, 2>;
    using Point3fP = Point<FloatP, 3>;
    using Ray3fP   = Ray<Point<FloatP, 3>, Spectrum>;
    using UInt32P  = ek::uint32_array_t<FloatP>;
    using Float32P = ek::Packet<ek::scalar_t<Float32>, N>;

    const Shape* shape = (const Shape*) geometryUserPtr;

    MaskP active = ek::neq(ek::load_aligned<UInt32P>(valid), 0);
    if (ek::none(active))
        return;

    // Create Mitsuba ray
    Ray3fP ray;
    ray.o.x() = ek::load_aligned<Float32P>(rtc_ray->org_x);
    ray.o.y() = ek::load_aligned<Float32P>(rtc_ray->org_y);
    ray.o.z() = ek::load_aligned<Float32P>(rtc_ray->org_z);
    ray.d.x() = ek::load_aligned<Float32P>(rtc_ray->dir_x);
    ray.d.y() = ek::load_aligned<Float32P>(rtc_ray->dir_y);
    ray.d.z() = ek::load_aligned<Float32P>(rtc_ray->dir_z);
    ray.time  = ek::load_aligned<Float32P>(rtc_ray->time);

    Float32P tnear = ek::load_aligned<Float32P>(rtc_ray->tnear),
             tfar  = ek::load_aligned<Float32P>(rtc_ray->tfar);
    ray.o += ray.d * tnear;
    ray.maxt = tfar - tnear;

    // Check whether this is a shadow ray or not
    if (rtc_hit) {
        auto [t, prim_uv, s_idx, p_idx] = shape->ray_intersect_preliminary_packet(ray, active);
        active &= ek::neq(t, ek::Infinity<Float>);
        ek::store_aligned(rtc_ray->tfar,      Float32P(ek::select(active, t,           ray.maxt)));
        ek::store_aligned(rtc_hit->u,         Float32P(ek::select(active, prim_uv.x(), ek::load_aligned<Float32P>(rtc_hit->u))));
        ek::store_aligned(rtc_hit->v,         Float32P(ek::select(active, prim_uv.y(), ek::load_aligned<Float32P>(rtc_hit->v))));
        ek::store_aligned(rtc_hit->geomID,    ek::select(active, UInt32P(geomID), ek::load_aligned<UInt32P>(rtc_hit->geomID)));
        ek::store_aligned(rtc_hit->primID,    ek::select(active, UInt32P(0),      ek::load_aligned<UInt32P>(rtc_hit->primID)));
        ek::store_aligned(rtc_hit->instID[0], ek::select(active, UInt32P(instID), ek::load_aligned<UInt32P>(rtc_hit->instID[0])));
    } else {
        active &= shape->ray_test_packet(ray, active);
        ek::store_aligned(rtc_ray->tfar, Float32P(ek::select(active, -ek::Infinity<Float>, tfar)));
    }
}

template <typename Float, typename Spectrum>
void embree_intersect(const RTCIntersectFunctionNArguments* args) {
    switch (args->N) {
        case 1:
            embree_intersect_scalar<Float, Spectrum>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0],
                &((RTCRayHit *) args->rayhit)->ray,
                &((RTCRayHit *) args->rayhit)->hit);
            break;

        case 4:
            embree_intersect_packet<Float, Spectrum, 4>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0],
                &((RTCRayHit4 *) args->rayhit)->ray,
                &((RTCRayHit4 *) args->rayhit)->hit);
            break;

        case 8:
            embree_intersect_packet<Float, Spectrum, 8>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0],
                &((RTCRayHit8 *) args->rayhit)->ray,
                &((RTCRayHit8 *) args->rayhit)->hit);
            break;

        case 16:
            embree_intersect_packet<Float, Spectrum, 16>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0],
                &((RTCRayHit16 *) args->rayhit)->ray,
                &((RTCRayHit16 *) args->rayhit)->hit);
            break;

        default:
            Throw("embree_intersect(): unsupported packet size!");
    }
}

template <typename Float, typename Spectrum>
void embree_occluded(const RTCOccludedFunctionNArguments* args) {
    switch (args->N) {
        case 1:
            embree_intersect_scalar<Float, Spectrum>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0],
                (RTCRay *) args->ray,
                (RTCHit *) nullptr);
            break;

        case 4:
            embree_intersect_packet<Float, Spectrum, 4>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0],
                (RTCRay4 *) args->ray,
                (RTCHit4 *) nullptr);
            break;

        case 8:
            embree_intersect_packet<Float, Spectrum, 8>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0],
                (RTCRay8 *) args->ray,
                (RTCHit8 *) nullptr);
            break;

        case 16:
            embree_intersect_packet<Float, Spectrum, 16>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0],
                (RTCRay16 *) args->ray,
                (RTCHit16 *) nullptr);
            break;

        default:
            Throw("embree_occluded(): unsupported packet size!");
    }
}

MTS_VARIANT RTCGeometry Shape<Float, Spectrum>::embree_geometry(RTCDevice device) {
    ENOKI_MARK_USED(device);
    if constexpr (!ek::is_cuda_array_v<Float>) {
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

#if defined(MTS_ENABLE_CUDA)
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
    hitgroup_records.back().data = {
        jit_registry_get_id(JitBackend::CUDA, this), m_optix_data_ptr
    };

    size_t program_group_idx = (is_mesh() ? 1 : 2 + get_shape_descr_idx(this));
    // Setup the hitgroup record and copy it to the hitgroup records array
    jit_optix_check(optixSbtRecordPackHeader(program_groups[program_group_idx],
                                             &hitgroup_records.back()));
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
    build_input.customPrimitiveArray.aabbBuffers   = (CUdeviceptr*) &m_optix_data_ptr;
    build_input.customPrimitiveArray.numPrimitives = 1;
    build_input.customPrimitiveArray.strideInBytes = 6 * sizeof(float);
    build_input.customPrimitiveArray.flags         = optix_geometry_flags;
    build_input.customPrimitiveArray.numSbtRecords = 1;
}
#endif

MTS_VARIANT typename Shape<Float, Spectrum>::DirectionSample3f
Shape<Float, Spectrum>::sample_direction(const Interaction3f &it,
                                         const Point2f &sample,
                                         Mask active) const {
    MTS_MASK_ARGUMENT(active);

    DirectionSample3f ds(sample_position(it.time, sample, active));
    ds.d = ds.p - it.p;

    Float dist_squared = ek::squared_norm(ds.d);
    ds.dist = ek::sqrt(dist_squared);
    ds.d /= ds.dist;

    Float dp = ek::abs_dot(ds.d, ds.n);
    Float x = dist_squared / dp;
    ds.pdf *= ek::select(ek::isfinite(x), x, 0.f);

    return ds;
}

MTS_VARIANT Float Shape<Float, Spectrum>::pdf_direction(const Interaction3f & /*it*/,
                                                        const DirectionSample3f &ds,
                                                        Mask active) const {
    MTS_MASK_ARGUMENT(active);

    Float pdf = pdf_position(ds, active),
           dp = ek::abs_dot(ds.d, ds.n);

    pdf *= ek::select(ek::neq(dp, 0.f), (ds.dist * ds.dist) / dp, 0.f);

    return pdf;
}

MTS_VARIANT typename Shape<Float, Spectrum>::PreliminaryIntersection3f
Shape<Float, Spectrum>::ray_intersect_preliminary(const Ray3f & /*ray*/, Mask /*active*/) const {
    NotImplementedError("ray_intersect_preliminary");
}

MTS_VARIANT
std::tuple<typename Shape<Float, Spectrum>::ScalarFloat,
           typename Shape<Float, Spectrum>::ScalarPoint2f,
           typename Shape<Float, Spectrum>::ScalarUInt32,
           typename Shape<Float, Spectrum>::ScalarUInt32>
Shape<Float, Spectrum>::ray_intersect_preliminary_scalar(const ScalarRay3f & /*ray*/) const {
    NotImplementedError("ray_intersect_preliminary_scalar");
}

#define MTS_DEFAULT_RAY_INTERSECT_PACKET(N)                                    \
    MTS_VARIANT std::tuple<typename Shape<Float, Spectrum>::FloatP##N,         \
                           typename Shape<Float, Spectrum>::Point2fP##N,       \
                           typename Shape<Float, Spectrum>::UInt32P##N,        \
                           typename Shape<Float, Spectrum>::UInt32P##N>        \
    Shape<Float, Spectrum>::ray_intersect_preliminary_packet(                  \
        const Ray3fP##N & /*ray*/, MaskP##N /*active*/) const {                \
        NotImplementedError("ray_intersect_preliminary_packet");               \
    }                                                                          \
    MTS_VARIANT typename Shape<Float, Spectrum>::MaskP##N                      \
    Shape<Float, Spectrum>::ray_test_packet(const Ray3fP##N &ray,              \
                                            MaskP##N active) const {           \
        auto res = ray_intersect_preliminary_packet(ray, active);              \
        return ek::neq(std::get<0>(res), ek::Infinity<Float>);                 \
    }

MTS_DEFAULT_RAY_INTERSECT_PACKET(4)
MTS_DEFAULT_RAY_INTERSECT_PACKET(8)
MTS_DEFAULT_RAY_INTERSECT_PACKET(16)

MTS_VARIANT typename Shape<Float, Spectrum>::Mask
Shape<Float, Spectrum>::ray_test(const Ray3f &ray, Mask active) const {
    MTS_MASK_ARGUMENT(active);
    return ray_intersect_preliminary(ray, active).is_valid();
}

MTS_VARIANT
bool Shape<Float, Spectrum>::ray_test_scalar(const ScalarRay3f & /*ray*/) const {
    NotImplementedError("ray_intersect_test_scalar");
}

MTS_VARIANT typename Shape<Float, Spectrum>::SurfaceInteraction3f
Shape<Float, Spectrum>::compute_surface_interaction(const Ray3f & /*ray*/,
                                                    const PreliminaryIntersection3f &/*pi*/,
                                                    uint32_t /*ray_flags*/,
                                                    uint32_t /*recursion_depth*/,
                                                    Mask /*active*/) const {
    NotImplementedError("compute_surface_interaction");
}

MTS_VARIANT typename Shape<Float, Spectrum>::SurfaceInteraction3f
Shape<Float, Spectrum>::ray_intersect(const Ray3f &ray, uint32_t ray_flags, Mask active) const {
    MTS_MASK_ARGUMENT(active);
    auto pi = ray_intersect_preliminary(ray, active);
    return pi.compute_surface_interaction(ray, ray_flags, active);
}

MTS_VARIANT
Float Shape<Float, Spectrum>::boundary_test(const Ray3f &/*ray*/,
                                            const SurfaceInteraction3f &/*si*/,
                                            Mask /*active*/) const {
    NotImplementedError("boundary_test");
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

MTS_VARIANT Float Shape<Float, Spectrum>::surface_area() const {
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
    if (dirty()) {
        if constexpr (ek::is_jit_array_v<Float>) {
            if (!is_mesh())
                ek::make_opaque(m_to_world, m_to_object);
        }

        if (m_emitter)
            m_emitter->parameters_changed({"parent"});
        if (m_sensor)
            m_sensor->parameters_changed({"parent"});

#if defined(MTS_ENABLE_CUDA)
        if constexpr (ek::is_cuda_array_v<Float>)
            optix_prepare_geometry();
#endif
    }
}

MTS_VARIANT bool Shape<Float, Spectrum>::parameters_grad_enabled() const {
    return false;
}

MTS_VARIANT void Shape<Float, Spectrum>::initialize() {
    if constexpr (ek::is_jit_array_v<Float>) {
        if (!is_mesh())
            ek::make_opaque(m_to_world, m_to_object);
    }

    // Explicitly register this shape as the parent of the provided sub-objects
    if (m_emitter)
        m_emitter->set_shape(this);
    if (m_sensor)
        m_sensor->set_shape(this);
}

MTS_VARIANT
typename Shape<Float, Spectrum>::SurfaceInteraction3f
Shape<Float, Spectrum>::eval_parameterization(const Point2f &, uint32_t, Mask) const {
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
