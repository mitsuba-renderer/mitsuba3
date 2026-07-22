#include <mitsuba/core/fwd.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/scene_ir.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/shapegroup.h>

#if defined(MI_ENABLE_EMBREE)
    #include <embree3/rtcore.h>
#endif

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-mergeinstance:

MergeInstance (:monosp:`mergeinstance`)
-------------------------------------------------

Internal shape plugin representing a batch of instances that share the same
ShapeGroup.  Instead of registering many individual ``Instance`` shapes
(each becoming a separate JIT object), a single ``MergeInstance`` stores all
transforms in a dynamic buffer and uses ``dr::gather`` to look up the
per-hit transform during shading.

This plugin is not intended to be specified by the user directly.  The parser
automatically merges compatible ``Instance`` primitives into a
``MergeInstance`` when the scene is loaded.

*/

template <typename Float, typename Spectrum>
class MergeInstance final: public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_to_world, m_shape_type, mark_dirty)
    MI_IMPORT_TYPES(BSDF)

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using ShapeGroup_ = ShapeGroup<Float, Spectrum>;

    MergeInstance(const Properties &props) : Base(props) {
        for (auto &prop : props.objects()) {
            ShapeGroup_ *shapegroup = prop.try_get<ShapeGroup_>();
            if (!shapegroup)
                Throw("Only a shapegroup can be specified in a mergeinstance.");
            if (m_shapegroup)
                Throw("Only a single shapegroup can be specified per mergeinstance.");
            m_shapegroup = shapegroup;
        }

        if (!m_shapegroup)
            Throw("A reference to a 'shapegroup' must be specified!");

        m_shape_type = ShapeType::MergeInstance;

        // Load transforms from properties.  The parser stores them as
        // "to_world_0", "to_world_1", ...
        size_t idx = 0;
        while (true) {
            std::string key = tfm::format("to_world_%zu", idx);
            if (!props.has_property(key))
                break;
            m_scalar_transforms.push_back(
                props.get<ScalarAffineTransform4f>(key));
            ++idx;
        }

        if (m_scalar_transforms.empty())
            Throw("MergeInstance: at least one transform is required.");

        upload_transforms();

        Log(Info, "MergeInstance: merged %zu instances.", m_scalar_transforms.size());
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("shapegroup", m_shapegroup, ParamFlags::NonDifferentiable);
        cb->put("transforms", m_transforms, ParamFlags::Differentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (keys.empty() || string::contains(keys, "transforms")) {
            update_scalar_transforms();
            mark_dirty();
        }
        Base::parameters_changed();
    }

    ScalarBoundingBox3f bbox() const override {
        const ScalarBoundingBox3f &group_bbox = m_shapegroup->bbox();
        if (!group_bbox.valid())
            return group_bbox;

        ScalarBoundingBox3f result;
        for (const auto &trafo : m_scalar_transforms) {
            for (int i = 0; i < 8; ++i)
                result.expand(trafo * group_bbox.corner(i));
        }
        return result;
    }

    ScalarSize primitive_count() const override { return 1; }

    ScalarSize effective_primitive_count() const override {
        return (ScalarSize) m_scalar_transforms.size() *
               m_shapegroup->primitive_count();
    }

    // =============================================================
    //  Ray tracing routines
    // =============================================================

    template <typename FloatP, typename Ray3fP>
    std::tuple<dr::mask_t<FloatP>, FloatP, Point<FloatP, 2>,
               dr::uint32_array_t<FloatP>, dr::uint32_array_t<FloatP>>
    ray_intersect_preliminary_impl(const Ray3fP &ray,
                                   ScalarIndex prim_index,
                                   dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);
        if constexpr (!dr::is_array_v<FloatP>) {
            if (prim_index < m_scalar_transforms.size()) {
                // Direct query for a specific instance index (prim_index)
                auto local_ray = m_scalar_transforms[prim_index].inverse() * ray;
                return m_shapegroup->ray_intersect_preliminary_scalar(local_ray);
            }

            // Fallback for unindexed scalar queries across all instances
            FloatP best_t = dr::Infinity<FloatP>;
            std::tuple<bool, FloatP, Point<FloatP, 2>,
                       dr::uint32_array_t<FloatP>,
                       dr::uint32_array_t<FloatP>> best{false, best_t, {0.f, 0.f}, 0u, 0u};

            for (size_t i = 0; i < m_scalar_transforms.size(); ++i) {
                auto [hit_box, mint, maxt] = m_scalar_bboxes[i].ray_intersect(ray);
                if (!hit_box || mint >= best_t)
                    continue;

                auto local_ray = m_scalar_transforms[i].inverse() * ray;
                auto [valid, t, uv, si_idx, pi_idx] =
                    m_shapegroup->ray_intersect_preliminary_scalar(local_ray);
                if (valid && t < best_t) {
                    best_t = t;
                    best = {true, t, uv, si_idx, pi_idx};
                }
            }
            return best;
        } else {
            Throw("MergeInstance::ray_intersect_preliminary() should only "
                  "be called with scalar types.");
        }
    }

    template <typename FloatP, typename Ray3fP>
    dr::mask_t<FloatP> ray_test_impl(const Ray3fP &ray,
                                     ScalarIndex prim_index,
                                     dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);
        if constexpr (!dr::is_array_v<FloatP>) {
            if (prim_index < m_scalar_transforms.size()) {
                auto local_ray = m_scalar_transforms[prim_index].inverse() * ray;
                return m_shapegroup->ray_test_scalar(local_ray);
            }

            for (size_t i = 0; i < m_scalar_transforms.size(); ++i) {
                if (!std::get<0>(m_scalar_bboxes[i].ray_intersect(ray)))
                    continue;

                if (m_shapegroup->ray_test_scalar(
                        m_scalar_transforms[i].inverse() * ray))
                    return true;
            }
            return false;
        } else {
            Throw("MergeInstance::ray_test_impl() should only be called "
                  "with scalar types.");
        }
    }

    MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     uint32_t recursion_depth,
                                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        // Nested instancing is not supported
        if (recursion_depth > 0)
            return dr::zeros<SurfaceInteraction3f>();

        // Gather the per-instance transform and compute its inverse
        AffineTransform4f to_world  = gather_transform(pi.instance_index, active);
        AffineTransform4f to_object = to_world.inverse();

        constexpr bool IsDiff = dr::is_diff_v<Float>;
        bool grad_enabled = dr::grad_enabled(m_transforms);

        if constexpr (IsDiff) {
            if (grad_enabled && m_shapegroup->parameters_grad_enabled())
                Throw("Cannot differentiate batch instance parameters and "
                      "shapegroup internal parameters at the same time!");
        }

        bool detach_shape = has_flag(ray_flags, RayFlags::DetachShape);
        bool follow_shape = has_flag(ray_flags, RayFlags::FollowShape);

        dr::suspend_grad<Float> scope(detach_shape, to_world, to_object);

        SurfaceInteraction3f si;
        {
            dr::suspend_grad<Float> scope2(grad_enabled);
            si = m_shapegroup->compute_surface_interaction(
                to_object * ray, pi, ray_flags,
                recursion_depth, active);
        }

        // Transform back to world space
        si.p = to_world * si.p;
        si.n = dr::normalize(dr::detach(to_world) * si.n);
        if (likely(has_flag(ray_flags, RayFlags::ShadingFrame)))
            si.sh_frame.n = dr::normalize(dr::detach(to_world) * si.sh_frame.n);

        if constexpr (IsDiff) {
            if (follow_shape && grad_enabled) {
                si.t = dr::sqrt(dr::squared_norm(si.p - ray.o) /
                                dr::squared_norm(ray.d));
            } else if (!follow_shape && grad_enabled) {
                si.t = (dr::dot(si.n, si.p) - dr::dot(si.n, ray.o)) /
                       dr::dot(si.n, ray.d);
                si.p = ray(si.t);
            }
        }

        if (likely(has_flag(ray_flags, RayFlags::ShadingFrame)))
            si.initialize_sh_frame();

        if (likely(has_flag(ray_flags, RayFlags::dPdUV))) {
            si.dp_du = to_world * si.dp_du;
            si.dp_dv = to_world * si.dp_dv;
        }

        if (has_flag(ray_flags, RayFlags::dNGdUV) ||
            has_flag(ray_flags, RayFlags::dNSdUV)) {
            Normal3f n = has_flag(ray_flags, RayFlags::dNGdUV)
                             ? si.n
                             : si.sh_frame.n;

            Normal3f tn = to_world * dr::normalize(to_object * n);
            Float inv_len = dr::rcp(dr::norm(tn));
            tn *= inv_len;

            si.dn_du = to_world * Normal3f(si.dn_du) * inv_len;
            si.dn_dv = to_world * Normal3f(si.dn_dv) * inv_len;

            si.dn_du -= tn * dr::dot(tn, si.dn_du);
            si.dn_dv -= tn * dr::dot(tn, si.dn_dv);
        }

        si.prim_index = pi.prim_index;
        si.instance = this;

        return si;
    }

    //! @}
    // =============================================================

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "MergeInstance[" << std::endl
            << "  shapegroup = " << string::indent(m_shapegroup) << std::endl
            << "  instance_count = " << m_scalar_transforms.size()
            << std::endl << "]";
        return oss.str();
    }

    bool parameters_grad_enabled() const override {
        return dr::grad_enabled(m_transforms) ||
               m_shapegroup->parameters_grad_enabled();
    }

    void describe(ShapeIR &g) const override {
        g.kind = ShapeIR::Kind::MergeInstance;
        g.type = m_shape_type;
        g.ctx = this;
        g.group_id = (const void *) m_shapegroup.get();

        g.batch_to_worlds.resize(m_scalar_transforms.size());
        for (size_t i = 0; i < m_scalar_transforms.size(); ++i) {
            const auto &M = m_scalar_transforms[i].matrix;
            for (size_t col = 0; col < 4; ++col)
                for (size_t row = 0; row < 3; ++row)
                    g.batch_to_worlds[i][col * 3 + row] = (float) M(row, col);
        }
    }

    /// Number of instances in the batch.
    size_t instance_count() const { return m_scalar_transforms.size(); }

    MI_DECLARE_CLASS(MergeInstance)
private:
    /// Gather the affine transform for the given batch index.
    AffineTransform4f gather_transform(const UInt32 &idx,
                                       const Mask &active) const {
        UInt32 vec_idx = idx * 4u;
        Vector3f c0 = dr::gather<Vector3f>(m_transforms, vec_idx + 0u, active);
        Vector3f c1 = dr::gather<Vector3f>(m_transforms, vec_idx + 1u, active);
        Vector3f c2 = dr::gather<Vector3f>(m_transforms, vec_idx + 2u, active);
        Vector3f c3 = dr::gather<Vector3f>(m_transforms, vec_idx + 3u, active);

        Matrix4f mat(
            c0.x(), c1.x(), c2.x(), c3.x(),
            c0.y(), c1.y(), c2.y(), c3.y(),
            c0.z(), c1.z(), c2.z(), c3.z(),
            0.f,    0.f,    0.f,    1.f
        );

        return AffineTransform4f(mat);
    }

    /// Upload scalar transforms to the JIT buffer (12 floats per 3x4 matrix).
    void upload_transforms() {
        using Scalar = dr::scalar_t<Float>;
        size_t n = m_scalar_transforms.size();
        std::vector<Scalar> data(n * 12);

        for (size_t i = 0; i < n; ++i) {
            const auto &Mw = m_scalar_transforms[i].matrix;
            for (size_t col = 0; col < 4; ++col) {
                for (size_t row = 0; row < 3; ++row) {
                    data[i * 12 + col * 3 + row] = (Scalar) Mw(row, col);
                }
            }
        }

        m_transforms =
            dr::load<DynamicBuffer<Float>>(data.data(), data.size());
        dr::make_opaque(m_transforms);
        update_scalar_bboxes();
    }

    /// Sync updated JIT transforms buffer back to host scalar transforms.
    void update_scalar_transforms() {
        dr::eval(m_transforms);
        using Scalar = dr::scalar_t<Float>;
        std::vector<Scalar> data(m_transforms.size());
        dr::store(data.data(), m_transforms);
        size_t n = m_scalar_transforms.size();
        for (size_t i = 0; i < n; ++i) {
            ScalarMatrix4f Mw(1.f);
            for (size_t col = 0; col < 4; ++col) {
                for (size_t row = 0; row < 3; ++row) {
                    Mw(row, col) = data[i * 12 + col * 3 + row];
                }
            }
            m_scalar_transforms[i] = ScalarAffineTransform4f(Mw);
        }
        update_scalar_bboxes();
    }

    void update_scalar_bboxes() {
        size_t n = m_scalar_transforms.size();
        m_scalar_bboxes.resize(n);
        const ScalarBoundingBox3f &group_bbox = m_shapegroup->bbox();
        if (group_bbox.valid()) {
            for (size_t i = 0; i < n; ++i) {
                ScalarBoundingBox3f b;
                for (int c = 0; c < 8; ++c)
                    b.expand(m_scalar_transforms[i] * group_bbox.corner(c));
                m_scalar_bboxes[i] = b;
            }
        } else {
            for (size_t i = 0; i < n; ++i)
                m_scalar_bboxes[i] = group_bbox;
        }
    }

    ref<ShapeGroup_> m_shapegroup;
    std::vector<ScalarAffineTransform4f> m_scalar_transforms;
    std::vector<ScalarBoundingBox3f> m_scalar_bboxes;
    DynamicBuffer<Float> m_transforms;
};

MI_EXPORT_PLUGIN(MergeInstance)
NAMESPACE_END(mitsuba)
