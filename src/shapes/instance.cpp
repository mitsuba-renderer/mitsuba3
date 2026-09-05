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

.. _shape-instance:

Instance (:monosp:`instance`)
-------------------------------------------------

.. pluginparameters::

 * - (Nested plugin)
   - :paramtype:`shapegroup`
   - A reference to a shape group that should be instantiated.

 * - to_world
   - |transform|
   - Specifies a linear object-to-world transformation. (Default: none (i.e. object space = world space))
   - |exposed|, |differentiable|, |discontinuous|

This plugin implements a geometry instance used to efficiently replicate geometry many times. For
details on how to create instances, refer to the :ref:`shape-shapegroup` plugin.

    .. image:: ../../resources/data/docs/images/render/shape_instance_fractal.jpg
        :width: 100%
        :align: center

    The Stanford bunny loaded a single time and instantiated 1365 times (equivalent to 100 million
    triangles)

.. warning::

    - Note that it is not possible to assign a different material to each instance. The material
      assignment specified within the shape group is the one that matters.
    - Shape groups cannot be used to replicate shapes with attached emitters, sensors, or
      subsurface scattering models.

 */

template <typename Float, typename Spectrum>
class Instance final: public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_to_world, m_shape_type,
                   mark_dirty, to_world_scalar)
    MI_IMPORT_TYPES(BSDF)

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using ShapeGroup_ = ShapeGroup<Float, Spectrum>;

    Instance(const Properties &props) : Base(props) {
        for (auto &prop : props.objects()) {
            // An <animation> arrives as an object property; Shape's constructor
            // already consumed it
            if (prop.try_get<AnimatedTransform4f>())
                continue;

            ShapeGroup_ *shapegroup = prop.try_get<ShapeGroup_>();
            if (!shapegroup)
                Throw("Only a shapegroup can be specified in an instance.");
            if (m_shapegroup)
                Throw("Only a single shapegroup can be specified per instance.");
            m_shapegroup = shapegroup;
        }

        if (!m_shapegroup)
            Throw("A reference to a 'shapegroup' must be specified!");

        m_shape_type = ShapeType::Instance;

        m_to_world->ensure_uniform_keyframes();
        // 'instance' does not call Shape::initialize(), which is where the
        // other shapes request this
        m_to_world->make_transform_opaque();
    }

    void traverse(TraversalCallback *cb) override {
        cb->put("to_world", m_to_world,
                ParamFlags::Differentiable | ParamFlags::Discontinuous);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (keys.empty() || string::contains(keys, "to_world")) {
            m_to_world->ensure_uniform_keyframes();
            mark_dirty();
        }
        Base::parameters_changed(keys);
    }

    ScalarBoundingBox3f bbox() const override {
        const ScalarBoundingBox3f &bbox = m_shapegroup->bbox();

        // If the shape group is empty, return the invalid bbox
        if (!bbox.valid())
            return bbox;

        // Union of the instance bounds across all keyframes
        return m_to_world->get_spatial_bounds(bbox);
    }

    ScalarSize primitive_count() const override { return 1; }

    ScalarSize effective_primitive_count() const override {
        return m_shapegroup->primitive_count();
    }

    // =============================================================

    // =============================================================
    // Ray tracing routines
    // =============================================================

    template <typename FloatP, typename Ray3fP>
    std::tuple<dr::mask_t<FloatP>, FloatP, Point<FloatP, 2>,
               dr::uint32_array_t<FloatP>, dr::uint32_array_t<FloatP>>
    ray_intersect_preliminary_impl(const Ray3fP &ray,
                                   ScalarIndex /*prim_index*/,
                                   dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);
        if constexpr (!dr::is_array_v<FloatP>) {
            return m_shapegroup->ray_intersect_preliminary_scalar(m_to_world->eval_scalar(ray.time).inverse() * ray);
        } else {
            Throw("Instance::ray_intersect_preliminary() should only be called with scalar types.");
        }
    }

    template <typename FloatP, typename Ray3fP>
    dr::mask_t<FloatP> ray_test_impl(const Ray3fP &ray,
                                     ScalarIndex /*prim_index*/,
                                     dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);

        if constexpr (!dr::is_array_v<FloatP>) {
            return m_shapegroup->ray_test_scalar(m_to_world->eval_scalar(ray.time).inverse() * ray);
        } else {
            Throw("Instance::ray_test_impl() should only be called with scalar types.");
        }
    }

    MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    // =============================================================

    std::string to_string() const override {
        std::ostringstream oss;
            oss << "Instance[" << std::endl
                << "  shapegroup = " << string::indent(m_shapegroup) << std::endl
                << "  to_world = " << string::indent(m_to_world, 13) << "," << std::endl
                << "]";
        return oss.str();
    }

    bool parameters_grad_enabled() const override {
        return m_to_world->parameters_grad_enabled() || m_shapegroup->parameters_grad_enabled();
    }

    void describe(ShapeIR &g) const override {
        g.kind = ShapeIR::Kind::Instance;
        g.type = m_shape_type;
        g.ctx = this;

        // For animated instances, emit one decomposed keyframe per to_world
        // keyframe. Backends that support motion blur consume these; the
        // packed to_world below remains the t=0 fallback.
        if (m_to_world->is_animated()) {
            for (const auto &[time, kf] : m_to_world->keyframes()) {
                KeyframeIR kf_ir;
                kf_ir.time = (float) time;
                kf_ir.scale[0] = (float) kf.S.x();
                kf_ir.scale[1] = (float) kf.S.y();
                kf_ir.scale[2] = (float) kf.S.z();
                kf_ir.quat[0] = (float) kf.Q.w();
                kf_ir.quat[1] = (float) kf.Q.x();
                kf_ir.quat[2] = (float) kf.Q.y();
                kf_ir.quat[3] = (float) kf.Q.z();
                kf_ir.trans[0] = (float) kf.T.x();
                kf_ir.trans[1] = (float) kf.T.y();
                kf_ir.trans[2] = (float) kf.T.z();
                g.keyframes.push_back(kf_ir);
            }
        }

        // Column-major 3x4 affine (to_world[col*3 + row]). Each backend repacks
        // into its instance-descriptor convention.
        const auto &M = to_world_scalar().matrix;
        for (size_t col = 0; col < 4; ++col)
            for (size_t row = 0; row < 3; ++row)
                g.to_world[col * 3 + row] = (float) M(row, col);
        // Stable id for caching one BLAS set per ShapeGroup across Instances
        g.group_id = (const void *) m_shapegroup.get();
    }

    MI_DECLARE_CLASS(Instance)
private:
   ref<ShapeGroup_> m_shapegroup;

   MI_TRAVERSE_CB(Base, m_shapegroup)
};

MI_EXPORT_PLUGIN(Instance)
NAMESPACE_END(mitsuba)
