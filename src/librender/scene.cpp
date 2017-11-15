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
        // TODO: proper handling of multiple samplers
        m_sampler = m_sensors.front()->sampler();
    }

    if (m_integrator && m_sampler) {
        m_integrator->configure_sampler(this, m_sampler);
    }
}

Scene::~Scene() { }

// =============================================================
//! @{ \name Sampling interface
// =============================================================
Spectrumf Scene::sample_emitter_direct(DirectSample3f &d_rec,
                                       const Point2f &sample_,
                                       bool test_visibility) const {
    // Randomly pick an emitter according to the precomputed (discrete)
    // distribution of emitter intensity.
    Point2f sample(sample_);
    size_t index;
    Float emitter_pdf;
    std::tie(index, emitter_pdf, sample.x()) =
        m_emitters_pdf.sample_reuse_pdf(sample.x());

    const Emitter *emitter = m_emitters[index].get();
    auto value = emitter->sample_direct(d_rec, sample);

    if (d_rec.pdf == 0)
        return Spectrumf(0.0f);

    if (test_visibility) {
        // Shadow ray
        Ray3f ray(d_rec.p, d_rec.d, rcp(d_rec.d), math::Epsilon,
                  d_rec.dist * (1.0f - math::ShadowEpsilon), d_rec.time);
        if (ray_intersect(ray, ray.mint, ray.maxt))
            return Spectrumf(0.0f);
    }

    d_rec.object = emitter;
    d_rec.pdf *= emitter_pdf;
    value /= emitter_pdf;
    return value;
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

template MTS_EXPORT_CORE bool Scene::ray_intersect(
    const Ray3f &, const Float &, const Float &) const;
template MTS_EXPORT_CORE mask_t<FloatP> Scene::ray_intersect(
    const Ray3fP &, const FloatP &, const FloatP &) const;
template MTS_EXPORT_CORE std::pair<bool, Float> Scene::ray_intersect(
    const Ray3f &, const Float &, const Float &, SurfaceInteraction3f &,
    const bool &) const;
template MTS_EXPORT_CORE std::pair<mask_t<FloatP>, FloatP> Scene::ray_intersect(
    const Ray3fP &, const FloatP &, const FloatP &, SurfaceInteraction3fP &,
    const mask_t<FloatP> &) const;
//! @}
// =============================================================

MTS_IMPLEMENT_CLASS(Scene, Object)
NAMESPACE_END(mitsuba)
