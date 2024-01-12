#include <mitsuba/core/properties.h>
#include <mitsuba/core/plugin.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/medium.h>
#include <mitsuba/render/mesh.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/integrator.h>

#if defined(MI_ENABLE_EMBREE)
#  include "scene_embree.inl"
#else
#  include <mitsuba/render/kdtree.h>
#  include "scene_native.inl"
#endif

#if defined(MI_ENABLE_CUDA)
#  include "scene_optix.inl"
#endif

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Scene<Float, Spectrum>::Scene(const Properties &props) {
    for (auto &[k, v] : props.objects()) {
        Scene *scene           = dynamic_cast<Scene *>(v.get());
        Shape *shape           = dynamic_cast<Shape *>(v.get());
        Mesh *mesh             = dynamic_cast<Mesh *>(v.get());
        Emitter *emitter       = dynamic_cast<Emitter *>(v.get());
        Sensor *sensor         = dynamic_cast<Sensor *>(v.get());
        Integrator *integrator = dynamic_cast<Integrator *>(v.get());

        if (!scene)
            m_children.push_back(v.get());

        if (shape) {
            if (shape->is_emitter())
                m_emitters.push_back(shape->emitter());
            if (shape->is_sensor())
                m_sensors.push_back(shape->sensor());
            if (shape->is_shapegroup()) {
                m_shapegroups.push_back((ShapeGroup*)shape);
            } else {
                m_bbox.expand(shape->bbox());
                m_shapes.push_back(shape);
            }
            if (mesh)
                mesh->set_scene(this);
        } else if (emitter) {
            // Surface emitters will be added to the list when attached to a shape
            if (!has_flag(emitter->flags(), EmitterFlags::Surface))
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

    // Create sensors' shapes (environment sensors)
    for (Sensor *sensor: m_sensors)
        sensor->set_scene(this);

    if constexpr (dr::is_cuda_v<Float>)
        accel_init_gpu(props);
    else
        accel_init_cpu(props);

    if (!m_emitters.empty()) {
        // Inform environment emitters etc. about the scene bounds
        for (Emitter *emitter: m_emitters)
            emitter->set_scene(this);
    }

    m_shapes_dr = dr::load<DynamicBuffer<ShapePtr>>(
        m_shapes.data(), m_shapes.size());

    m_emitters_dr = dr::load<DynamicBuffer<EmitterPtr>>(
        m_emitters.data(), m_emitters.size());

    m_sensors_dr = dr::load<DynamicBuffer<SensorPtr>>(
        m_sensors.data(), m_sensors.size());

    dr::eval(m_emitters_dr, m_shapes_dr, m_sensors_dr);

    update_emitter_sampling_distribution();
    update_silhouette_sampling_distribution();

    m_shapes_grad_enabled = false;
}

MI_VARIANT
void Scene<Float, Spectrum>::update_emitter_sampling_distribution() {
    // Check if we need to use non-uniform emitter sampling.
    bool non_uniform_sampling = false;
    for (auto &e : m_emitters) {
        if (e->sampling_weight() != ScalarFloat(1.0)) {
            non_uniform_sampling = true;
            break;
        }
    }
    size_t n_emitters = m_emitters.size();
    if (non_uniform_sampling) {
        std::unique_ptr<ScalarFloat[]> sample_weights(new ScalarFloat[n_emitters]);
        for (size_t i = 0; i < n_emitters; ++i)
            sample_weights[i] = m_emitters[i]->sampling_weight();
        m_emitter_distr = std::make_unique<DiscreteDistribution<Float>>(
            sample_weights.get(), n_emitters);
    } else {
        // By default use uniform sampling with constant PMF
        m_emitter_pmf = m_emitters.empty() ? 0.f : (1.f / n_emitters);
        m_emitter_distr = nullptr;
    }
    // Clear emitter's dirty flag
    for (auto &e : m_emitters)
        e->set_dirty(false);
}

MI_VARIANT
void Scene<Float, Spectrum>::update_silhouette_sampling_distribution() {
    size_t n_shapes = m_shapes.size();
    std::vector<ScalarFloat> shape_weights{};
    m_silhouette_shapes.clear();

    for (size_t i = 0; i < n_shapes; ++i) {
        ScalarFloat weight = m_shapes[i]->silhouette_sampling_weight();
        // Only consider shapes that are being differentiated
        bool grad_enabled = m_shapes[i]->parameters_grad_enabled();

        if (grad_enabled && (weight > 0.f)) {
            uint32_t types = m_shapes[i]->silhouette_discontinuity_types();

            bool has_interior = has_flag(types, DiscontinuityFlags::InteriorType);
            bool has_perimeter = has_flag(types, DiscontinuityFlags::PerimeterType);
            bool has_discontinuity = has_interior || has_perimeter;

            if (has_discontinuity) {
                m_silhouette_shapes.emplace_back(m_shapes[i]);
                shape_weights.emplace_back(weight);
            }
        }
    }

    size_t silhouette_shape_count = m_silhouette_shapes.size();
    m_silhouette_shapes_dr = dr::load<DynamicBuffer<ShapePtr>>(
        m_silhouette_shapes.data(), silhouette_shape_count);
    if (silhouette_shape_count > 0u)
        m_silhouette_distr = std::make_unique<DiscreteDistribution<Float>>(
            shape_weights.data(), silhouette_shape_count);
}

MI_VARIANT Scene<Float, Spectrum>::~Scene() {
    if constexpr (dr::is_cuda_v<Float>)
        accel_release_gpu();
    else
        accel_release_cpu();

    // Trigger deallocation of all instances
    m_emitters.clear();
    m_shapes.clear();
    m_shapegroups.clear();
    m_sensors.clear();
    m_children.clear();
    m_integrator = nullptr;
    m_environment = nullptr;
}

// -----------------------------------------------------------------------

MI_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect(const Ray3f &ray, uint32_t ray_flags, Mask coherent, Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::RayIntersect, active);
    DRJIT_MARK_USED(coherent);

    if constexpr (dr::is_cuda_v<Float>)
        return ray_intersect_gpu(ray, ray_flags, active);
    else
        return ray_intersect_cpu(ray, ray_flags, coherent, active);
}

MI_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary(const Ray3f &ray, Mask coherent, Mask active) const {
    DRJIT_MARK_USED(coherent);
    if constexpr (dr::is_cuda_v<Float>)
        return ray_intersect_preliminary_gpu(ray, active);
    else
        return ray_intersect_preliminary_cpu(ray, coherent, active);
}

MI_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test(const Ray3f &ray, Mask coherent, Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::RayTest, active);
    DRJIT_MARK_USED(coherent);

    if constexpr (dr::is_cuda_v<Float>)
        return ray_test_gpu(ray, active);
    else
        return ray_test_cpu(ray, coherent, active);
}

MI_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_naive(const Ray3f &ray, Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::RayIntersect, active);

#if !defined(MI_ENABLE_EMBREE)
    if constexpr (!dr::is_cuda_v<Float>)
        return ray_intersect_naive_cpu(ray, active);
#endif
    DRJIT_MARK_USED(ray);
    DRJIT_MARK_USED(active);
    NotImplementedError("ray_intersect_naive");
}

// -----------------------------------------------------------------------

MI_VARIANT std::tuple<typename Scene<Float, Spectrum>::UInt32, Float, Float>
Scene<Float, Spectrum>::sample_emitter(Float index_sample, Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::SampleEmitter, active);

    if (unlikely(m_emitters.size() < 2)) {
        if (m_emitters.size() == 1)
            return { UInt32(0), 1.f, index_sample };
        else
            return { UInt32(-1), 0.f, index_sample };
    }

    if (m_emitter_distr != nullptr) {
        auto [index, reused_sample, pmf] = m_emitter_distr->sample_reuse_pmf(index_sample);
        return {index, dr::rcp(pmf), reused_sample};
    }

    uint32_t emitter_count = (uint32_t) m_emitters.size();
    ScalarFloat emitter_count_f = (ScalarFloat) emitter_count;
    Float index_sample_scaled = index_sample * emitter_count_f;

    UInt32 index = dr::minimum(UInt32(index_sample_scaled), emitter_count - 1u);

    return { index, emitter_count_f, index_sample_scaled - Float(index) };
}

MI_VARIANT Float Scene<Float, Spectrum>::pdf_emitter(UInt32 index,
                                                      Mask active) const {
    if (m_emitter_distr == nullptr)
        return m_emitter_pmf;
    else
        return m_emitter_distr->eval_pmf_normalized(index, active);
}

MI_VARIANT std::tuple<typename Scene<Float, Spectrum>::Ray3f, Spectrum,
                       const typename Scene<Float, Spectrum>::EmitterPtr>
Scene<Float, Spectrum>::sample_emitter_ray(Float time, Float sample1,
                                           const Point2f &sample2,
                                           const Point2f &sample3,
                                           Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::SampleEmitterRay, active);


    Ray3f ray;
    Spectrum weight;
    EmitterPtr emitter;

    // Potentially disable inlining of emitter sampling (if there is just a single emitter)
    bool vcall_inline = true;
    if constexpr (dr::is_jit_v<Float>)
         vcall_inline = false;

    size_t emitter_count = m_emitters.size();
    if (emitter_count > 1 || (emitter_count == 1 && !vcall_inline)) {
        auto [index, emitter_weight, sample_1_re] = sample_emitter(sample1, active);
        emitter = dr::gather<EmitterPtr>(m_emitters_dr, index, active);

        std::tie(ray, weight) =
            emitter->sample_ray(time, sample_1_re, sample2, sample3, active);

        weight *= emitter_weight;
    } else if (emitter_count == 1) {
        std::tie(ray, weight) =
            m_emitters[0]->sample_ray(time, sample1, sample2, sample3, active);
    } else {
        ray = dr::zeros<Ray3f>();
        weight = dr::zeros<Spectrum>();
        emitter = EmitterPtr(nullptr);
    }

    return { ray, weight, emitter };
}

MI_VARIANT std::pair<typename Scene<Float, Spectrum>::DirectionSample3f, Spectrum>
Scene<Float, Spectrum>::sample_emitter_direction(const Interaction3f &ref, const Point2f &sample_,
                                                 bool test_visibility, Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::SampleEmitterDirection, active);

    Point2f sample(sample_);
    DirectionSample3f ds;
    Spectrum spec;

    // Potentially disable inlining of emitter sampling (if there is just a single emitter)
    bool vcall_inline = true;
    if constexpr (dr::is_jit_v<Float>)
         vcall_inline = false;

    size_t emitter_count = m_emitters.size();
    if (emitter_count > 1 || (emitter_count == 1 && !vcall_inline)) {
        // Randomly pick an emitter
        auto [index, emitter_weight, sample_x_re] = sample_emitter(sample.x(), active);
        sample.x() = sample_x_re;

        // Sample a direction towards the emitter
        EmitterPtr emitter = dr::gather<EmitterPtr>(m_emitters_dr, index, active);
        std::tie(ds, spec) = emitter->sample_direction(ref, sample, active);

        // Account for the discrete probability of sampling this emitter
        ds.pdf *= pdf_emitter(index, active);
        spec *= emitter_weight;

        active &= (ds.pdf != 0.f);

        // Mark occluded samples as invalid if requested by the user
        if (test_visibility && dr::any_or<true>(active)) {
            Mask occluded = ray_test(ref.spawn_ray_to(ds.p), active);
            dr::masked(spec, occluded) = 0.f;
            dr::masked(ds.pdf, occluded) = 0.f;
        }
    } else if (emitter_count == 1) {
        // Sample a direction towards the (single) emitter
        std::tie(ds, spec) = m_emitters[0]->sample_direction(ref, sample, active);

        active &= (ds.pdf != 0.f);

        // Mark occluded samples as invalid if requested by the user
        if (test_visibility && dr::any_or<true>(active)) {
            Mask occluded = ray_test(ref.spawn_ray_to(ds.p), active);
            dr::masked(spec, occluded) = 0.f;
            dr::masked(ds.pdf, occluded) = 0.f;
        }
    } else {
        ds = dr::zeros<DirectionSample3f>();
        spec = 0.f;
    }

    return { ds, spec };
}

MI_VARIANT Float
Scene<Float, Spectrum>::pdf_emitter_direction(const Interaction3f &ref,
                                              const DirectionSample3f &ds,
                                              Mask active) const {
    MI_MASK_ARGUMENT(active);
    Float emitter_pmf;
    if (m_emitter_distr == nullptr)
        emitter_pmf = m_emitter_pmf;
    else
        emitter_pmf = ds.emitter->sampling_weight() * m_emitter_distr->normalization();
    return ds.emitter->pdf_direction(ref, ds, active) * emitter_pmf;
}

MI_VARIANT Spectrum Scene<Float, Spectrum>::eval_emitter_direction(
    const Interaction3f &ref, const DirectionSample3f &ds, Mask active) const {
    MI_MASK_ARGUMENT(active);
    return ds.emitter->eval_direction(ref, ds, active);
}

MI_VARIANT typename Scene<Float, Spectrum>::SilhouetteSample3f
Scene<Float, Spectrum>::sample_silhouette(const Point3f &sample_,
                                          uint32_t flags, Mask active) const {
    MI_MASK_ARGUMENT(active);

    if (unlikely(!m_silhouette_distr|| m_silhouette_shapes.size() == 0))
        return dr::zeros<SilhouetteSample3f>();

    // Sample a shape
    UInt32 shape_idx;
    Float reused_sample_x,
          shape_weight;
    std::tie(shape_idx, reused_sample_x, shape_weight) =
        m_silhouette_distr->sample_reuse_pmf(sample_.x(), active);
    ShapePtr shape =
        dr::gather<ShapePtr>(m_silhouette_shapes_dr, shape_idx, active);

    bool has_interior = has_flag(flags, DiscontinuityFlags::InteriorType);
    bool has_perimeter = has_flag(flags, DiscontinuityFlags::PerimeterType);
    Point3f sample(sample_);
    sample.x() = reused_sample_x;

    // Map a boundary sample space to a boundary segment in the scene space
    SilhouetteSample3f ss = dr::zeros<SilhouetteSample3f>();
    if (has_interior != has_perimeter) { // Only one discontinuity type
        ss = shape->sample_silhouette(sample, flags, active);
    } else {
        UInt32 shape_sil_types = shape->silhouette_discontinuity_types();
        Mask only_interior =
            active &&
            has_flag(shape_sil_types, DiscontinuityFlags::InteriorType) &&
            !has_flag(shape_sil_types, DiscontinuityFlags::PerimeterType);
        Mask only_perimeter =
            active &&
            !has_flag(shape_sil_types, DiscontinuityFlags::InteriorType) &&
            has_flag(shape_sil_types, DiscontinuityFlags::PerimeterType);
        Mask both =
            active &&
            has_flag(shape_sil_types, DiscontinuityFlags::InteriorType) &&
            has_flag(shape_sil_types, DiscontinuityFlags::PerimeterType);

        // If shapes have both types, weight them equally
        Mask interior  = only_interior  || (both && (sample.x() <  0.5f));
        Mask perimeter = only_perimeter || (both && (sample.x() >= 0.5f));
        dr::masked(sample.x(), interior && both)  = sample.x() * 2.f;
        dr::masked(sample.x(), perimeter && both) = sample.x() * 2.f - 1.f;

        uint32_t other_flags = flags & ~DiscontinuityFlags::AllTypes;
        SilhouetteSample3f ss_interior = shape->sample_silhouette(
            sample, (uint32_t) DiscontinuityFlags::InteriorType | other_flags,
            interior);
        SilhouetteSample3f ss_perimeter = shape->sample_silhouette(
            sample, (uint32_t) DiscontinuityFlags::PerimeterType | other_flags,
            perimeter);

        ss = dr::select(interior, ss_interior, ss_perimeter);
        dr::masked(ss.pdf, both) *= 0.5f;
    }

    ss.pdf *= shape_weight;
    ss.scene_index = shape_idx;

    return ss;
}

MI_VARIANT typename Scene<Float, Spectrum>::Point3f
Scene<Float, Spectrum>::invert_silhouette_sample(const SilhouetteSample3f &ss,
                                                 Mask active) const {
    MI_MASK_ARGUMENT(active);

    Point3f sample = ss.shape->invert_silhouette_sample(ss, active);

    // Inverse mapping of samples on shapes that have both types
    Mask both_types_sampled =
        ss.flags == (uint32_t) DiscontinuityFlags::AllTypes;
    Mask shape_has_both_types =
        ss.shape->silhouette_discontinuity_types() == (uint32_t) DiscontinuityFlags::AllTypes;
    Mask is_interior =
        has_flag(ss.discontinuity_type, DiscontinuityFlags::InteriorType);
    dr::masked(sample.x(), both_types_sampled && shape_has_both_types) =
        dr::select(is_interior,
                   sample.x() * 0.5f,
                   sample.x() * 0.5f + 0.5f);

    if (m_silhouette_shapes.size() == 1)
        return sample;

    // Inverse mapping of samples w.r.t. scene
    Float cdf = m_silhouette_distr->eval_cdf_normalized(ss.scene_index, active);
    Float normalization = m_silhouette_distr->normalization();
    Float weight = ss.shape->silhouette_sampling_weight();
    Float offset = cdf - weight * normalization;
    sample.x() = sample.x() * weight * normalization + offset;

    return sample;
}

MI_VARIANT void Scene<Float, Spectrum>::traverse(TraversalCallback *callback) {
    for (auto& child : m_children) {
        std::string id = child->id();
        if (id.empty() || string::starts_with(id, "_unnamed_"))
            id = child->class_()->name();
        callback->put_object(id, child.get(), +ParamFlags::Differentiable);
    }
}

MI_VARIANT void Scene<Float, Spectrum>::parameters_changed(const std::vector<std::string> &/*keys*/) {
    if (m_environment)
        m_environment->set_scene(this); // TODO use parameters_changed({"scene"})

    bool accel_is_dirty = false;
    for (auto &s : m_shapes) {
        if (s->dirty()) {
            accel_is_dirty = true;
            break;
        }
    }

    for (auto &s : m_shapegroups) {
        if (s->dirty()) {
            accel_is_dirty = true;
            break;
        }
    }

    if (accel_is_dirty) {
        if constexpr (dr::is_cuda_v<Float>)
            accel_parameters_changed_gpu();
        else
            accel_parameters_changed_cpu();

        m_bbox = {};
        for (auto &s : m_shapes)
            m_bbox.expand(s->bbox());
    }

    // Check whether any shape parameters have gradient tracking enabled
    m_shapes_grad_enabled = false;
    for (auto &s : m_shapes) {
        m_shapes_grad_enabled |= s->parameters_grad_enabled();
        if (m_shapes_grad_enabled) {
            update_silhouette_sampling_distribution();
            break;
        }
    }

    // Check if emitters were modified and we potentially need to update
    // the emitter sampling distribution.
    for (auto &e : m_emitters) {
        if (e->dirty()) {
            update_emitter_sampling_distribution();
            break;
        }
    }
}

MI_VARIANT std::string Scene<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
    oss << "Scene[" << std::endl
        << "  children = [" << std::endl;
    for (size_t i = 0; i < m_children.size(); ++i) {
        oss << "    " << string::indent(m_children[i], 4);
        if (i + 1 < m_children.size())
            oss << ",";
        oss <<  std::endl;
    }
    oss << "  ]"<< std::endl
        << "]";
    return oss.str();
}

MI_VARIANT void Scene<Float, Spectrum>::static_accel_initialization() {
    if constexpr (dr::is_cuda_v<Float>)
        Scene::static_accel_initialization_gpu();
    else
        Scene::static_accel_initialization_cpu();
}

MI_VARIANT void Scene<Float, Spectrum>::static_accel_shutdown() {
    if constexpr (dr::is_cuda_v<Float>)
        Scene::static_accel_shutdown_gpu();
    else
        Scene::static_accel_shutdown_cpu();
}

MI_VARIANT void Scene<Float, Spectrum>::clear_shapes_dirty() {
    for (auto &s : m_shapes)
        s->m_dirty = false;
    for (auto &s : m_shapegroups)
        s->m_dirty = false;
}

MI_VARIANT void Scene<Float, Spectrum>::static_accel_initialization_cpu() { }
MI_VARIANT void Scene<Float, Spectrum>::static_accel_shutdown_cpu() { }

void librender_nop() { }

#if !defined(MI_ENABLE_CUDA)
MI_VARIANT void Scene<Float, Spectrum>::accel_init_gpu(const Properties &) {
    NotImplementedError("accel_init_gpu");
}
MI_VARIANT void Scene<Float, Spectrum>::accel_parameters_changed_gpu() {
    NotImplementedError("accel_parameters_changed_gpu");
}
MI_VARIANT void Scene<Float, Spectrum>::accel_release_gpu() {
    NotImplementedError("accel_release_gpu");
}
MI_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_gpu(const Ray3f &, Mask) const {
    NotImplementedError("ray_intersect_preliminary_gpu");
}
MI_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_gpu(const Ray3f &, uint32_t, Mask) const {
    NotImplementedError("ray_intersect_naive_gpu");
}
MI_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_gpu(const Ray3f &, Mask) const {
    NotImplementedError("ray_test_gpu");
}
MI_VARIANT void Scene<Float, Spectrum>::static_accel_initialization_gpu() { }
MI_VARIANT void Scene<Float, Spectrum>::static_accel_shutdown_gpu() { }
#endif

MI_IMPLEMENT_CLASS_VARIANT(Scene, Object, "scene")
MI_INSTANTIATE_CLASS(Scene)
NAMESPACE_END(mitsuba)
