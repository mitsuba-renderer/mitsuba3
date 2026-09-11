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

#if defined(MI_ENABLE_METAL)
#  include "scene_metal.inl"
#endif

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT Scene<Float, Spectrum>::Scene(const Properties &props)
    : JitObject<Scene>(props.id()) {
    m_thread_reordering = props.get<bool>("allow_thread_reordering", true);
    m_compact_accel = props.get<bool>("compact_accel", true);
    m_compact_accel_auto = !props.has_property("compact_accel");
    m_portal_data.weight = props.get<ScalarFloat>("portal_weight", .5f);
    if (m_portal_data.weight < 0.f || m_portal_data.weight > 1.f)
        Throw("'portal_weight' must be in [0, 1]!");

    for (auto &prop : props.objects()) {
        ref<Object> v = prop.get<ref<Object>>();

        Mesh *mesh             = dynamic_cast<Mesh *>(v.get());
        Emitter *emitter       = dynamic_cast<Emitter *>(v.get());
        Sensor *sensor         = dynamic_cast<Sensor *>(v.get());
        Integrator *integrator = dynamic_cast<Integrator *>(v.get());

        if (Scene *scene = dynamic_cast<Scene *>(v.get())) {
            // Skip nested scenes in children list
        } else {
            m_children.push_back(v.get());
        }

        if (Shape *shape = dynamic_cast<Shape *>(v.get())) {
            if (shape->is_emitter())
                m_emitters.push_back(shape->emitter());
            if (shape->is_sensor())
                m_sensors.push_back(shape->sensor());
            if (shape->is_shape_group()) {
                m_shapegroups.push_back((ShapeGroup*)shape);
            } else {
                m_bbox.expand(shape->bbox());
                m_shapes.push_back(shape);
                if (shape->is_instance())
                    m_instances.push_back(shape);
            }
            if (mesh)
                mesh->set_scene(this);
        } else if (emitter) {
            if (emitter->is_portal()) {
                m_portals.push_back(emitter);
                continue;
            }

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

    // Mark backend-specific properties as queried
    props.mark_queried("embree_use_robust_intersections");
    props.mark_queried("kd_intersection_cost");
    props.mark_queried("kd_traversal_cost");
    props.mark_queried("kd_empty_space_bonus");
    props.mark_queried("kd_stop_prims");
    props.mark_queried("kd_max_depth");
    props.mark_queried("kd_min_max_bins");
    props.mark_queried("kd_clip");
    props.mark_queried("kd_retract_bad_splits");
    props.mark_queried("kd_exact_primitive_threshold");

    // Only emitters that rays can intersect have a meaningful visibility.
    // Plugin constructors assign the flags after the base class has run,
    // which is why this check does not live in the Emitter constructor.
    for (Emitter *emitter : m_emitters) {
        if (!emitter->shape() && !emitter->is_environment() &&
            emitter->m_visibility != ShapeVisibility::All)
            Throw("Emitter \"%s\": the 'visibility' property only applies to "
                  "emitters that rays can intersect (area and environment "
                  "emitters).", emitter->id());
    }

    // Implement the deprecated "hide_emitters" flag by hiding every emitter
    // from camera rays before the acceleration data structures bake the
    // masks. The shape owns the property when the emitter has one.
    if (m_integrator && m_integrator->hide_emitters()) {
        for (Emitter *emitter : m_emitters) {
            ShapeVisibility &visibility = emitter->shape()
                ? emitter->shape()->m_visibility : emitter->m_visibility;
            if (visibility == ShapeVisibility::Primary)
                visibility = ShapeVisibility::Hidden;
            else if (visibility == ShapeVisibility::All)
                visibility = ShapeVisibility::Secondary;
        }
    }

    // Release transient buffers created while loading the shapes
    if constexpr (dr::is_jit_v<Float>)
        if (!jit_flag(JitFlag::FreezingScope))
            jit_flush_malloc_cache();

    m_accel.init(this, props);
    clear_shapes_dirty();
    update_instance_transforms();
    update_portal_data();

    // Set up the UV parameterization mesh emitters if needed
    for (Shape *shape : m_shapes) {
        Mesh *mesh = dynamic_cast<Mesh *>(shape);
        if (mesh && mesh->needs_parameterization())
            mesh->build_parameterization();
    }

    if (!m_emitters.empty()) {
        // Inform environment emitters etc. about the scene bounds
        for (Emitter *emitter: m_emitters)
            emitter->set_scene(this);
    }

    if constexpr (dr::is_jit_v<Float>) {
        // Mitsuba resolves 1-level instancing directly within the scene
        // class. Traced calls don't need to ever reach 'instance' and
        // 'shapegroup' shapes, so we just deregister them from the JIT here.
        std::unique_ptr<uint32_t[]> ids(new uint32_t[m_shapes.size()]);
        for (size_t i = 0; i < m_shapes.size(); ++i) {
            Shape *shape = m_shapes[i];
            uint32_t index = 0;
            if (shape->is_instance())
                shape->unregister();
            else
                index = jit_registry_id(shape);
            ids[i] = index;
        }
        for (ShapeGroup *group : m_shapegroups)
            group->unregister();
        m_shapes_dr = dr::reinterpret_array<DynamicBuffer<ShapePtr>>(
            dr::load<DynamicBuffer<UInt32>>(ids.get(), m_shapes.size()));
    } else {
        m_shapes_dr = dr::load<DynamicBuffer<ShapePtr>>(
            m_shapes.data(), m_shapes.size());
    }

    m_emitters_dr = dr::load<DynamicBuffer<EmitterPtr>>(
        m_emitters.data(), m_emitters.size());

    m_sensors_dr = dr::load<DynamicBuffer<SensorPtr>>(
        m_sensors.data(), m_sensors.size());

    dr::eval(m_emitters_dr, m_shapes_dr, m_sensors_dr);

    update_emitter_sampling_distribution();
    update_silhouette_sampling_distribution();

    m_shapes_grad_enabled = false;

    // Release the scratch space of the acceleration structure build
    if constexpr (dr::is_jit_v<Float>)
        if (!jit_flag(JitFlag::FreezingScope))
            jit_flush_malloc_cache();
}

MI_VARIANT
void Scene<Float, Spectrum>::update_emitter_sampling_distribution() {
    // Check if we need to use non-uniform emitter sampling.
    bool non_uniform_sampling = false;
    ScalarFloat weight_sum = 0.f;
    for (auto &e : m_emitters) {
        ScalarFloat weight = e->sampling_weight();
        non_uniform_sampling |= weight != ScalarFloat(1.0);
        weight_sum += weight;
    }

    // Emitters hidden from secondary rays have a zero weight. A scene where
    // this applies to every emitter has nothing left to sample.
    size_t n_emitters = weight_sum != 0.f ? m_emitters.size() : 0;
    m_emitter_pmf = n_emitters != 0 ? 1.f / n_emitters : 0.f;

    if (non_uniform_sampling && n_emitters != 0) {
        std::unique_ptr<ScalarFloat[]> sample_weights(new ScalarFloat[n_emitters]);
        for (size_t i = 0; i < n_emitters; ++i)
            sample_weights[i] = m_emitters[i]->sampling_weight();
        m_emitter_distr = std::make_unique<DiscreteDistribution<Float>>(
            sample_weights.get(), n_emitters);
    } else {
        // By default use uniform sampling with constant PMF
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
    // Release the acceleration structure first (it may sync_thread to ensure
    // no ray-tracing kernel still references the shapes about to be freed).
    // ``release()`` is idempotent; the m_accel destructor calls it again.
    m_accel.release();

    // Trigger deallocation of all instances
    m_emitters.clear();
    m_shapes.clear();
    m_shapegroups.clear();
    m_portals.clear();
    m_sensors.clear();
    m_children.clear();
    m_integrator = nullptr;
    m_environment = nullptr;
}

// -----------------------------------------------------------------------

/// Stash the 3x4 affine part of a transformation into a flat list
template <typename Value, typename Transform>
static void pack_matrix(Value *rec, const Transform &t) {
    for (size_t col = 0; col < 4; ++col)
        for (size_t row = 0; row < 3; ++row)
            rec[col * 3 + row] = t.matrix(row, col);
}

/// Reassemble a transformation from a flat column-major 3x4 list
template <typename AffineTransform4f, typename Rec>
static AffineTransform4f unpack_matrix(const Rec &rec) {
    using Matrix = typename AffineTransform4f::Matrix;
    return AffineTransform4f(Matrix(
        rec[0], rec[3], rec[6], rec[9],
        rec[1], rec[4], rec[7], rec[10],
        rec[2], rec[5], rec[8], rec[11],
        0.f,    0.f,    0.f,    1.f));
}

MI_VARIANT void Scene<Float, Spectrum>::update_portal_data() {
    size_t n = m_portals.size();
    std::unique_ptr<ScalarFloat[]> data(new ScalarFloat[12 * n]);

    for (size_t i = 0; i < n; ++i) {
        const ScalarAffineTransform4f &t = m_portals[i]->world_transform_scalar();
        ScalarVector3f rec[4] = {
            t * ScalarPoint3f(0.f),
            t * ScalarVector3f(1.f, 0.f, 0.f),
            t * ScalarVector3f(0.f, 1.f, 0.f),
            dr::normalize(t * ScalarNormal3f(0.f, 0.f, 1.f))
        };
        for (size_t j = 0; j < 4; ++j)
            for (size_t k = 0; k < 3; ++k)
                data[12 * i + 3 * j + k] = rec[j][k];
    }

    m_portal_data.records = dr::load<DynamicBuffer<Float>>(data.get(), 12 * n);
    m_portal_data.count   = (uint32_t) n;
    dr::make_opaque(m_portal_data.count);
}

MI_VARIANT void Scene<Float, Spectrum>::update_instance_transforms() {
    // An empty record buffer marks an instance-free scene (the instance list
    // itself is fixed at construction time)
    if (m_instances.empty())
        return;

    // Pack the primal transform data on the host
    size_t n = m_instances.size();
    std::unique_ptr<ScalarFloat[]> data(new ScalarFloat[12 * n]);
    for (size_t i = 0; i < n; ++i)
        pack_matrix(data.get() + 12 * i, m_instances[i]->to_world_scalar());
    m_instance_transforms =
        dr::load<DynamicBuffer<Float>>(data.get(), 12 * n);

    // Overlay the records of differentiated instances so that gradients
    // flow from the packed buffer back to each instance's ``to_world``.
    if constexpr (dr::is_diff_v<Float>) {
        for (size_t i = 0; i < n; ++i) {
            AffineTransform4f t = m_instances[i]->to_world();
            if (!dr::grad_enabled(t))
                continue;
            dr::Array<Float, 12> rec;
            pack_matrix(rec.data(), t);
            dr::scatter(m_instance_transforms, rec, UInt32((uint32_t) i),
                        true, ReduceMode::NoConflicts);
        }
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::compute_surface_interaction(
    const Ray3f &ray, const PreliminaryIntersection3f &pi, uint32_t ray_flags,
    Mask active) const {
    active &= pi.is_valid();
    if (dr::none_or<false>(active)) {
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.wi = -ray.d;
        si.wavelengths = ray.wavelengths;
        return si;
    }

    ScopedPhase sp(ProfilerPhase::CreateSurfaceInteraction);

    // Instance-free scenes only need the plain leaf-shape call
    if (m_instance_transforms.size() == 0) {
        SurfaceInteraction3f si = pi.shape->compute_surface_interaction(
            ray, pi, ray_flags, active);
        si.finalize_surface_interaction(pi, ray, ray_flags, active);
        return si;
    }

    return compute_surface_interaction_instanced(ray, pi, ray_flags, active);
}

MI_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::compute_surface_interaction_instanced(
    const Ray3f &ray, const PreliminaryIntersection3f &pi, uint32_t ray_flags,
    Mask active) const {
    Mask has_inst = active && (pi.instance_index != 0u);

    bool detach_shape = has_flag(ray_flags, RayFlags::DetachShape),
         follow_shape = has_flag(ray_flags, RayFlags::FollowShape),
         grad_enabled = dr::grad_enabled(m_instance_transforms);

    // Move instanced lanes' rays into the local frame of their shape
    // group. Both parameterizations share the same distance value ``t``
    // because the direction is not re-normalized.
    auto [ray_l_o, ray_l_d] = dr::if_stmt(
        std::make_tuple(ray.o, ray.d, pi.instance_index),
        has_inst,

        [this, detach_shape](const Point3f &o, const Vector3f &d,
                             const UInt32 &index) {
            DRJIT_MARK_USED(detach_shape);
            AffineTransform4f to_object =
                unpack_matrix<AffineTransform4f>(
                    dr::gather<dr::Array<Float, 12>>(m_instance_transforms,
                                                     index - 1u)).inverse();

            if constexpr (dr::is_diff_v<Float>) {
                if (detach_shape)
                    to_object = dr::detach(to_object);
            }

            return std::make_pair(Point3f(to_object * o),
                                  Vector3f(to_object * d));
        },

        [](const Point3f &o, const Vector3f &d,
           const UInt32 &) { return std::make_pair(o, d); },

        "Scene::compute_surface_interaction_instanced() [ray transform]");

    Ray3f ray_l = ray;
    ray_l.o = ray_l_o;
    ray_l.d = ray_l_d;

    SurfaceInteraction3f si =
        pi.shape->compute_surface_interaction(ray_l, pi, ray_flags, active);

    si = dr::if_stmt(
        std::make_tuple(si, ray, pi.instance_index),
        has_inst,

        [this, ray_flags, detach_shape, follow_shape, grad_enabled](
            SurfaceInteraction3f si, const Ray3f &ray,
            const UInt32 &index) {
            DRJIT_MARK_USED(detach_shape);
            DRJIT_MARK_USED(follow_shape);
            DRJIT_MARK_USED(grad_enabled);
            AffineTransform4f to_world =
                unpack_matrix<AffineTransform4f>(
                    dr::gather<dr::Array<Float, 12>>(m_instance_transforms, index - 1u));
            if constexpr (dr::is_diff_v<Float>) {
                if (detach_shape)
                    to_world = dr::detach(to_world);
            }

            AffineTransform4f to_world_d = dr::detach(to_world);

            // Hit point `si.p` is only attached to the surface motion
            si.p = to_world * si.p;
            si.n = dr::normalize(to_world_d * si.n);

            if (likely(has_flag(ray_flags, RayFlags::Shading))) {
                // Transforming a normal applies the inverse transpose,
                // which does not preserve its length. Differentiating the
                // re-normalization projects the transformed partials back
                // onto the tangent plane.
                Normal3f n = to_world_d * si.sh_frame.n;
                Float inv_len = dr::rcp(dr::norm(n));
                n *= inv_len;
                si.sh_frame.n = n;

                if (has_flag(ray_flags, RayFlags::NormalPartials)) {
                    Vector3f dn_du = to_world_d * Normal3f(si.dn_du) * inv_len,
                             dn_dv = to_world_d * Normal3f(si.dn_dv) * inv_len;

                    si.dn_du = dr::fnmadd(n, dr::dot(n, dn_du), dn_du);
                    si.dn_dv = dr::fnmadd(n, dr::dot(n, dn_dv), dn_dv);
                }

                // A tangent direction supplied by the nested shape
                // transforms along; finalize_surface_interaction()
                // orthonormalizes it against the transformed normal and
                // derives the bitangent. A mirroring instance transform
                // flips the orientation of the nested parameterization.
                si.sh_frame.s = to_world_d * si.sh_frame.s;
                si.frame_flipped ^=
                    dr::det(Matrix3f(to_world_d.matrix)) < 0.f;

                si.dp_du = to_world * si.dp_du;
                si.dp_dv = to_world * si.dp_dv;
            }

            if constexpr (dr::is_diff_v<Float>) {
                if (follow_shape && grad_enabled) {
                    // Recompute si.t in a differential manner as the
                    // distance between the ray origin and the hit point
                    // following the moving surface.
                    si.t = dr::sqrt(dr::squared_norm(si.p - ray.o) /
                                    dr::squared_norm(ray.d));
                } else if (!follow_shape && grad_enabled) {
                    // Differential recomputation of the intersection of
                    // the ray with the moving plane tangent to the hit
                    // point. In this scenario, it is important that
                    // `si.p` stays along the ray as the surface moves.
                    si.t = (dr::dot(si.n, si.p) - dr::dot(si.n, ray.o)) /
                            dr::dot(si.n, ray.d);
                    si.p = ray(si.t);
                }
            }

            return si;
        },

        [](SurfaceInteraction3f si, const Ray3f &,
           const UInt32 &) { return si; },

        "Scene::compute_surface_interaction_instanced() [world transform]");

    si.instance_index = pi.instance_index;

    si.finalize_surface_interaction(pi, ray, ray_flags, active);
    return si;
}


MI_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect(const Ray3f &ray, uint32_t ray_flags,
                                      Mask coherent, UInt32 ray_mask,
                                      Mask active, bool reorder,
                                      UInt32 reorder_hint,
                                      uint32_t reorder_hint_bits) const {
    MI_MASKED_FUNCTION(ProfilerPhase::RayIntersect, active);
    DRJIT_MARK_USED(coherent);
    DRJIT_MARK_USED(reorder);
    DRJIT_MARK_USED(reorder_hint);
    DRJIT_MARK_USED(reorder_hint_bits);

    // Locate the intersection using the backend, then expand it into a full
    // SurfaceInteraction. This composition is backend-independent.
    PreliminaryIntersection3f pi = m_accel.ray_intersect_preliminary(
        this, ray, coherent, reorder, reorder_hint, reorder_hint_bits, active,
        ray_mask);
    return compute_surface_interaction(ray, pi, ray_flags, active);
}

MI_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary(const Ray3f &ray,
                                                  Mask coherent,
                                                  UInt32 ray_mask,
                                                  Mask active, bool reorder,
                                                  UInt32 reorder_hint,
                                                  uint32_t reorder_hint_bits) const {
    DRJIT_MARK_USED(coherent);
    DRJIT_MARK_USED(reorder);
    DRJIT_MARK_USED(reorder_hint);
    DRJIT_MARK_USED(reorder_hint_bits);

    return m_accel.ray_intersect_preliminary(this, ray, coherent, reorder,
                                             reorder_hint, reorder_hint_bits,
                                             active, ray_mask);
}

MI_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test(const Ray3f &ray, Mask coherent,
                                 UInt32 ray_mask, Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::RayTest, active);
    DRJIT_MARK_USED(coherent);

    return m_accel.ray_test(this, ray, coherent, active, ray_mask,
                            /* skip_null = */ false).occluded;
}

// The helper function relative_grad(x) below, whose Python equivalent is
//
//     def relative_grad(x):
//         return x / dr.detach(x)   # a zero entry of 'x' returns 1 instead
//
// makes it possible to differentiate a loop that computes a product of terms
// using a Dr.Jit sum loop, which requires no trajectory storage.
//
// This uses the fact that the product rule
//
//     d/dt prod_i v_i = (prod_i v_i) * sum_i (dv_i/dt) / v_i
//
// expresses the derivative of a product as a sum, which is exactly what such
// loops accumulate. Since relative_grad() has primal value 1, subtracting 1
// yields a term that carries derivatives alone:
//
//     @dr.syntax
//     def product(x, n):
//         y, rel, i = Float(1), Float(0), UInt(0)
//         while dr.hint(i < n, max_iterations=-1):
//             v = f(x, i)
//             y   *= dr.detach(v)          # detached, as the rules require
//             rel += relative_grad(v) - 1  # sum with zero primal value
//             i   += 1
//         return y * (1 + rel)   # primal from 'y', derivative from 'rel'
template <typename T> static T relative_grad(const T &x) {
    if constexpr (dr::is_diff_v<T>) {
        if (!dr::grad_enabled(x))
            return T(1.f);
        T x0 = dr::detach(x);
        return dr::select(x0 != 0.f, x / x0, T(1.f));
    } else {
        DRJIT_MARK_USED(x);
        return T(1.f);
    }
}

MI_VARIANT Spectrum Scene<Float, Spectrum>::ray_test_tr(
    const Ray3f &ray, uint32_t ray_flags, Mask coherent, UInt32 ray_mask,
    Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::RayTest, active);

    // Without null shapes, this is an ordinary occlusion query. Otherwise,
    // null shapes do not occlude but are reported, and only lanes that
    // crossed one need the walk below.
    auto [occluded, null] = m_accel.ray_test(this, ray, coherent, active,
                                             ray_mask, m_has_null_shapes);

    Spectrum tr(dr::select(occluded, Float(0.f), Float(1.f)));
    if (!m_has_null_shapes)
        return tr;

    Mask unresolved = active && null && !occluded;

    // OptiX and Metal stop at the first hit and cannot tell whether an
    // opaque shape follows a null one. A second query against the opaque
    // shapes settles the lanes in question, which then skip the walk.
    if constexpr (SceneAccel<Float, Spectrum>::stops_at_first_hit) {
        if (dr::any_or<true>(unresolved)) {
            Mask blocked = m_accel.ray_test(
                this, ray, coherent, unresolved,
                ray_mask & (uint32_t) RayMask::Opaque,
                /* skip_null = */ false).occluded;
            dr::masked(tr, blocked) = Spectrum(0.f);
            unresolved &= !blocked;
        }
    }

    if (dr::any_or<true>(unresolved))
        tr *= null_walk(ray, ray_flags, ray_mask & (uint32_t) RayMask::Null,
                        /* stop_at_surface = */ false, unresolved).second;

    return tr;
}

MI_VARIANT std::pair<typename Scene<Float, Spectrum>::SurfaceInteraction3f, Spectrum>
Scene<Float, Spectrum>::ray_intersect_tr(const Ray3f &ray, uint32_t ray_flags,
                                         Mask coherent, UInt32 ray_mask,
                                         Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::RayIntersect, active);

    if (!m_has_null_shapes)
        return { ray_intersect(ray, ray_flags, coherent, ray_mask, active),
                 Spectrum(1.f) };

    auto [pi, tr] = null_walk(ray, ray_flags, ray_mask,
                              /* stop_at_surface = */ true, active);
    return { compute_surface_interaction(ray, pi, ray_flags, active), tr };
}

MI_VARIANT std::pair<typename Scene<Float, Spectrum>::PreliminaryIntersection3f, Spectrum>
Scene<Float, Spectrum>::null_walk(const Ray3f &ray, uint32_t ray_flags,
                                  UInt32 ray_mask, bool stop_at_surface,
                                  Mask active) const {
    // Copy shape differentiation bits from the caller, and compute the
    // default intersection fields
    uint32_t flags = (ray_flags & ((uint32_t) RayFlags::FollowShape |
                                   (uint32_t) RayFlags::DetachShape)) |
                     (uint32_t) RayFlags::Default;

    struct LoopState {
        Ray3f ray;                    //< Input ray, possibly attached, never modified
        Float t;                      //< Distance from 'ray.o' to the next query origin
        Spectrum tr;                  //< Detached transmittance product
        UnpolarizedSpectrum rel;      //< Relative derivative of 'tr' (Dr.Jit sum loop)
        PreliminaryIntersection3f pi; //< Last query result, 't' measured from 'ray.o'
        UInt32 mask;                  //< Ray mask of the queries
        Mask active;                  //< Lanes that are still walking
        DRJIT_STRUCT(LoopState, ray, t, tr, rel, pi, mask, active)
    } ls = { ray, 0.f, Spectrum(1.f), UnpolarizedSpectrum(0.f),
             dr::zeros<PreliminaryIntersection3f>(), ray_mask, active };

    dr::tie(ls) = dr::while_loop(dr::make_tuple(ls),
        [](const LoopState &ls) { return ls.active; },
        [this, flags, stop_at_surface](LoopState &ls) {
            Ray3f query = dr::detach(ls.ray);
            query.o = query(ls.t);
            query.maxt -= ls.t;

            ls.pi = ray_intersect_preliminary(query, false, ls.mask, ls.active);
            ls.pi.t += ls.t;

            if (stop_at_surface)
                ls.active &= ls.pi.is_valid() && ls.pi.shape->has_null() &&
                             !ls.pi.shape->is_emitter();
            else
                ls.active &= ls.pi.is_valid();

            if (dr::none_or<false>(ls.active))
                return;

            // Evaluating the crossing along the input ray attaches it
            // according to 'flags'
            SurfaceInteraction3f si =
                compute_surface_interaction(ls.ray, ls.pi, flags, ls.active);

            Spectrum value = si.bsdf()->eval_null(si, ls.active);
            if constexpr (is_polarized_v<Spectrum>)
                value = si.to_world_mueller(value, si.wi, si.wi);

            // The masks here protect lanes that ended during this iteration
            value = dr::select(ls.active, value, Spectrum(1.f));
            ls.tr *= dr::detach(value);

            // In polarized modes, the accumulated derivative is wrong. The
            // factors are then Mueller matrices that do not commute, and only
            // their (0, 0) entry enters 'rel'. Primal values stay correct.
            if constexpr (dr::is_diff_v<Float>)
                ls.rel += relative_grad(unpolarized_spectrum(value)) - 1.f;

            // A crossed shape is not the result of the query. Continue past
            // it with an offset analogous to spawn_ray().
            dr::masked(ls.pi.valid, ls.active) = false;

            Point3f p = dr::detach(si.p);
            Normal3f n = dr::detach(si.n);
            Float mag = (1.f + dr::max(dr::abs(p))) * math::RayEpsilon<Float>,
                  cos_theta = dr::maximum(dr::abs(dr::dot(n, query.d)), 1e-2f);
            ls.t = ls.pi.t + mag / cos_theta;

            ls.active &= ls.t < dr::detach(ls.ray.maxt) &&
                         dr::any(unpolarized_spectrum(ls.tr) != 0.f);
        },
        "Scene::null_walk()", /* max_iterations = */ -1);

    Spectrum tr = ls.tr;
    if constexpr (dr::is_diff_v<Float>)
        tr = tr * Spectrum(1.f + ls.rel);

    return { ls.pi, tr };
}

MI_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_naive(const Ray3f &ray, Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::RayIntersect, active);
    return m_accel.ray_intersect_naive(this, ray, active);
}

// -----------------------------------------------------------------------

MI_VARIANT std::tuple<typename Scene<Float, Spectrum>::UInt32, Float, Float>
Scene<Float, Spectrum>::sample_emitter(Float index_sample, Mask active) const {
    MI_MASKED_FUNCTION(ProfilerPhase::SampleEmitter, active);

    if (m_emitter_distr != nullptr) {
        auto [index, reused_sample, pmf] = m_emitter_distr->sample_reuse_pmf(index_sample);
        return {index, dr::rcp(pmf), reused_sample};
    }

    // No emitters, or none that illuminates the scene
    if (unlikely(m_emitter_pmf == 0.f))
        return { UInt32(-1), 0.f, index_sample };

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
    EmitterPtr emitter{};

    // Don't inline emitter sampling in JIT variants(if there is just a single emitter)
    size_t emitter_count = m_emitter_pmf != 0.f ? m_emitters.size() : 0;
    if (emitter_count > 1 || (emitter_count == 1 && drjit::is_jit_v<Float>)) {
        auto [index, emitter_weight, sample_1_re] = sample_emitter(sample1, active);
        emitter = dr::gather<EmitterPtr>(m_emitters_dr, index, active);

        std::tie(ray, weight) =
            emitter->sample_ray(time, sample_1_re, sample2, sample3, active);

        weight *= emitter_weight;
    } else if (emitter_count == 1) {
        std::tie(ray, weight) =
            m_emitters[0]->sample_ray(time, sample1, sample2, sample3, active);
        emitter = m_emitters[0].get();
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

    // Don't inline emitter sampling in JIT variants(if there is just a single emitter)
    size_t emitter_count = m_emitter_pmf != 0.f ? m_emitters.size() : 0;
    if (emitter_count > 1 || (emitter_count == 1 && drjit::is_jit_v<Float>)) {
        // Randomly pick an emitter
        auto [index, emitter_weight, sample_x_re] = sample_emitter(sample.x(), active);
        sample.x() = sample_x_re;

        // Sample a direction towards the emitter
        EmitterPtr emitter = dr::gather<EmitterPtr>(m_emitters_dr, index, active);
        std::tie(ds, spec) = emitter->sample_direction(ref, sample, active);

        // Account for the discrete probability of sampling this emitter
        ds.pdf *= pdf_emitter(index, active);
        spec *= emitter_weight;
    } else if (emitter_count == 1) {
        // Sample a direction towards the (single) emitter
        std::tie(ds, spec) = m_emitters[0]->sample_direction(ref, sample, active);
    } else {
        return { dr::zeros<DirectionSample3f>(), Spectrum(0.f) };
    }

    active &= (ds.pdf != 0.f);

    if constexpr (dr::is_diff_v<Float>) {
        // Detach the sample and re-attach the weight following the
        // convention described in the class documentation
        if (dr::grad_enabled(ref.p, ds.p, ds.pdf, spec)) {
            UInt32 flags = ds.emitter->flags();
            Mask is_surface  = has_flag(flags, EmitterFlags::Surface),
                 is_infinite = has_flag(flags, EmitterFlags::Infinite);

            Spectrum spec_d = dr::detach(spec);
            ds = dr::detach(ds);

            // The sample is a fixed point seen from the moving reference
            // point. Delta and infinite emitters keep their direction.
            Vector3f d = dr::normalize(ds.p - ref.p);
            ds.d = dr::select(is_infinite, ds.d, d);
            Float G = dr::select(active && is_surface,
                                 dr::abs_dot(ds.n, d) / dr::squared_norm(ds.p - ref.p),
                                 1.f);

            // The weight equals 'Le / ds.pdf'. The additive form keeps the
            // derivative of 'Le' where the radiance is currently zero.
            Spectrum Le = ds.emitter->eval_direction(ref, ds, active);
            Float inv_pdf = dr::select(ds.pdf != 0.f, dr::rcp(ds.pdf), 0.f);
            spec = (spec_d + (Le - dr::detach(Le)) * inv_pdf) * relative_grad(G);
        }
    }

    // Shadow ray that passes through null surfaces and returns their
    // transmittance (zero when occluded)
    if (test_visibility && dr::any_or<true>(active)) {
        Spectrum tr = ray_test_tr(ref.spawn_ray_to(ds.p), +RayFlags::Default,
                                  false, +RayMask::Secondary, active);
        Mask occluded = !dr::any(unpolarized_spectrum(tr) != 0.f);
        spec *= tr;
        dr::masked(ds.pdf, occluded) = 0.f;
    }

    return { ds, spec };
}


MI_VARIANT uint32_t Scene<Float, Spectrum>::shape_types() const {
    uint32_t result = 0;
    for (const Shape *shape : m_shapes)
        result |= (uint32_t) shape->shape_type();
    for (const ShapeGroup *group : m_shapegroups)
        result |= group->shape_types();
    return result;
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

    // This is an escape hatch for any failed sample. Ideally these cases should
    // be resolved directly in each shape's `sample_silhouette`. Just in case,
    // they are caught and ignored here.
    Mask to_ignore =
        (dr::isnan(ss.p.x()) || dr::isnan(ss.p.y()) || dr::isnan(ss.p.z()) ||
         dr::isnan(ss.d.x()) || dr::isnan(ss.d.y()) || dr::isnan(ss.d.z()) ||
         dr::isnan(ss.n.x()) || dr::isnan(ss.n.y()) || dr::isnan(ss.n.z()));
    dr::masked(ss, to_ignore) = dr::zeros<SilhouetteSample3f>();

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

MI_VARIANT void Scene<Float, Spectrum>::traverse(TraversalCallback *cb) {
    cb->put("allow_thread_reordering", m_thread_reordering, ParamFlags::NonDifferentiable);
    for (auto& child : m_children) {
        std::string_view id = child->id();
        if (id.empty() || string::starts_with(id, "_unnamed_"))
            id = "";
        cb->put(id, child, ParamFlags::Differentiable);
    }
}

MI_VARIANT bool Scene<Float, Spectrum>::compact_accel() {
    bool first_build = !m_accel_built;
    m_accel_built = true;

    if (!m_compact_accel_auto)
        return m_compact_accel;
    return first_build;
}

MI_VARIANT void Scene<Float, Spectrum>::parameters_changed(const std::vector<std::string> &/*keys*/) {
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
        m_accel.rebuild(this);
        clear_shapes_dirty();
        update_instance_transforms();

        m_bbox = {};
        for (auto &s : m_shapes)
            m_bbox.expand(s->bbox());
    }

    if (m_environment)
        m_environment->set_scene(this);

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
    SceneAccel<Float, Spectrum>::static_initialization();
}

MI_VARIANT void Scene<Float, Spectrum>::static_accel_shutdown() {
    SceneAccel<Float, Spectrum>::static_shutdown();
}

MI_VARIANT void Scene<Float, Spectrum>::clear_shapes_dirty() {
    bool has_null = false;
    for (auto &s : m_shapes) {
        s->m_dirty = false;
        has_null |= s->has_null();
    }
    for (auto &s : m_shapegroups) {
        s->m_dirty = false;
        // Clear the group's children too (consumed into its accel by the same
        // build); a backend's per-group dirty check relies on this.
        for (auto &c : s->shapes()) {
            const_cast<Shape *>(c.get())->m_dirty = false;
            has_null |= c->has_null();
        }
    }
    m_has_null_shapes = has_null;
}

MI_IMPLEMENT_TRAVERSE_CB(Scene, Object)

MI_INSTANTIATE_CLASS(Scene)
NAMESPACE_END(mitsuba)
