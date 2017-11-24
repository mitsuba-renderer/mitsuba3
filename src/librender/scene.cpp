#include <mitsuba/core/properties.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/integrator.h>
#include <mitsuba/render/records.h>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/sensor.h>


NAMESPACE_BEGIN(mitsuba)

Scene::Scene(const Properties &props) {
    m_kdtree = new ShapeKDTree(props);

    for (auto &kv : props.objects()) {
        auto *shape = dynamic_cast<Shape *>(kv.second.get());
        auto *emitter = dynamic_cast<Emitter *>(kv.second.get());
        auto *sensor = dynamic_cast<Sensor *>(kv.second.get());
        auto *integrator = dynamic_cast<Integrator *>(kv.second.get());

        if (shape) {
            m_kdtree->add_shape(shape);
            if (shape->emitter())
                m_emitters.push_back(shape->emitter());
        } else if (emitter) {
            m_emitters.push_back(emitter);
        } else if (sensor) {
            m_sensors.push_back(sensor);
        } else if (integrator) {
            if (m_integrator)
                Log(EError, "Only one integrator can be specified per scene.");
            m_integrator = integrator;
        } else {
            Throw("Tried to add an unsupported object of type %s", kv.second);
        }
    }

    m_kdtree->build();

    // Precompute the emitters PDF.
    m_emitters_pdf = DiscreteDistribution(m_emitters.size());
    for (size_t i = 0; i < m_emitters.size(); ++i) {
        // TODO: which weight to give each emitter?
        m_emitters_pdf.append(1.0f);
    }
    if (m_emitters.size() > 0)
        m_emitters_pdf.normalize();

    if (m_sensors.size() > 0) {
        // TODO: proper handling of multiple sensors (need to always use the
        // sampler from the currently selected sensor).
        m_sampler = m_sensors.front()->sampler();
    }

    if (m_integrator && m_sampler)
        m_integrator->configure_sampler(this, m_sampler);
}

Scene::~Scene() { }

// =============================================================
//! @{ \name Sampling interface
// =============================================================
template <typename DirectSample, typename Value>
auto Scene::sample_emitter_direct_impl(
        DirectSample &d_rec, const Point<Value, 2> &sample_,
        bool test_visibility, mask_t<Value> active) const {
    using Point2      = Point<Value, 2>;
    using Point3      = point3_t<Point2>;
    using Index       = like_t<Value, size_t>;
    using Ray         = Ray<Point3>;
    using EmittersPtr = like_t<Value, const Emitter *>;
    // Randomly pick an emitter according to the precomputed (discrete)
    // distribution of emitter intensity.
    Point2 sample(sample_);
    Index index;
    Value emitter_pdf;

    std::tie(index, emitter_pdf, sample.x()) =
        m_emitters_pdf.sample_reuse_pdf(sample.x());
    auto emitter = gather<EmittersPtr>(m_emitters.data(), index, active);

    auto value = emitter->sample_direct(d_rec, sample, active);
    active &= neq(d_rec.pdf, 0.0f);

    if (test_visibility) {
        // Shadow ray
        Ray ray(d_rec.p, d_rec.d, rcp(d_rec.d), math::Epsilon,
                d_rec.dist * (1.0f - math::ShadowEpsilon), d_rec.time);
        active &= ~ray_intersect(ray, ray.mint, ray.maxt, active);
    }

    masked(d_rec.object, active) = emitter;
    masked(d_rec.pdf, active) *= emitter_pdf;
    return select(active, value / emitter_pdf, 0.0f);
}

Spectrumf Scene::sample_emitter_direct(DirectSample3f &d_rec,
                                       const Point2f &sample,
                                       bool test_visibility) const {
    return sample_emitter_direct_impl(d_rec, sample, test_visibility, true);
}
SpectrumfP Scene::sample_emitter_direct(
        DirectSample3fP &d_rec, const Point2fP &sample,
        bool test_visibility, const mask_t<FloatP> &active) const {
    return sample_emitter_direct_impl(d_rec, sample, test_visibility, active);
}
//! @}
// =============================================================


// =============================================================
//! @{ \name Explicit template instantiations & misc.
// =============================================================
std::string Scene::to_string() const {
    return tfm::format("Scene[\n"
        "  kdtree = %s\n"
        "]",
        string::indent(m_kdtree->to_string())
    );
}

// Full intersection
template MTS_EXPORT_RENDER auto Scene::ray_intersect(
    const Ray3f &, const Float &, const Float &, SurfaceInteraction3f &,
    const bool &) const;
template MTS_EXPORT_RENDER auto Scene::ray_intersect(
    const Ray3fP &, const FloatP &, const FloatP &, SurfaceInteraction3fP &,
    const mask_t<FloatP> &) const;
// Shadow rays
// TODO: why does the following lead to a compile error?
// template MTS_EXPORT_RENDER auto Scene::ray_intersect<Ray3f, Float>(
//     const Ray3f &, const Float &, const Float &, const bool &) const;
// template MTS_EXPORT_RENDER auto Scene::ray_intersect(
//     const Ray3fP &, const FloatP &, const FloatP &, const mask_t<FloatP> &) const;
//! @}
// =============================================================

MTS_IMPLEMENT_CLASS(Scene, Object)
NAMESPACE_END(mitsuba)
