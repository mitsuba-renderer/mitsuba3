#include <mitsuba/core/fwd.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/shapegroup.h>

#if defined(MTS_ENABLE_EMBREE)
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

This plugin implements a geometry instance used to efficiently replicate geometry many times. For
details on how to create instances, refer to the :ref:`shape-shapegroup` plugin.

    .. image:: ../../resources/data/docs/images/render/shape_instance_fractal.jpg
        :width: 100%
        :align: center

    The Stanford bunny loaded a single time and instanciated 1365 times (equivalent to 100 million
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
    MTS_IMPORT_BASE(Shape, m_id, m_to_world, m_to_object)
    MTS_IMPORT_TYPES(BSDF)

    using typename Base::ScalarSize;
    using ShapeGroup_ = ShapeGroup<Float, Spectrum>;

    Instance(const Properties &props) {
        m_id = props.id();

        m_to_world = props.transform("to_world", ScalarTransform4f());
        m_to_object = m_to_world.inverse();

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
    }

    ScalarBoundingBox3f bbox() const override {
        const ScalarBoundingBox3f &bbox = m_shapegroup->bbox();

        // If the shape group is empty, return the invalid bbox
        if (!bbox.valid())
            return bbox;

        ScalarBoundingBox3f result;
        for (int i = 0; i < 8; ++i)
            result.expand(m_to_world * bbox.corner(i));
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
    std::tuple<FloatP, Point<FloatP, 2>, ek::uint32_array_t<FloatP>,
               ek::uint32_array_t<FloatP>>
    ray_intersect_preliminary_impl(const Ray3fP &ray,
                                   ek::mask_t<FloatP> active) const {
        MTS_MASK_ARGUMENT(active);
        if constexpr (!ek::is_array_v<FloatP>) {
            return m_shapegroup->ray_intersect_preliminary_scalar(m_to_object.transform_affine(ray));
        } else {
            Throw("Instance::ray_intersect_preliminary() should only be called with scalar types.");
        }
    }

    template <typename FloatP, typename Ray3fP>
    ek::mask_t<FloatP> ray_test_impl(const Ray3fP &ray, ek::mask_t<FloatP> active) const {
        MTS_MASK_ARGUMENT(active);

        if constexpr (!ek::is_array_v<FloatP>) {
            return m_shapegroup->ray_test_scalar(m_to_object.transform_affine(ray));
        } else {
            Throw("Instance::ray_test_impl() should only be called with scalar types.");
        }
    }

    MTS_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     PreliminaryIntersection3f pi,
                                                     uint32_t hit_flags,
                                                     Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        if constexpr (ek::is_jit_array_v<Float>) {
            if (jit_flag(JitFlag::VCallRecord))
                Throw("Instances are only supported in wavefront mode!");
        }

        SurfaceInteraction3f si = m_shapegroup->compute_surface_interaction(
            m_to_object.transform_affine(ray), pi, hit_flags, active);

        si.p = m_to_world.transform_affine(si.p);
        si.n = ek::normalize(m_to_world.transform_affine(si.n));

        if (likely(has_flag(hit_flags, HitComputeFlags::ShadingFrame))) {
            si.sh_frame.n = ek::normalize(m_to_world.transform_affine(si.sh_frame.n));
            si.initialize_sh_frame();
        }

        if (likely(has_flag(hit_flags, HitComputeFlags::dPdUV))) {
            si.dp_du = m_to_world.transform_affine(si.dp_du);
            si.dp_dv = m_to_world.transform_affine(si.dp_dv);
        }

        if (has_flag(hit_flags, HitComputeFlags::dNGdUV) || has_flag(hit_flags, HitComputeFlags::dNSdUV)) {
            Normal3f n = has_flag(hit_flags, HitComputeFlags::dNGdUV) ? si.n : si.sh_frame.n;

            // Determine the length of the transformed normal before it was re-normalized
            Normal3f tn = m_to_world.transform_affine(
                ek::normalize(m_to_object.transform_affine(n)));
            Float inv_len = ek::rcp(ek::norm(tn));
            tn *= inv_len;

            // Apply transform to dn_du and dn_dv
            si.dn_du = m_to_world.transform_affine(Normal3f(si.dn_du)) * inv_len;
            si.dn_dv = m_to_world.transform_affine(Normal3f(si.dn_dv)) * inv_len;

            si.dn_du -= tn * ek::dot(tn, si.dn_du);
            si.dn_dv -= tn * ek::dot(tn, si.dn_dv);
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

#if defined(MTS_ENABLE_EMBREE)
    RTCGeometry embree_geometry(RTCDevice device) override {
        ENOKI_MARK_USED(device);
        if constexpr (!ek::is_cuda_array_v<Float>) {
            RTCGeometry instance = m_shapegroup->embree_geometry(device);
            rtcSetGeometryTimeStepCount(instance, 1);
            ek::Matrix<ScalarFloat32, 4> matrix(m_to_world.matrix);
            rtcSetGeometryTransform(instance, 0, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, &matrix);
            rtcCommitGeometry(instance);
            return instance;
        } else {
            Throw("embree_geometry() should only be called in CPU mode.");
        }
    }
#endif

#if defined(MTS_ENABLE_CUDA)
    virtual void optix_prepare_ias(const OptixDeviceContext& context,
                                   std::vector<OptixInstance>& instances,
                                   uint32_t instance_id,
                                   const ScalarTransform4f& transf) override {
        m_shapegroup->optix_prepare_ias(context, instances, instance_id, transf * m_to_world);
    }

    virtual void optix_fill_hitgroup_records(std::vector<HitGroupSbtRecord> &,
                                             const OptixProgramGroup *) override {
        /* no op */
    }
#endif

    MTS_DECLARE_CLASS()
private:
   ref<ShapeGroup_> m_shapegroup;
};

MTS_IMPLEMENT_CLASS_VARIANT(Instance, Shape)
MTS_EXPORT_PLUGIN(Instance, "Instanced geometry")
NAMESPACE_END(mitsuba)
