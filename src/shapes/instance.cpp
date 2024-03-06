#include <mitsuba/core/fwd.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/shape.h>
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

    - Note that it is not possible to assign a different material to each instance — the material
      assignment specified within the shape group is the one that matters.
    - Shape groups cannot be used to replicate shapes with attached emitters, sensors, or
      subsurface scattering models.

 */

template <typename Float, typename Spectrum>
class Instance final: public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_id, m_to_world, m_to_object, m_shape_type,
                   mark_dirty)
    MI_IMPORT_TYPES(BSDF)

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;
    using ShapeGroup_ = ShapeGroup<Float, Spectrum>;

    Instance(const Properties &props) : Base(props) {
        for (auto &kv : props.objects()) {
            Base *shape = dynamic_cast<Base *>(kv.second.get());
            if (shape && shape->is_shapegroup()) {
                if (m_shapegroup)
                    Throw("Only a single shapegroup can be specified per instance.");
                m_shapegroup = (ShapeGroup_*) shape;
            } else {
                Throw("Only a shapegroup can be specified in an instance.");
            }
        }

        if (!m_shapegroup)
            Throw("A reference to a 'shapegroup' must be specified!");

        m_shape_type = ShapeType::Instance;

        dr::make_opaque(m_to_world, m_to_object);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("to_world", *m_to_world.ptr(), +ParamFlags::NonDifferentiable);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (keys.empty() || string::contains(keys, "to_world")) {
            // Update the scalar value of the matrix
            m_to_world = m_to_world.value();
            m_to_object = m_to_world.value().inverse();
            mark_dirty();
        }
        Base::parameters_changed();
    }

    ScalarBoundingBox3f bbox() const override {
        const ScalarBoundingBox3f &bbox = m_shapegroup->bbox();

        // If the shape group is empty, return the invalid bbox
        if (!bbox.valid())
            return bbox;

        ScalarBoundingBox3f result;
        for (int i = 0; i < 8; ++i)
            result.expand(m_to_world.scalar() * bbox.corner(i));
        return result;
    }

    ScalarSize primitive_count() const override { return 1; }

    ScalarSize effective_primitive_count() const override {
        return m_shapegroup->primitive_count();
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    template <typename FloatP, typename Ray3fP>
    std::tuple<FloatP, Point<FloatP, 2>, dr::uint32_array_t<FloatP>,
               dr::uint32_array_t<FloatP>>
    ray_intersect_preliminary_impl(const Ray3fP &ray,
                                   ScalarIndex /*prim_index*/,
                                   dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);
        if constexpr (!dr::is_array_v<FloatP>) {
            return m_shapegroup->ray_intersect_preliminary_scalar(m_to_object.scalar().transform_affine(ray));
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
            return m_shapegroup->ray_test_scalar(m_to_object.scalar().transform_affine(ray));
        } else {
            Throw("Instance::ray_test_impl() should only be called with scalar types.");
        }
    }

    MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     uint32_t recursion_depth,
                                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        const Transform4f& to_world  = m_to_world.value();
        const Transform4f& to_object = m_to_object.value();

        constexpr bool IsDiff = dr::is_diff_v<Float>;
        bool grad_enabled = dr::grad_enabled(to_world);

        if constexpr (IsDiff) {
            if (grad_enabled && m_shapegroup->parameters_grad_enabled())
                Throw("Cannot differentiate instance parameters and shapegroup "
                      "internal parameters at the same time!");
        }

        // Nested instancing is not supported
        if (recursion_depth > 0)
            return dr::zeros<SurfaceInteraction3f>();

        bool detach_shape = has_flag(ray_flags, RayFlags::DetachShape);
        bool follow_shape = has_flag(ray_flags, RayFlags::FollowShape);

        /* If necessary, temporally suspend gradient tracking for all shape
           parameters to construct a surface interaction completely detach from
           the shape. */
        dr::suspend_grad<Float> scope(detach_shape, to_world, to_object);

        SurfaceInteraction3f si;
        {
            /* Temporally suspend gradient tracking when `to_world` need to be
               differentiated as the various terms of `si` will be recomputed
               to account for the motion of `si` already. */
            dr::suspend_grad<Float> scope2(grad_enabled);
            si = m_shapegroup->compute_surface_interaction(
                to_object.transform_affine(ray), pi, ray_flags,
                recursion_depth, active);
        }

        // Hit point `si.p` is only attached to the surface motion
        si.p = to_world.transform_affine(si.p);
        si.n = dr::normalize(dr::detach(to_world).transform_affine(si.n));
        if (likely(has_flag(ray_flags, RayFlags::ShadingFrame)))
            si.sh_frame.n = dr::normalize(dr::detach(to_world).transform_affine(si.sh_frame.n));

        if constexpr (IsDiff) {
            if (follow_shape && grad_enabled) {
                /* Recompute si.t in a differential manner as the distance
                   between the ray origin and the hit point following the moving
                   surface. */
                si.t = dr::sqrt(dr::squared_norm(si.p - ray.o) / dr::squared_norm(ray.d));
            } else if (!follow_shape && grad_enabled) {
                /* Differential recomputation of the intersection of the ray
                   with the moving plane tangent to the hit point. In this
                   scenario, it is important that `si.p` stays along the ray as
                   the surface moves. */
                si.t = (dr::dot(si.n, si.p) - dr::dot(si.n, ray.o)) / dr::dot(si.n, ray.d);
                si.p = ray(si.t);
                // TODO what can we do about the normals? Take into account curvature?
                // TODO si.uv should be attached but we don't know about the underlying parameterization
            }
        }

        if (likely(has_flag(ray_flags, RayFlags::ShadingFrame)))
            si.initialize_sh_frame();

        if (likely(has_flag(ray_flags, RayFlags::dPdUV))) {
            si.dp_du = to_world.transform_affine(si.dp_du);
            si.dp_dv = to_world.transform_affine(si.dp_dv);
        }

        if (has_flag(ray_flags, RayFlags::dNGdUV) || has_flag(ray_flags, RayFlags::dNSdUV)) {
            Normal3f n = has_flag(ray_flags, RayFlags::dNGdUV) ? si.n : si.sh_frame.n;

            // Determine the length of the transformed normal before it was re-normalized
            Normal3f tn = to_world.transform_affine(dr::normalize(to_object.transform_affine(n)));
            Float inv_len = dr::rcp(dr::norm(tn));
            tn *= inv_len;

            // Apply transform to dn_du and dn_dv
            si.dn_du = to_world.transform_affine(Normal3f(si.dn_du)) * inv_len;
            si.dn_dv = to_world.transform_affine(Normal3f(si.dn_dv)) * inv_len;

            si.dn_du -= tn * dr::dot(tn, si.dn_du);
            si.dn_dv -= tn * dr::dot(tn, si.dn_dv);
        }

        si.instance = this;

        return si;
    }

    //! @}
    // =============================================================

    std::string to_string() const override {
        std::ostringstream oss;
            oss << "Instance[" << std::endl
                << "  shapegroup = " << string::indent(m_shapegroup) << std::endl
                << "  to_world = " << string::indent(m_to_world, 13) << "," << std::endl
                << "]";
        return oss.str();
    }

#if defined(MI_ENABLE_EMBREE)
    RTCGeometry embree_geometry(RTCDevice device) override {
        DRJIT_MARK_USED(device);
        if constexpr (!dr::is_cuda_v<Float>) {
            RTCGeometry instance = m_shapegroup->embree_geometry(device);
            rtcSetGeometryTimeStepCount(instance, 1);
            dr::Matrix<ScalarFloat32, 4> matrix(dr::transpose(m_to_world.scalar().matrix));
            rtcSetGeometryTransform(instance, 0, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, matrix.data());
            rtcCommitGeometry(instance);
            return instance;
        } else {
            Throw("embree_geometry() should only be called in CPU mode.");
        }
    }
#endif

#if defined(MI_ENABLE_CUDA)
    virtual void optix_prepare_ias(const OptixDeviceContext& context,
                                   std::vector<OptixInstance>& instances,
                                   uint32_t instance_id,
                                   const ScalarTransform4f& transf) override {
        m_shapegroup->optix_prepare_ias(context, instances, instance_id,
                                        transf * m_to_world.scalar());
    }

    virtual void optix_fill_hitgroup_records(std::vector<HitGroupSbtRecord> &,
                                             const OptixProgramGroup *) override {
        /* no op */
    }

    virtual void optix_prepare_geometry() override {
        /* no op */
    }
#endif

    bool parameters_grad_enabled() const override {
        return dr::grad_enabled(m_to_world) || m_shapegroup->parameters_grad_enabled();
    }

    MI_DECLARE_CLASS()
private:
   ref<ShapeGroup_> m_shapegroup;
};

MI_IMPLEMENT_CLASS_VARIANT(Instance, Shape)
MI_EXPORT_PLUGIN(Instance, "Instanced geometry")
NAMESPACE_END(mitsuba)
