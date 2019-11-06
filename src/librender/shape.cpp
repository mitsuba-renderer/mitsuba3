#include <mitsuba/render/kdtree.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/core/plugin.h>

NAMESPACE_BEGIN(mitsuba)

Shape::Shape(const Properties &props) : m_id(props.id()) {
    for (auto &kv : props.objects()) {
        auto emitter = dynamic_cast<Emitter *>(kv.second.get());
        auto bsdf = dynamic_cast<BSDF *>(kv.second.get());

        if (emitter) {
            if (m_emitter)
                Throw("Only a single Emitter child object can be specified per shape.");
            m_emitter = emitter;
        } else if (bsdf) {
            if (m_bsdf)
                Throw("Only a single BSDF child object can be specified per shape.");
            m_bsdf = bsdf;
        } else {
            Throw("Tried to add an unsupported object of type \"%s\"", kv.second);
        }
    }

    if (!m_bsdf) {
        // Create a default diffuse BSDF
        Properties props2("diffuse");
        props2.set_bool("monochrome", props.bool_("monochrome"));
        props2.mark_queried("monochrome");
        m_bsdf = PluginManager::instance()->create_object<BSDF>(props2);
    }
}

Shape::~Shape() { }

std::string Shape::id() const { return m_id; }

PositionSample3f Shape::sample_position(Float /* time */,
                                        const Point2f& /* sample */) const {
    NotImplementedError("sample_position");
}

PositionSample3fP Shape::sample_position(FloatP /* time */,
                                         const Point2fP& /* sample */,
                                         MaskP /* active */) const {
    NotImplementedError("sample_position_p");
}

#if defined(MTS_ENABLE_AUTODIFF)
PositionSample3fD Shape::sample_position(FloatD /* time */,
                                         const Point2fD& /* sample */,
                                         MaskD /* active */) const {
    NotImplementedError("sample_position_d");
}
#endif

Float Shape::pdf_position(const PositionSample3f& /* ps */) const {
    NotImplementedError("pdf_position");
}

FloatP Shape::pdf_position(const PositionSample3fP& /* ps */, MaskP /* active */) const {
    NotImplementedError("pdf_position_p");
}

#if defined(MTS_USE_EMBREE)
RTCGeometry Shape::embree_geometry(RTCDevice) const {
    NotImplementedError("embree_geometry");
}
#endif

#if defined(MTS_USE_OPTIX)
RTgeometrytriangles Shape::optix_geometry(RTcontext) {
    NotImplementedError("optix_geometry");
}
#endif

#if defined(MTS_ENABLE_AUTODIFF)
FloatD Shape::pdf_position(const PositionSample3fD& /* ps */, MaskD /* active */) const {
    NotImplementedError("pdf_position_d");
}
#endif

template <typename Interaction, typename Value, typename Point2,
          typename Point3, typename DirectionSample>
DirectionSample Shape::sample_direction_fallback(const Interaction &it,
                                                 const Point2 &sample,
                                                 mask_t<Value> active) const {
    DirectionSample ds(sample_position(it.time, sample, active));
    ds.d = ds.p - it.p;

    Value dist_squared = squared_norm(ds.d);
    ds.dist = sqrt(dist_squared);
    ds.d /= ds.dist;

    Value dp = abs_dot(ds.d, ds.n);
    ds.pdf *= select(neq(dp, 0.f), dist_squared / dp, 0.f);
    ds.object = (const Object *) this;

    return ds;
}

template <typename Interaction, typename DirectionSample,
          typename Value>
Value Shape::pdf_direction_fallback(const Interaction &,
                                    const DirectionSample &ds,
                                    mask_t<Value> active) const {
    Value pdf = pdf_position(ds, active),
           dp = abs_dot(ds.d, ds.n);

    pdf *= select(neq(dp, 0.f), (ds.dist * ds.dist) / dp, 0.f);

    return pdf;
}

DirectionSample3f Shape::sample_direction(const Interaction3f& it,
                                          const Point2f& sample) const {
    return sample_direction_fallback(it, sample, true);
}

DirectionSample3fP Shape::sample_direction(const Interaction3fP& it,
                                           const Point2fP& sample,
                                           MaskP active) const {
    return sample_direction_fallback(it, sample, active);
}

#if defined(MTS_ENABLE_AUTODIFF)
DirectionSample3fD Shape::sample_direction(const Interaction3fD& it,
                                           const Point2fD& sample,
                                           MaskD active) const {
    return sample_direction_fallback(it, sample, active);
}
#endif

Float Shape::pdf_direction(const Interaction3f& it,
                           const DirectionSample3f& ds) const {
    return pdf_direction_fallback(it, ds, true);
}

FloatP Shape::pdf_direction(const Interaction3fP& it,
                            const DirectionSample3fP& ds,
                            MaskP active) const {
    return pdf_direction_fallback(it, ds, active);
}

#if defined(MTS_ENABLE_AUTODIFF)
FloatD Shape::pdf_direction(const Interaction3fD& it,
                            const DirectionSample3fD& ds,
                            MaskD active) const {
    return pdf_direction_fallback(it, ds, active);
}
#endif

std::pair<bool, Float> Shape::ray_intersect(const Ray3f& /* ray */,
                                            Float* /* cache */) const {
    NotImplementedError("ray_intersect");
}

std::pair<MaskP, FloatP> Shape::ray_intersect(const Ray3fP& /* ray */,
                                              FloatP* /* cache */,
                                              MaskP /* active */) const {
    NotImplementedError("ray_intersect_p");
}

#if defined(MTS_ENABLE_AUTODIFF)
std::pair<MaskD, FloatD> Shape::ray_intersect(const Ray3fD& /* ray */,
                                              FloatD* /* cache */,
                                              MaskD /* active */) const {
    NotImplementedError("ray_intersect_d");
}
#endif

bool Shape::ray_test(const Ray3f &ray) const {
    Float unused[MTS_KD_INTERSECTION_CACHE_SIZE];
    return ray_intersect(ray, unused).first;
}

MaskP Shape::ray_test(const Ray3fP &ray, MaskP active) const {
    FloatP unused[MTS_KD_INTERSECTION_CACHE_SIZE];
    return ray_intersect(ray, unused, active).first;
}

#if defined(MTS_ENABLE_AUTODIFF)
MaskD Shape::ray_test(const Ray3fD &ray, MaskD active) const {
    FloatD unused[MTS_KD_INTERSECTION_CACHE_SIZE];
    return ray_intersect(ray, unused, active).first;
}
#endif

void Shape::fill_surface_interaction(const Ray3f & /* ray */,
                                     const Float * /* cache */,
                                     SurfaceInteraction3f & /* si */) const {
    NotImplementedError("fill_surface_interaction");
}

void Shape::fill_surface_interaction(const Ray3fP & /* ray */,
                                     const FloatP * /* cache */,
                                     SurfaceInteraction3fP & /* si */,
                                     MaskP /* active */) const {
    NotImplementedError("fill_surface_interaction_p");
}

#if defined(MTS_ENABLE_AUTODIFF)
void Shape::fill_surface_interaction(const Ray3fD & /* ray */,
                                     const FloatD * /* cache */,
                                     SurfaceInteraction3fD & /* si */,
                                     MaskD /* active */) const {
    NotImplementedError("fill_surface_interaction_d");
}
#endif

SurfaceInteraction3f Shape::ray_intersect(const Ray3f &ray) const {
    SurfaceInteraction3f si = zero<SurfaceInteraction3f>();
    Float cache[MTS_KD_INTERSECTION_CACHE_SIZE];
    bool success = false;
    std::tie(success, si.t) = ray_intersect(ray, cache);
    if (success)
        fill_surface_interaction(ray, cache, si);
    return si;
}

SurfaceInteraction3fP Shape::ray_intersect(const Ray3fP &ray, MaskP active) const {
    SurfaceInteraction3fP si = zero<SurfaceInteraction3fP>();
    FloatP cache[MTS_KD_INTERSECTION_CACHE_SIZE];
    MaskP success = false;
    std::tie(success, si.t) = ray_intersect(ray, cache, active);
    if (any(success && active))
        fill_surface_interaction(ray, cache, si, success && active);
    return si;
}

std::pair<Vector3f, Vector3f>
Shape::normal_derivative(const SurfaceInteraction3f & /* si */,
                         bool /* shading_frame */) const {
    NotImplementedError("normal_derivative");
}

std::pair<Vector3fP, Vector3fP>
Shape::normal_derivative(const SurfaceInteraction3fP & /* si */,
                         bool /* shading_frame */,
                         MaskP /* active */) const {
    NotImplementedError("normal_derivative_p");
}

#if defined(MTS_ENABLE_AUTODIFF)
std::pair<Vector3fD, Vector3fD>
Shape::normal_derivative(const SurfaceInteraction3fD & /* si */,
                         bool /* shading_frame */,
                         MaskD /* active */) const {
    NotImplementedError("normal_derivative_d");
}
#endif

ScalarFloat Shape::surface_area() const { NotImplementedError("surface_area"); }

ScalarBoundingBox3f Shape::bbox(Index) const {
    return bbox();
}

ScalarBoundingBox3f Shape::bbox(Index index, const ScalarBoundingBox3f &clip) const {
    ScalarBoundingBox3f result = bbox(index);
    result.clip(clip);
    return result;
}

Shape::Size Shape::primitive_count() const {
    return 1;
}

Shape::Size Shape::effective_primitive_count() const {
    return primitive_count();
}

std::vector<ref<Object>> Shape::children() {
    std::vector<ref<Object>> result;

    if (m_bsdf)
        result.push_back(m_bsdf.get());
    if (m_emitter)
        result.push_back(m_emitter.get());
    if (m_sensor)
        result.push_back(m_sensor.get());
    if (m_interior_medium)
        result.push_back(m_interior_medium.get());
    if (m_exterior_medium)
        result.push_back(m_exterior_medium.get());

    return result;
}

MTS_INSTANTIATE_OBJECT(Shape)
NAMESPACE_END(mitsuba)
