#include <mitsuba/core/properties.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/kdtree.h>
#include <mitsuba/render/integrator.h>
#include <enoki/stl.h>

#if defined(MTS_ENABLE_EMBREE)
#  include "scene_embree.inl"
#else
#  include "scene_native.inl"
#endif

#if defined(MTS_ENABLE_OPTIX)
#  include "scene_optix.inl"
#endif

NAMESPACE_BEGIN(mitsuba)

MTS_VARIANT Scene<Float, Spectrum>::Scene(const Properties &props) {
    std::vector<Emitter *> surface_emitters_to_check;

    for (auto &kv : props.objects()) {
        m_children.push_back(kv.second.get());

        Shape *shape           = dynamic_cast<Shape *>(kv.second.get());
        Emitter *emitter       = dynamic_cast<Emitter *>(kv.second.get());
        Sensor *sensor         = dynamic_cast<Sensor *>(kv.second.get());
        Integrator *integrator = dynamic_cast<Integrator *>(kv.second.get());

        if (shape) {
            if (shape->is_emitter())
                m_emitters.push_back(shape->emitter());
            if (shape->is_sensor())
                m_sensors.push_back(shape->sensor());

            m_bbox.expand(shape->bbox());
            m_shapes.push_back(shape);
        } else if (emitter) {
            // Surface emitters will be added to the list when attached to a shape
            if (has_flag(emitter->flags(), EmitterFlags::Surface))
                surface_emitters_to_check.push_back(emitter);
            else
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

    if (m_sensors.empty()) {
        Log(Warn, "No sensors found! Instantiating a perspective camera..");
        Properties sensor_props("perspective");
        sensor_props.set_float("fov", 45.0f);

        /* Create a perspective camera with a 45 deg. field of view
           and positioned so that it can see the entire scene */
        if (m_bbox.valid()) {
            ScalarPoint3f center = m_bbox.center();
            ScalarVector3f extents = m_bbox.extents();

            ScalarFloat distance =
                hmax(extents) / (2.f * std::tan(45.f * .5f * math::Pi<ScalarFloat> / 180.f));

            sensor_props.set_float("far_clip", hmax(extents) * 5 + distance);
            sensor_props.set_float("near_clip", distance / 100);

            sensor_props.set_float("focus_distance", distance + extents.z() / 2);
            sensor_props.set_transform(
                "to_world", ScalarTransform4f::translate(ScalarVector3f(
                                center.x(), center.y(), m_bbox.min.z() - distance)));
        }

        m_sensors.push_back(
            PluginManager::instance()->create_object<Sensor>(sensor_props));
    }

    if (!m_integrator) {
        Log(Warn, "No integrator found! Instantiating a path tracer..");
        m_integrator = PluginManager::instance()->
            create_object<Integrator>(Properties("path"));
    }

    if constexpr (is_cuda_array_v<Float>)
        accel_init_gpu(props);
    else
        accel_init_cpu(props);

    // Create emitters' shapes (environment luminaires)
    for (Emitter *emitter: m_emitters)
        emitter->set_scene(this);

    // Check that all surface emitters are associated with a shape
    for (const Emitter *emitter : surface_emitters_to_check) {
        if (!emitter->shape())
            Throw("Surface emitter was not attached to any shape: %s", emitter);
    }
}

MTS_VARIANT Scene<Float, Spectrum>::~Scene() {
    if constexpr (is_cuda_array_v<Float>)
        accel_release_gpu();
    else
        accel_release_cpu();
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect(const Ray3f &ray, Mask active) const {
    if constexpr (is_cuda_array_v<Float>)
        return ray_intersect_gpu(ray, active);
    else
        return ray_intersect_cpu(ray, active);
}

MTS_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_naive(const Ray3f &ray, Mask active) const {
#if !defined(MTS_ENABLE_EMBREE)
    if constexpr (!is_cuda_array_v<Float>)
        return ray_intersect_naive_cpu(ray, active);
#endif
    ENOKI_MARK_USED(ray);
    ENOKI_MARK_USED(active);
    NotImplementedError("ray_intersect_naive");
}

MTS_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test(const Ray3f &ray, Mask active) const {
    if constexpr (is_cuda_array_v<Float>)
        return ray_test_gpu(ray, active);
    else
        return ray_test_cpu(ray, active);
}

MTS_VARIANT std::pair<typename Scene<Float, Spectrum>::DirectionSample3f, Spectrum>
Scene<Float, Spectrum>::sample_emitter_direction(const Interaction3f &ref, const Point2f &sample_,
                                                 bool test_visibility, Mask active) const {
    using EmitterPtr = replace_scalar_t<Float, Emitter*>;

    ScopedPhase sp(ProfilerPhase::SampleEmitterDirection);
    Point2f sample(sample_);

    DirectionSample3f ds;
    Spectrum spec;

    if (likely(!m_emitters.empty())) {
        // Randomly pick an emitter
        UInt32 index = min(UInt32(sample.x() * (ScalarFloat) m_emitters.size()), (uint32_t) m_emitters.size()-1);
        ScalarFloat emitter_pdf = 1.f / m_emitters.size();
        EmitterPtr emitter = gather<EmitterPtr>(m_emitters.data(), index, active);

        // Rescale sample.x() to lie in [0,1) again
        sample.x() = (sample.x() - index*emitter_pdf) * m_emitters.size();

        // Sample a direction towards the emitter
        std::tie(ds, spec) = emitter->sample_direction(ref, sample, active);
        active &= neq(ds.pdf, 0.f);

        // Perform a visibility test if requested
        if (test_visibility && any_or<true>(active)) {
            Ray3f ray(ref.p, ds.d, math::RayEpsilon<Float> * (1.f + hmax(abs(ref.p))),
                      ds.dist * (1.f - math::ShadowEpsilon<Float>), ref.time, ref.wavelengths);
            spec[ray_test(ray, active)] = 0.f;
        }

        // Account for the discrete probability of sampling this emitter
        ds.pdf *= emitter_pdf;
        spec *= rcp(emitter_pdf);
    } else {
        ds = zero<DirectionSample3f>();
        spec = 0.f;
    }

    return { ds, spec };
}

// =============================================================
//! Methods related to participating media
// =============================================================
MTS_VARIANT
Spectrum Scene<Float, Spectrum>::eval_transmittance(const Point3f &p1, Mask p1_on_surface,
                                                    const Point3f &p2, Mask p2_on_surface,
                                                    Float time, Wavelength wavelengths,
                                                    MediumPtr medium, int max_interactions,
                                                    Sampler *sampler, Mask active) const {

    Vector3f d      = p2 - p1;
    Float remaining = norm(d);
    d /= remaining;

    Float used_epsilon = math::Epsilon<ScalarFloat> * (1.f + hmax(abs(p1)));

    Float min_dist      = select(p1_on_surface, used_epsilon, Float(0.0f));
    Float length_factor = select(p2_on_surface, Float(1 - math::ShadowEpsilon<Float>), Float(1.0f));
    Ray3f ray(p1, d, min_dist, remaining * length_factor, time, wavelengths);
    Spectrum transmittance(1.0f);

    int interactions = 0;
    while (any((remaining > 0) && active) && interactions < max_interactions) {

        // Intersect the transmittance ray with the scene
        SurfaceInteraction3f si = this->ray_intersect(ray, active);

        // If intersection is found: evaluate BSDF transmission
        Mask active_surface = active && si.is_valid();
        if (any_or<true>(active_surface)) {
            auto bsdf = si.bsdf(ray);
            masked(transmittance, active_surface) *= bsdf->eval_null_transmission(si, active_surface);
        }

        Mask active_medium = neq(medium, nullptr) && active;
        if (any_or<true>(active_medium)) {
            Ray tr_eval_ray(ray, 0, min(si.t, remaining));
            masked(transmittance, active_medium) *=
                medium->eval_transmittance(tr_eval_ray, sampler, active);
        }

        active &= si.is_valid() && any(neq(depolarize(transmittance), 0.f));

        // If a medium transition is taking place: Update the medium pointer
        Mask has_medium_trans = active && si.is_valid() && si.is_medium_transition();
        if (any_or<true>(has_medium_trans)) {
            masked(medium, has_medium_trans) = si.target_medium(ray.d);
        }
        // Update the ray with new origin & t parameter
        masked(ray.o, active) = si.p;
        masked(remaining, active) -= si.t;
        masked(ray.mint, active) = used_epsilon;
        masked(ray.maxt, active) = remaining * length_factor;
        interactions++;
    }
    return transmittance;
}

MTS_VARIANT std::pair<typename Scene<Float, Spectrum>::DirectionSample3f, Spectrum>
Scene<Float, Spectrum>::sample_emitter_direction_attenuated(const Interaction3f &ref, bool is_medium_interaction,
                                                            const MediumPtr medium, const Point2f &sample_,
                                                            Sampler *sampler, bool test_visibility, Mask active) const {
    using EmitterPtr = replace_scalar_t<Float, Emitter *>;

    ScopedPhase sp(ProfilerPhase::SampleEmitterDirection);
    DirectionSample3f ds;
    Spectrum spec = 0.f;
    Point2f sample(sample_);

    int max_interactions = 10;
    if (likely(!m_emitters.empty())) {
        // Randomly pick an emitter according to the precomputed emitter distribution
        UInt32 index            = min(UInt32(sample.x() * (ScalarFloat) m_emitters.size()),
                           (uint32_t) m_emitters.size() - 1);
        ScalarFloat emitter_pdf = 1.f / m_emitters.size();
        EmitterPtr emitter      = gather<EmitterPtr>(m_emitters.data(), index, active);

        // Sample a direction towards the emitter
        std::tie(ds, spec) = emitter->sample_direction(ref, sample, active);
        active &= neq(ds.pdf, 0.f);

        // Perform a visibility test if requested
        if (test_visibility && any_or<true>(active)) {
            Mask is_surface_interaction = !is_medium_interaction;
            Mask is_surface_emitter     = !(emitter->is_environment());
            Spectrum tr =
                eval_transmittance(ref.p, is_surface_interaction, ds.p, is_surface_emitter,
                                   ref.time, ref.wavelengths, medium, max_interactions, sampler, active);
            masked(spec, active) = tr * spec;
        }
        // Account for the discrete probability of sampling this emitter
        ds.pdf *= emitter_pdf;
        spec *= rcp(emitter_pdf);
    }
    return { ds, spec };
}

MTS_VARIANT Float
Scene<Float, Spectrum>::pdf_emitter_direction(const Interaction3f &ref,
                                              const DirectionSample3f &ds,
                                              Mask active) const {
    using EmitterPtr = replace_scalar_t<Float, const Emitter *>;

    return reinterpret_array<EmitterPtr>(ds.object)->pdf_direction(ref, ds, active) *
        (1.f / m_emitters.size());
}

MTS_VARIANT void Scene<Float, Spectrum>::traverse(TraversalCallback *callback) {
    for (auto& child : m_children) {
        std::string id = child->id();
        if (id.empty() || string::starts_with(id, "_unnamed_"))
            id = child->class_()->name();
        callback->put_object(id, child.get());
    }
}

MTS_VARIANT void Scene<Float, Spectrum>::parameters_changed() {
    if (m_environment)
        m_environment->set_scene(this);
}

MTS_VARIANT std::string Scene<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
    oss << "Scene[" << std::endl
        << "  children = [" << std::endl;
    for (size_t i = 0; i < m_children.size(); ++i) {
        oss << "    " << string::indent(m_children[i]->to_string(), 4);
        if (i + 1 < m_children.size())
            oss << ",";
        oss << std::endl;
    }
    oss << "  ]"<< std::endl
        << "]";
    return oss.str();
}

void librender_nop() { }

MTS_IMPLEMENT_CLASS_VARIANT(Scene, Object, "scene")
MTS_INSTANTIATE_CLASS(Scene)
NAMESPACE_END(mitsuba)
