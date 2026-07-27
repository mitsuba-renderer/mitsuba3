#include <mitsuba/core/properties.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/scene_ir.h>
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

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Shape<Float, Spectrum>::Shape(const Properties &props)
    : JitObject<Shape>(props.id()) {
    m_to_world =
        (ScalarAffineTransform4f) props.get<ScalarAffineTransform4f>("to_world", ScalarAffineTransform4f());

    for (auto &prop : props.objects()) {
        if (Emitter *emitter = prop.try_get<Emitter>()) {
            if (m_emitter)
                Throw("Only a single Emitter child object can be specified per shape.");
            m_emitter = emitter;
        } else if (Sensor *sensor = prop.try_get<Sensor>()) {
            if (m_sensor)
                Throw("Only a single Sensor child object can be specified per shape.");
            m_sensor = sensor;
        } else if (BSDF *bsdf = prop.try_get<BSDF>()) {
            if (m_bsdf)
                Throw("Only a single BSDF child object can be specified per shape.");
            m_bsdf = bsdf;
        } else if (Medium *medium = prop.try_get<Medium>()) {
            if (prop.name() == "interior") {
                if (m_interior_medium)
                    Throw("Only a single interior medium can be specified per shape.");
                m_interior_medium = medium;
            } else if (prop.name() == "exterior") {
                if (m_exterior_medium)
                    Throw("Only a single exterior medium can be specified per shape.");
                m_exterior_medium = medium;
            }
        } else if (Texture *texture = prop.try_get<Texture>()) {
            add_texture_attribute(prop.name(), texture);
        }
    }

    // Create a default diffuse BSDF if needed.
    if (!m_bsdf) {
        Properties props2("diffuse");
        if (m_emitter)
            props2.set("reflectance", 0.f);
        m_bsdf = PluginManager::instance()->create_object<BSDF>(props2);
    }

    m_silhouette_sampling_weight = props.get<ScalarFloat>("silhouette_sampling_weight", 1.0f);
}

MI_VARIANT Shape<Float, Spectrum>::~Shape() { }


MI_VARIANT typename Shape<Float, Spectrum>::PositionSample3f
Shape<Float, Spectrum>::sample_position(Float /*time*/, const Point2f & /*sample*/,
                                        Mask /*active*/) const {
    NotImplementedError("sample_position");
}

MI_VARIANT Float Shape<Float, Spectrum>::pdf_position(const PositionSample3f & /*ps*/, Mask /*active*/) const {
    NotImplementedError("pdf_position");
}


MI_VARIANT void
Shape<Float, Spectrum>::describe(ShapeIR &g) const {
    g.kind = ShapeIR::Kind::Custom;
    g.type = m_shape_type;
    g.prim_count = primitive_count();
    g.ctx = this;
#if defined(MI_ENABLE_METAL) || defined(MI_ENABLE_CUDA)
    // Default custom shape: one AABB equal to the shape bounds.
    g.fill_aabbs = [](const void *ctx, void *out) {
        ScalarBoundingBox3f b = static_cast<const Shape *>(ctx)->bbox();
        float *d = (float *) out;
        d[0] = (float) b.min.x(); d[1] = (float) b.min.y(); d[2] = (float) b.min.z();
        d[3] = (float) b.max.x(); d[4] = (float) b.max.y(); d[5] = (float) b.max.z();
    };
#endif
}

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
std::tuple<bool,
           typename Shape<Float, Spectrum>::ScalarFloat,
           typename Shape<Float, Spectrum>::ScalarPoint2f,
           typename Shape<Float, Spectrum>::ScalarUInt32,
           typename Shape<Float, Spectrum>::ScalarUInt32>
Shape<Float, Spectrum>::ray_intersect_preliminary_scalar(const ScalarRay3f & /*ray*/) const {
    NotImplementedError("ray_intersect_preliminary_scalar");
}

#define MI_DEFAULT_RAY_INTERSECT_PACKET(N)                                                          \
    MI_VARIANT std::tuple<typename Shape<Float, Spectrum>::MaskP##N,                                \
                           typename Shape<Float, Spectrum>::FloatP##N,                              \
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
        return std::get<0>(res);                                                                    \
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

MI_VARIANT void
Shape<Float, Spectrum>::add_texture_attribute(std::string_view name, Texture *texture) {
    // Replaces existing attribute with name `name`, if any.
    m_texture_attributes.insert_or_assign(std::string(name), texture);
}

MI_VARIANT typename Shape<Float, Spectrum>::Texture *
Shape<Float, Spectrum>::texture_attribute(std::string_view name) {
    auto it = m_texture_attributes.find(name);
    if (it == m_texture_attributes.end())
        Throw("texture_attribute(): attribute %s doesn't exist.", name);
    return const_cast<Texture*>(it->second.get());
}

MI_VARIANT const typename Shape<Float, Spectrum>::Texture *
Shape<Float, Spectrum>::texture_attribute(std::string_view name) const {
    const auto it = m_texture_attributes.find(name);
    if (it == m_texture_attributes.end())
        Throw("texture_attribute(): attribute %s doesn't exist.", name);
    return it->second.get();
}


MI_VARIANT void
Shape<Float, Spectrum>::remove_attribute(std::string_view name) {
    const auto& it = m_texture_attributes.find(name);
    if (it == m_texture_attributes.end())
        Throw("remove_attribute(): Attribute \"%s\" not found.", name);
    m_texture_attributes.erase(it);
}

MI_VARIANT typename Shape<Float, Spectrum>::Mask
Shape<Float, Spectrum>::has_attribute(std::string_view name, Mask /*active*/) const {
    return m_texture_attributes.find(name) != m_texture_attributes.end();
}

MI_VARIANT typename Shape<Float, Spectrum>::UnpolarizedSpectrum
Shape<Float, Spectrum>::eval_attribute(std::string_view name,
                                       const SurfaceInteraction3f & si,
                                       Mask active) const {
    const auto& it = m_texture_attributes.find(name);
    if (it == m_texture_attributes.end()) {
        if constexpr (dr::is_jit_v<Float>)
            return 0.f;
        else
            Throw("Invalid attribute requested %s.", name);
    }

    const auto& texture = it->second;
    return texture->eval(si, active);
}

MI_VARIANT Float
Shape<Float, Spectrum>::eval_attribute_1(std::string_view name,
                                         const SurfaceInteraction3f &si,
                                         Mask active) const {
    const auto& it = m_texture_attributes.find(name);
    if (it == m_texture_attributes.end()) {
        if constexpr (dr::is_jit_v<Float>)
            return 0.f;
        else
            Throw("Invalid attribute requested %s.", name);
    }

    const auto& texture = it->second;
    return texture->eval_1(si, active);
}

MI_VARIANT typename Shape<Float, Spectrum>::Color3f
Shape<Float, Spectrum>::eval_attribute_3(std::string_view name,
                                         const SurfaceInteraction3f &si,
                                         Mask active) const {
    const auto& it = m_texture_attributes.find(name);
    if (it == m_texture_attributes.end()) {
        if constexpr (dr::is_jit_v<Float>)
            return 0.f;
        else
            Throw("Invalid attribute requested %s.", name);
    }

    const auto& texture = it->second;
    return texture->eval_3(si, active);
}

MI_VARIANT typename dr::DynamicArray<Float>
Shape<Float, Spectrum>::eval_attribute_x(std::string_view /*name*/,
                                         const SurfaceInteraction3f & /*si*/,
                                         Mask /*active*/) const {
    if constexpr (dr::is_jit_v<Float>)
        return 0.f;
    else
        NotImplementedError("eval_attribute_x");
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

MI_VARIANT void
Shape<Float, Spectrum>::set_bsdf(BSDF *bsdf) {
    m_bsdf = bsdf;
}

MI_VARIANT typename Shape<Float, Spectrum>::ScalarSize
Shape<Float, Spectrum>::primitive_count() const {
    return 1;
}

MI_VARIANT typename Shape<Float, Spectrum>::ScalarSize
Shape<Float, Spectrum>::effective_primitive_count() const {
    return primitive_count();
}

MI_VARIANT bool Shape<Float, Spectrum>::has_flipped_normals() const {
    return false;
}

MI_VARIANT void Shape<Float, Spectrum>::traverse(TraversalCallback *cb) {
    cb->put("bsdf", m_bsdf, ParamFlags::Differentiable);
    if (m_emitter)
        cb->put("emitter",         m_emitter,         ParamFlags::Differentiable);
    if (m_sensor)
        cb->put("sensor",          m_sensor,          ParamFlags::Differentiable);
    if (m_interior_medium)
        cb->put("interior_medium", m_interior_medium, ParamFlags::Differentiable);
    if (m_exterior_medium)
        cb->put("exterior_medium", m_exterior_medium, ParamFlags::Differentiable);

    cb->put("silhouette_sampling_weight", m_silhouette_sampling_weight, ParamFlags::NonDifferentiable);

    for (auto it = m_texture_attributes.begin(); it != m_texture_attributes.end(); ++it)
        cb->put(it.key(), it.value(), ParamFlags::Differentiable);
}

MI_VARIANT
void Shape<Float, Spectrum>::parameters_changed(const std::vector<std::string> &/*keys*/) {
    if (dirty()) {
        if constexpr (dr::is_jit_v<Float>) {
            bool is_bspline_curve = shape_type() == +ShapeType::BSplineCurve,
                 is_linear_curve  = shape_type() == +ShapeType::LinearCurve;

            if (!is_mesh() && !is_bspline_curve && !is_linear_curve) // to_world is used
                dr::make_opaque(m_to_world);
        }

        if (m_emitter)
            m_emitter->parameters_changed({"parent"});

        if (m_sensor)
            m_sensor->parameters_changed({"parent"});
    }
}

MI_VARIANT bool Shape<Float, Spectrum>::parameters_grad_enabled() const {
    return false;
}

MI_VARIANT void Shape<Float, Spectrum>::initialize() {
    if constexpr (dr::is_jit_v<Float>) {
        bool is_bspline_curve = shape_type() == +ShapeType::BSplineCurve,
             is_linear_curve  = shape_type() == +ShapeType::LinearCurve;

        if (!is_mesh() && !is_bspline_curve && !is_linear_curve) // to_world is not used
            dr::make_opaque(m_to_world);
    }

    // Explicitly register this shape as the parent of the provided sub-objects
    if (!m_initialized) {
        if (m_emitter)
            m_emitter->set_shape(this);

        if (m_sensor)
            m_sensor->set_shape(this);
    }

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

MI_IMPLEMENT_TRAVERSE_CB(Shape, Object);
MI_INSTANTIATE_CLASS(Shape)
NAMESPACE_END(mitsuba)
