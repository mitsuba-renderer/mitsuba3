#include <mitsuba/core/properties.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/integrator.h>
#include <enoki/stl.h>

NAMESPACE_BEGIN(mitsuba)

Scene::Scene(const Properties &props) {
    #if !defined(MTS_USE_EMBREE)
        m_kdtree = new ShapeKDTree(props);
    #else
        embree_init();
    #endif

    #if defined(MTS_USE_OPTIX)
        uint32_t n_shapes = 0;
        for (auto &kv : props.objects()) {
            Shape *shape = dynamic_cast<Shape *>(kv.second.get());
            if (shape)
                ++n_shapes;
        }
        optix_init(n_shapes);
    #endif

    for (auto &kv : props.objects()) {
        m_children.push_back(kv.second.get());

        Shape *shape           = dynamic_cast<Shape *>(kv.second.get());
        Emitter *emitter       = dynamic_cast<Emitter *>(kv.second.get());
        Sensor *sensor         = dynamic_cast<Sensor *>(kv.second.get());
        Integrator *integrator = dynamic_cast<Integrator *>(kv.second.get());

        if (shape) {
            m_shapes.push_back(shape);
            #if !defined(MTS_USE_EMBREE)
                m_kdtree->add_shape(shape);
            #else
                embree_register(shape);
            #endif

            #if defined(MTS_USE_OPTIX)
                optix_register(shape);
            #endif

            if (shape->emitter())
                m_emitters.push_back(shape->emitter());

            m_bbox.expand(shape->bbox());
        } else if (emitter) {
            m_emitters.push_back(emitter);
            if (emitter->is_environment()) {
                if (m_environment)
                    Throw("Only one environment emitter can be specified per scene.");
                m_environment = emitter;
            }
        } else if (sensor) {
            m_sensors.push_back(sensor);
        } else if (integrator) {
            if (m_integrator)
                Throw("Only one integrator can be specified per scene.");
            m_integrator = integrator;
        }
    }

    if (m_sensors.size() == 0) {
        Log(EWarn, "No sensors found! Instantiating a perspective camera..");
        Properties sensor_props("perspective");
        sensor_props.set_float("fov", 45.0f);
        sensor_props.set_bool("monochrome", m_monochrome);
        sensor_props.mark_queried("monochrome");

        /* Create a perspective camera with a 45 deg. field of view
           and positioned so that it can see the entire scene */
        if (m_bbox.valid()) {
            Point3f center = m_bbox.center();
            Vector3f extents = m_bbox.extents();

            Float distance =
                hmax(extents) / (2.f * std::tan(45.f * .5f * math::Pi / 180));

            sensor_props.set_float("far_clip", hmax(extents) * 5 + distance);
            sensor_props.set_float("near_clip", distance / 100);

            sensor_props.set_float("focus_distance", distance + extents.z() / 2);
            sensor_props.set_transform(
                "to_world", Transform4f::translate(Vector3f(
                                center.x(), center.y(), m_bbox.min.z() - distance)));
        }

        m_sensors.push_back(
            PluginManager::instance()->create_object<Sensor>(sensor_props));
    }

    if (!m_integrator) {
        Log(EWarn, "No integrator found! Instantiating a path tracer..");
        Properties props_integrator("path");
        props_integrator.set_bool("monochrome", m_monochrome);
        props_integrator.mark_queried("monochrome");
        m_integrator = PluginManager::instance()->create_object<Integrator>(props_integrator);
    }

    #if !defined(MTS_USE_EMBREE)
        m_kdtree->build();
    #else
        embree_build();
    #endif

    #if defined(MTS_USE_OPTIX)
        optix_build();
    #endif

    // Precompute a discrete distribution over emitters
    m_emitter_distr = DiscreteDistribution(m_emitters.size());
    for (size_t i = 0; i < m_emitters.size(); ++i)
        m_emitter_distr.append(1.f);  // Simple uniform distribution for now.
    if (m_emitters.size() > 0)
        m_emitter_distr.normalize();

    // Create emitters' shapes (environment luminaires)
    for (auto emitter: m_emitters)
        emitter->create_shape(this);

    if (!m_sensors.empty()) {
        m_sampler = m_sensors.front()->sampler();
        if (m_sensors.size() > 1)
            Throw("Only one sensor is supported at the moment.");
    }
}

Scene::~Scene() {
    #if defined(MTS_USE_OPTIX)
        optix_release();
    #endif

    #if defined(MTS_USE_EMBREE)
        embree_release();
    #endif
}

std::vector<ref<Object>> Scene::children() { return m_children; }

// =============================================================
//! Sampling-related methods
// =============================================================

template <typename Interaction, typename Value, typename Spectrum,
          typename Point3, typename Point2, typename DirectionSample,
          typename Mask>
MTS_INLINE std::pair<DirectionSample, Spectrum>
Scene::sample_emitter_direction_impl(const Interaction &ref, Point2 sample,
                                     bool test_visibility, Mask active) const {
    ScopedPhase sp(is_static_array_v<Value> ?
            EProfilerPhase::ESampleEmitterDirectionP :
            EProfilerPhase::ESampleEmitterDirection);

    using EmitterPtr = replace_scalar_t<Value, const Emitter *>;
    using Index = uint_array_t<Value>;
    using Ray = Ray<Point<Value, 3>>;

    DirectionSample ds;
    Spectrum spec = 0.f;

    if (likely(!m_emitters.empty())) {
        // Randomly pick an emitter according to the precomputed emitter distribution
        Index index;
        Value emitter_pdf;
        std::tie(index, emitter_pdf, sample.x()) =
            m_emitter_distr.sample_reuse_pdf(sample.x());
        EmitterPtr emitter = gather<EmitterPtr>(m_emitters.data(), index, active);

        // Sample a direction towards the emitter
        std::tie(ds, spec) = emitter->sample_direction(ref, sample, active);
        active &= neq(ds.pdf, 0.f);

        // Perform a visibility test if requested
        if (test_visibility && any_or<true>(active)) {
            Ray ray(ref.p, ds.d, math::Epsilon * (1.f + hmax(abs(ref.p))),
                    ds.dist * (1.f - math::ShadowEpsilon),
                    ref.time, ref.wavelengths);
            spec[ray_test(ray, active)] = 0.f;
        }

        // Account for the discrete probability of sampling this emitter
        ds.pdf *= emitter_pdf;
        spec *= rcp(emitter_pdf);
    }

    return { ds, spec };
}

std::pair<DirectionSample3f, Spectrumf>
Scene::sample_emitter_direction(const Interaction3f &ref,
                                const Point2f &sample,
                                bool test_visibility) const {
    return sample_emitter_direction_impl(ref, sample, test_visibility, true);
}

std::pair<DirectionSample3fP, SpectrumfP>
Scene::sample_emitter_direction(const Interaction3fP &ref,
                                const Point2fP &sample,
                                bool test_visibility,
                                MaskP active) const {
    return sample_emitter_direction_impl(ref, sample, test_visibility, active);
}

#if defined(MTS_ENABLE_AUTODIFF)
std::pair<DirectionSample3fD, SpectrumfD>
Scene::sample_emitter_direction(const Interaction3fD &ref,
                                const Point2fD &sample,
                                bool test_visibility,
                                MaskD active) const {
    return sample_emitter_direction_impl(ref, sample, test_visibility, active);
}
#endif

template <typename Interaction, typename DirectionSample, typename Value,
          typename Mask>
MTS_INLINE Value Scene::pdf_emitter_direction_impl(const Interaction &ref,
                                                   const DirectionSample &ds,
                                                   Mask active) const {
    using EmitterPtr = replace_scalar_t<Value, const Emitter *>;
    return reinterpret_array<EmitterPtr>(ds.object)->pdf_direction(ref, ds, active) *
        m_emitter_distr.eval(0);
}

Float Scene::pdf_emitter_direction(const Interaction3f &ref,
                                   const DirectionSample3f &ds) const {
    return pdf_emitter_direction_impl(ref, ds, true);
}

FloatP Scene::pdf_emitter_direction(const Interaction3fP &ref,
                                    const DirectionSample3fP &ds,
                                    MaskP active) const {
    return pdf_emitter_direction_impl(ref, ds, active);
}

#if defined(MTS_ENABLE_AUTODIFF)
FloatD Scene::pdf_emitter_direction(const Interaction3fD &ref,
                                    const DirectionSample3fD &ds,
                                    MaskD active) const {
    return pdf_emitter_direction_impl(ref, ds, active);
}
#endif

std::string Scene::to_string() const {
    std::ostringstream oss;
    oss << "Scene[" << std::endl
        << "  children = [" << std::endl;
    for (auto shape : m_children)
        oss << "    " << string::indent(shape->to_string(), 4)
            << "," << std::endl;
    oss << "  ]"<< std::endl
        << "]";
    return oss.str();
}

MTS_IMPLEMENT_CLASS(Scene, Object)
NAMESPACE_END(mitsuba)
