#include <mitsuba/render/kdtree.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/core/plugin.h>

NAMESPACE_BEGIN(mitsuba)

Shape::Shape(const Properties &props) {
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
        /* Create a default diffuse BSDF */
        m_bsdf = PluginManager::instance()->create_object<BSDF>(
            Properties("diffuse"));
    }
}

Shape::~Shape() { }

PositionSample3f Shape::sample_position(Float /* time */,
                                        const Point2f& /* sample */) const {
    NotImplementedError("sample_position");
}

PositionSample3fP Shape::sample_position(FloatP /* time */,
                                         const Point2fP& /* sample */,
                                         MaskP /* active */) const {
    NotImplementedError("sample_position_p");
}

Float Shape::pdf_position(const PositionSample3f& /* ps */) const {
    NotImplementedError("pdf_position");
}

FloatP Shape::pdf_position(const PositionSample3fP& /* ps */, MaskP /* active */) const {
    NotImplementedError("pdf_position_p");
}

template <typename Interaction, typename Value, typename Point2,
          typename Point3, typename DirectionSample, typename Mask>
DirectionSample Shape::sample_direction_fallback(const Interaction &it,
                                                 const Point2 &sample,
                                                 Mask active) const {
    DirectionSample ds(sample_position(it.time, sample, active));
    ds.d = ds.p - it.p;

    Value dist_squared = squared_norm(ds.d);
    ds.dist = sqrt(dist_squared);
    ds.d /= ds.dist;

    Value dp = abs_dot(ds.d, ds.n);
    ds.pdf *= select(neq(dp, 0.f), dist_squared / dp, 0.f);
    ds.object = this;

    return ds;
}

template <typename Interaction, typename DirectionSample,
          typename Value, typename Mask>
Value Shape::pdf_direction_fallback(const Interaction &,
                                    const DirectionSample &ds,
                                    Mask active) const {
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

Float Shape::pdf_direction(const Interaction3f& it,
                           const DirectionSample3f& ds) const {
    return pdf_direction_fallback(it, ds, true);
}

FloatP Shape::pdf_direction(const Interaction3fP& it,
                            const DirectionSample3fP& ds,
                            MaskP active) const {
    return pdf_direction_fallback(it, ds, active);
}

std::pair<bool, Float> Shape::ray_intersect(const Ray3f& /* ray */,
                                            Float* /* cache */) const {
    NotImplementedError("ray_intersect");
}

std::pair<MaskP, FloatP> Shape::ray_intersect(const Ray3fP& /* ray */,
                                              FloatP* /* cache */,
                                              MaskP /* active */) const {
    NotImplementedError("ray_intersect_p");
}

bool Shape::ray_test(const Ray3f &ray) const {
    Float unused[MTS_KD_INTERSECTION_CACHE_SIZE];
    return ray_intersect(ray, unused).first;
}

MaskP Shape::ray_test(const Ray3fP &ray, MaskP active) const {
    FloatP unused[MTS_KD_INTERSECTION_CACHE_SIZE];
    return ray_intersect(ray, unused, active).first;
}

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

Float Shape::surface_area() const { NotImplementedError("surface_area"); }

BoundingBox3f Shape::bbox(Index) const {
    return bbox();
}

BoundingBox3f Shape::bbox(Index index, const BoundingBox3f &clip) const {
    BoundingBox3f result = bbox(index);
    result.clip(clip);
    return result;
}

Shape::Size Shape::primitive_count() const {
    return 1;
}

Shape::Size Shape::effective_primitive_count() const {
    return primitive_count();
}

MTS_IMPLEMENT_CLASS(Shape, Object)
NAMESPACE_END(mitsuba)
