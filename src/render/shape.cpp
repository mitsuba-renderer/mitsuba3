#include <mitsuba/core/properties.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/core/plugin.h>

#if defined(MI_ENABLE_EMBREE)
#  include <embree3/rtcore.h>
#else
#  include <mitsuba/render/kdtree.h>
#endif

#if defined(MI_ENABLE_CUDA)
#  include <mitsuba/render/optix/shapes.h>
#endif

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Shape<Float, Spectrum>::Shape(const Properties &props) : m_id(props.id()) {
    m_to_world =
        (ScalarTransform4f) props.get<ScalarTransform4f>("to_world", ScalarTransform4f());
    m_to_object = m_to_world.scalar().inverse();

    for (auto &[name, obj] : props.objects(false)) {
        Emitter *emitter = dynamic_cast<Emitter *>(obj.get());
        Sensor *sensor   = dynamic_cast<Sensor *>(obj.get());
        BSDF *bsdf       = dynamic_cast<BSDF *>(obj.get());
        Medium *medium   = dynamic_cast<Medium *>(obj.get());
        Texture *texture = dynamic_cast<Texture *>(obj.get());

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
        } else if (texture) {
            m_texture_attributes.insert({ name, texture });
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

    m_silhouette_sampling_weight = props.get<ScalarFloat>("silhouette_sampling_weight", 1.0f);

    if constexpr (dr::is_jit_v<Float>)
        jit_registry_put(dr::backend_v<Float>, "mitsuba::Shape", this);
}

MI_VARIANT Shape<Float, Spectrum>::~Shape() {
#if defined(MI_ENABLE_CUDA)
    if constexpr (dr::is_cuda_v<Float>)
        jit_free(m_optix_data_ptr);
#endif

    if constexpr (dr::is_jit_v<Float>)
        jit_registry_remove(this);
}

MI_VARIANT typename Shape<Float, Spectrum>::PositionSample3f
Shape<Float, Spectrum>::sample_position(Float /*time*/, const Point2f & /*sample*/,
                                        Mask /*active*/) const {
    NotImplementedError("sample_position");
}

MI_VARIANT Float Shape<Float, Spectrum>::pdf_position(const PositionSample3f & /*ps*/, Mask /*active*/) const {
    NotImplementedError("pdf_position");
}

#if defined(MI_ENABLE_EMBREE)
template <typename Float, typename Spectrum>
void embree_bbox(const struct RTCBoundsFunctionArguments* args) {
    MI_IMPORT_TYPES(Shape)
    const Shape* shape = (const Shape*) args->geometryUserPtr;
    ScalarBoundingBox3f bbox = shape->bbox(args->primID);
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
                             unsigned int primID,
                             RTCRay* rtc_ray,
                             RTCHit* rtc_hit) {
    MI_IMPORT_TYPES(Shape)

    const Shape* shape = (const Shape*) geometryUserPtr;

    if (!valid[0])
        return;

    // Create a Mitsuba ray
    Ray3f ray = dr::zeros<Ray3f>();
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
        PreliminaryIntersection3f pi = shape->ray_intersect_preliminary(ray, primID, true);
        if (dr::all(pi.is_valid())) {
            rtc_ray->tfar      = (float) dr::slice(pi.t);
            rtc_hit->u         = (float) dr::slice(pi.prim_uv.x());
            rtc_hit->v         = (float) dr::slice(pi.prim_uv.y());
            rtc_hit->geomID    = geomID;
            rtc_hit->primID    = primID;
            rtc_hit->instID[0] = instID;
#if !defined(NDEBUG)
            rtc_hit->Ng_x      = 0.f;
            rtc_hit->Ng_y      = 0.f;
            rtc_hit->Ng_z      = 0.f;
#endif
        }
    } else {
        if (dr::all(shape->ray_test(ray, primID, true)))
            rtc_ray->tfar = -dr::Infinity<float>;
    }
}

template <typename Float, typename Spectrum, size_t N, typename RTCRay_, typename RTCHit_>
static void embree_intersect_packet(int *valid, void *geometryUserPtr,
                                    unsigned int geomID,
                                    unsigned int instID,
                                    unsigned int primID,
                                    RTCRay_ *rtc_ray,
                                    RTCHit_ *rtc_hit) {
    MI_IMPORT_TYPES(Shape)

    using FloatP   = dr::Packet<dr::scalar_t<Float>, N>;
    using MaskP    = dr::mask_t<FloatP>;
    using Point2fP = Point<FloatP, 2>;
    using Point3fP = Point<FloatP, 3>;
    using Ray3fP   = Ray<Point<FloatP, 3>, Spectrum>;
    using UInt32P  = dr::uint32_array_t<FloatP>;
    using Float32P = dr::Packet<dr::scalar_t<Float32>, N>;

    const Shape* shape = (const Shape*) geometryUserPtr;

    MaskP active = dr::load_aligned<UInt32P>(valid) != 0;
    if (dr::none(active))
        return;

    // Create Mitsuba ray
    Ray3fP ray;
    ray.o.x() = dr::load_aligned<Float32P>(rtc_ray->org_x);
    ray.o.y() = dr::load_aligned<Float32P>(rtc_ray->org_y);
    ray.o.z() = dr::load_aligned<Float32P>(rtc_ray->org_z);
    ray.d.x() = dr::load_aligned<Float32P>(rtc_ray->dir_x);
    ray.d.y() = dr::load_aligned<Float32P>(rtc_ray->dir_y);
    ray.d.z() = dr::load_aligned<Float32P>(rtc_ray->dir_z);
    ray.time  = dr::load_aligned<Float32P>(rtc_ray->time);

    Float32P tnear = dr::load_aligned<Float32P>(rtc_ray->tnear),
             tfar  = dr::load_aligned<Float32P>(rtc_ray->tfar);
    ray.o += ray.d * tnear;
    ray.maxt = tfar - tnear;

    // Check whether this is a shadow ray or not
    if (rtc_hit) {
        auto [t, prim_uv, s_idx, p_idx] = shape->ray_intersect_preliminary_packet(ray, primID, active);
        active &= (t != dr::Infinity<Float>);
        dr::store_aligned(rtc_ray->tfar,      Float32P(dr::select(active, t,           ray.maxt)));
        dr::store_aligned(rtc_hit->u,         Float32P(dr::select(active, prim_uv.x(), dr::load_aligned<Float32P>(rtc_hit->u))));
        dr::store_aligned(rtc_hit->v,         Float32P(dr::select(active, prim_uv.y(), dr::load_aligned<Float32P>(rtc_hit->v))));
        dr::store_aligned(rtc_hit->geomID,    dr::select(active, UInt32P(geomID), dr::load_aligned<UInt32P>(rtc_hit->geomID)));
        dr::store_aligned(rtc_hit->primID,    dr::select(active, UInt32P(primID), dr::load_aligned<UInt32P>(rtc_hit->primID)));
        dr::store_aligned(rtc_hit->instID[0], dr::select(active, UInt32P(instID), dr::load_aligned<UInt32P>(rtc_hit->instID[0])));
    } else {
        active &= shape->ray_test_packet(ray, primID, active);
        dr::store_aligned(rtc_ray->tfar, Float32P(dr::select(active, -dr::Infinity<Float>, tfar)));
    }
}

template <typename Float, typename Spectrum>
void embree_intersect(const RTCIntersectFunctionNArguments* args) {
    switch (args->N) {
        case 1:
            embree_intersect_scalar<Float, Spectrum>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0], args->primID,
                &((RTCRayHit *) args->rayhit)->ray,
                &((RTCRayHit *) args->rayhit)->hit);
            break;

        case 4:
            embree_intersect_packet<Float, Spectrum, 4>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0], args->primID,
                &((RTCRayHit4 *) args->rayhit)->ray,
                &((RTCRayHit4 *) args->rayhit)->hit);
            break;

        case 8:
            embree_intersect_packet<Float, Spectrum, 8>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0], args->primID,
                &((RTCRayHit8 *) args->rayhit)->ray,
                &((RTCRayHit8 *) args->rayhit)->hit);
            break;

        case 16:
            embree_intersect_packet<Float, Spectrum, 16>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0], args->primID,
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
                args->context->instID[0], args->primID,
                (RTCRay *) args->ray,
                (RTCHit *) nullptr);
            break;

        case 4:
            embree_intersect_packet<Float, Spectrum, 4>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0], args->primID,
                (RTCRay4 *) args->ray,
                (RTCHit4 *) nullptr);
            break;

        case 8:
            embree_intersect_packet<Float, Spectrum, 8>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0], args->primID,
                (RTCRay8 *) args->ray,
                (RTCHit8 *) nullptr);
            break;

        case 16:
            embree_intersect_packet<Float, Spectrum, 16>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0], args->primID,
                (RTCRay16 *) args->ray,
                (RTCHit16 *) nullptr);
            break;

        default:
            Throw("embree_occluded(): unsupported packet size!");
    }
}

MI_VARIANT RTCGeometry Shape<Float, Spectrum>::embree_geometry(RTCDevice device) {
    if constexpr (!dr::is_cuda_v<Float>) {
        RTCGeometry geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_USER);
        rtcSetGeometryUserPrimitiveCount(geom, primitive_count());
        rtcSetGeometryUserData(geom, (void *) this);
        rtcSetGeometryBoundsFunction(geom, embree_bbox<Float, Spectrum>, nullptr);
        rtcSetGeometryIntersectFunction(geom, embree_intersect<Float, Spectrum>);
        rtcSetGeometryOccludedFunction(geom, embree_occluded<Float, Spectrum>);
        rtcCommitGeometry(geom);
        return geom;
    } else {
        DRJIT_MARK_USED(device);
        Throw("embree_geometry() should only be called in CPU mode.");
    }
}
#endif

#if defined(MI_ENABLE_CUDA)
static const uint32_t optix_geometry_flags[1] = { OPTIX_GEOMETRY_FLAG_NONE };

MI_VARIANT void Shape<Float, Spectrum>::optix_prepare_geometry() {
    NotImplementedError("optix_prepare_geometry");
}

MI_VARIANT
void Shape<Float, Spectrum>::optix_fill_hitgroup_records(std::vector<HitGroupSbtRecord> &hitgroup_records,
                                                         const OptixProgramGroup *program_groups) {
    optix_prepare_geometry();
    // Set hitgroup record data
    hitgroup_records.push_back(HitGroupSbtRecord());
    hitgroup_records.back().data = {
        jit_registry_id(this), m_optix_data_ptr
    };

    size_t program_group_idx = (is_mesh() ? 1 : 2 + get_shape_descr_idx(this));
    // Setup the hitgroup record and copy it to the hitgroup records array
    jit_optix_check(optixSbtRecordPackHeader(program_groups[program_group_idx],
                                             &hitgroup_records.back()));
}

MI_VARIANT void Shape<Float, Spectrum>::optix_prepare_ias(const OptixDeviceContext& /*context*/,
                                                           std::vector<OptixInstance>& /*instances*/,
                                                           uint32_t /*instance_id*/,
                                                           const ScalarTransform4f& /*transf*/) {
    NotImplementedError("optix_prepare_ias");
}

MI_VARIANT void Shape<Float, Spectrum>::optix_build_input(OptixBuildInput &build_input) const {
    build_input.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
    // Assumes the aabb is always the first member of the data struct
    build_input.customPrimitiveArray.aabbBuffers   = (CUdeviceptr*) &m_optix_data_ptr;
    build_input.customPrimitiveArray.numPrimitives = 1;
    build_input.customPrimitiveArray.strideInBytes = 6 * sizeof(float);
    build_input.customPrimitiveArray.flags         = optix_geometry_flags;
    build_input.customPrimitiveArray.numSbtRecords = 1;
}
#endif

MI_VARIANT typename Shape<Float, Spectrum>::DirectionSample3f
Shape<Float, Spectrum>::sample_direction(const Interaction3f &it,
                                         const Point2f &sample,
                                         Mask active) const {
    MI_MASK_ARGUMENT(active);

    DirectionSample3f ds(sample_position(it.time, sample, active));
    ds.d = ds.p - it.p;

    Float dist_squared = dr::squared_norm(ds.d);
    ds.dist = dr::sqrt(dist_squared);
    ds.d /= ds.dist;

    Float dp = dr::abs_dot(ds.d, ds.n);
    Float x = dist_squared / dp;
    ds.pdf *= dr::select(dr::isfinite(x), x, 0.f);

    return ds;
}

MI_VARIANT Float Shape<Float, Spectrum>::pdf_direction(const Interaction3f & /*it*/,
                                                        const DirectionSample3f &ds,
                                                        Mask active) const {
    MI_MASK_ARGUMENT(active);

    Float pdf = pdf_position(ds, active),
           dp = dr::abs_dot(ds.d, ds.n);

    pdf *= dr::select(dp != 0.f, (ds.dist * ds.dist) / dp, 0.f);

    return pdf;
}

MI_VARIANT typename Shape<Float, Spectrum>::SilhouetteSample3f
Shape<Float, Spectrum>::sample_silhouette(const Point3f & /*sample*/,
                                          uint32_t /*type*/,
                                          Mask /*active*/) const {
    if constexpr (dr::is_jit_v<Float>)
        return dr::zeros<SilhouetteSample3f>();
    else
        NotImplementedError("sample_silhouette");
}

MI_VARIANT typename Shape<Float, Spectrum>::Point3f
Shape<Float, Spectrum>::invert_silhouette_sample(const SilhouetteSample3f & /*ss*/,
                                                 Mask /*active*/) const {
    if constexpr (dr::is_jit_v<Float>)
        return dr::zeros<Point3f>();
    else
        NotImplementedError("invert_silhouette_sample");
}

MI_VARIANT typename Shape<Float, Spectrum>::Point3f
Shape<Float, Spectrum>::differential_motion(const SurfaceInteraction3f &si,
                                            Mask /*active*/) const {
    return dr::detach(si.p);
}

MI_VARIANT typename Shape<Float, Spectrum>::SilhouetteSample3f
Shape<Float, Spectrum>::primitive_silhouette_projection(const Point3f &,
                                                        const SurfaceInteraction3f &,
                                                        uint32_t,
                                                        Float,
                                                        Mask) const {
    return dr::zeros<SilhouetteSample3f>();
}

MI_VARIANT
std::tuple<DynamicBuffer<typename CoreAliases<Float>::UInt32>,
           DynamicBuffer<Float>>
Shape<Float, Spectrum>::precompute_silhouette(
    const ScalarPoint3f & /*viewpoint*/) const {
    NotImplementedError("precompute_silhouette");
}

MI_VARIANT typename Shape<Float, Spectrum>::SilhouetteSample3f
Shape<Float, Spectrum>::sample_precomputed_silhouette(
    const Point3f & /*viewpoint*/, Index /*sample1*/, Float /*sample2*/,
    Mask /*active*/) const {
    if constexpr (dr::is_jit_v<Float>)
        return dr::zeros<SilhouetteSample3f>();
    else
        NotImplementedError("sample_precomputed_silhouette");
}

MI_VARIANT typename Shape<Float, Spectrum>::PreliminaryIntersection3f
Shape<Float, Spectrum>::ray_intersect_preliminary(const Ray3f & /*ray*/,
                                                  uint32_t /*prim_index*/, Mask /*active*/) const {
    NotImplementedError("ray_intersect_preliminary");
}

MI_VARIANT
std::tuple<typename Shape<Float, Spectrum>::ScalarFloat,
           typename Shape<Float, Spectrum>::ScalarPoint2f,
           typename Shape<Float, Spectrum>::ScalarUInt32,
           typename Shape<Float, Spectrum>::ScalarUInt32>
Shape<Float, Spectrum>::ray_intersect_preliminary_scalar(const ScalarRay3f & /*ray*/) const {
    NotImplementedError("ray_intersect_preliminary_scalar");
}

#define MI_DEFAULT_RAY_INTERSECT_PACKET(N)                                                          \
    MI_VARIANT std::tuple<typename Shape<Float, Spectrum>::FloatP##N,                               \
                           typename Shape<Float, Spectrum>::Point2fP##N,                            \
                           typename Shape<Float, Spectrum>::UInt32P##N,                             \
                           typename Shape<Float, Spectrum>::UInt32P##N>                             \
    Shape<Float, Spectrum>::ray_intersect_preliminary_packet(                                       \
        const Ray3fP##N & /*ray*/, uint32_t /*prim_index*/, MaskP##N /*active*/) const {            \
        NotImplementedError("ray_intersect_preliminary_packet");                                    \
    }                                                                                               \
    MI_VARIANT typename Shape<Float, Spectrum>::MaskP##N                                            \
    Shape<Float, Spectrum>::ray_test_packet(const Ray3fP##N &ray,                                   \
                                            uint32_t prim_index,                                    \
                                            MaskP##N active) const {                                \
        auto res = ray_intersect_preliminary_packet(ray, prim_index, active);                       \
        return std::get<0>(res) != dr::Infinity<Float>;                                             \
    }

MI_DEFAULT_RAY_INTERSECT_PACKET(4)
MI_DEFAULT_RAY_INTERSECT_PACKET(8)
MI_DEFAULT_RAY_INTERSECT_PACKET(16)

MI_VARIANT typename Shape<Float, Spectrum>::Mask
Shape<Float, Spectrum>::ray_test(const Ray3f &ray, uint32_t prim_index, Mask active) const {
    MI_MASK_ARGUMENT(active);
    return ray_intersect_preliminary(ray, prim_index, active).is_valid();
}

MI_VARIANT
bool Shape<Float, Spectrum>::ray_test_scalar(const ScalarRay3f & /*ray*/) const {
    NotImplementedError("ray_intersect_test_scalar");
}

MI_VARIANT typename Shape<Float, Spectrum>::SurfaceInteraction3f
Shape<Float, Spectrum>::compute_surface_interaction(const Ray3f & /*ray*/,
                                                    const PreliminaryIntersection3f &/*pi*/,
                                                    uint32_t /*ray_flags*/,
                                                    uint32_t /*recursion_depth*/,
                                                    Mask /*active*/) const {
    NotImplementedError("compute_surface_interaction");
}

MI_VARIANT typename Shape<Float, Spectrum>::SurfaceInteraction3f
Shape<Float, Spectrum>::ray_intersect(const Ray3f &ray, uint32_t ray_flags, Mask active) const {
    MI_MASK_ARGUMENT(active);
    auto pi = ray_intersect_preliminary(ray, 0, active);
    return pi.compute_surface_interaction(ray, ray_flags, active);
}

MI_VARIANT typename Shape<Float, Spectrum>::Mask
Shape<Float, Spectrum>::has_attribute(const std::string& name, Mask /*active*/) const {
    return m_texture_attributes.find(name) != m_texture_attributes.end();
}

MI_VARIANT typename Shape<Float, Spectrum>::UnpolarizedSpectrum
Shape<Float, Spectrum>::eval_attribute(const std::string & name,
                                       const SurfaceInteraction3f & si,
                                       Mask active) const {
    const auto& it = m_texture_attributes.find(name);
    if (it == m_texture_attributes.end()) {
        if constexpr (dr::is_jit_v<Float>)
            return 0.f;
        else
            Throw("Invalid attribute requested %s.", name.c_str());
    }

    const auto& texture = it->second;
    return texture->eval(si, active);
}

MI_VARIANT Float
Shape<Float, Spectrum>::eval_attribute_1(const std::string& name,
                                         const SurfaceInteraction3f &si,
                                         Mask active) const {
    const auto& it = m_texture_attributes.find(name);
    if (it == m_texture_attributes.end()) {
        if constexpr (dr::is_jit_v<Float>)
            return 0.f;
        else
            Throw("Invalid attribute requested %s.", name.c_str());
    }

    const auto& texture = it->second;
    return texture->eval_1(si, active);
}

MI_VARIANT typename Shape<Float, Spectrum>::Color3f
Shape<Float, Spectrum>::eval_attribute_3(const std::string& name,
                                         const SurfaceInteraction3f &si,
                                         Mask active) const {
    const auto& it = m_texture_attributes.find(name);
    if (it == m_texture_attributes.end()) {
        if constexpr (dr::is_jit_v<Float>)
            return 0.f;
        else
            Throw("Invalid attribute requested %s.", name.c_str());
    }

    const auto& texture = it->second;
    return texture->eval_3(si, active);
}

MI_VARIANT Float Shape<Float, Spectrum>::surface_area() const {
    NotImplementedError("surface_area");
}

MI_VARIANT typename Shape<Float, Spectrum>::ScalarBoundingBox3f
Shape<Float, Spectrum>::bbox(ScalarIndex) const {
    return bbox();
}

MI_VARIANT typename Shape<Float, Spectrum>::ScalarBoundingBox3f
Shape<Float, Spectrum>::bbox(ScalarIndex index, const ScalarBoundingBox3f &clip) const {
    ScalarBoundingBox3f result = bbox(index);
    result.clip(clip);
    return result;
}

MI_VARIANT typename Shape<Float, Spectrum>::ScalarSize
Shape<Float, Spectrum>::primitive_count() const {
    return 1;
}

MI_VARIANT typename Shape<Float, Spectrum>::ScalarSize
Shape<Float, Spectrum>::effective_primitive_count() const {
    return primitive_count();
}

MI_VARIANT void Shape<Float, Spectrum>::traverse(TraversalCallback *callback) {
    callback->put_object("bsdf", m_bsdf.get(), +ParamFlags::Differentiable);
    if (m_emitter)
        callback->put_object("emitter",         m_emitter.get(),         +ParamFlags::Differentiable);
    if (m_sensor)
        callback->put_object("sensor",          m_sensor.get(),          +ParamFlags::Differentiable);
    if (m_interior_medium)
        callback->put_object("interior_medium", m_interior_medium.get(), +ParamFlags::Differentiable);
    if (m_exterior_medium)
        callback->put_object("exterior_medium", m_exterior_medium.get(), +ParamFlags::Differentiable);

    callback->put_parameter("silhouette_sampling_weight", m_silhouette_sampling_weight, +ParamFlags::NonDifferentiable);
}

MI_VARIANT
void Shape<Float, Spectrum>::parameters_changed(const std::vector<std::string> &/*keys*/) {
    if (dirty()) {
        if constexpr (dr::is_jit_v<Float>) {
            bool is_bspline_curve = (shape_type() == +ShapeType::BSplineCurve);
            bool is_linear_curve = (shape_type() == +ShapeType::LinearCurve);
            if (!is_mesh() && !is_bspline_curve && !is_linear_curve) // to_world/to_object is used
                dr::make_opaque(m_to_world, m_to_object);
        }

        if (m_emitter)
            m_emitter->parameters_changed({"parent"});
        if (m_sensor)
            m_sensor->parameters_changed({"parent"});

#if defined(MI_ENABLE_CUDA)
        if constexpr (dr::is_cuda_v<Float>)
            optix_prepare_geometry();
#endif
    }
}

MI_VARIANT bool Shape<Float, Spectrum>::parameters_grad_enabled() const {
    return false;
}

MI_VARIANT void Shape<Float, Spectrum>::initialize() {
    if constexpr (dr::is_jit_v<Float>) {
        bool is_bspline_curve = (shape_type() == +ShapeType::BSplineCurve);
        bool is_linear_curve = (shape_type() == +ShapeType::LinearCurve);
        if (!is_mesh() && !is_bspline_curve && !is_linear_curve) // to_world/to_object is not used
            dr::make_opaque(m_to_world, m_to_object);
    }

    // Explicitly register this shape as the parent of the provided sub-objects
    if (m_emitter)
        m_emitter->set_shape(this);
    if (m_sensor)
        m_sensor->set_shape(this);

    m_initialized = true;
}

MI_VARIANT
typename Shape<Float, Spectrum>::SurfaceInteraction3f
Shape<Float, Spectrum>::eval_parameterization(const Point2f &, uint32_t, Mask) const {
    NotImplementedError("eval_parameterization");
}

MI_VARIANT std::string Shape<Float, Spectrum>::get_children_string() const {
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

MI_IMPLEMENT_CLASS_VARIANT(Shape, Object, "shape")
MI_INSTANTIATE_CLASS(Shape)
NAMESPACE_END(mitsuba)
