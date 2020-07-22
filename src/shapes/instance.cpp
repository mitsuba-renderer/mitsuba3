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
    #include <enoki/transform.h>
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
    using ShapeGroup = ShapeGroup<Float, Spectrum>;

    Instance(const Properties &props) {
        m_id = props.id();

        m_to_world = props.transform("to_world", ScalarTransform4f());
        m_to_object = m_to_world.inverse();

        for (auto &kv : props.objects()) {
            Base *shape = dynamic_cast<Base *>(kv.second.get());
            if (shape && shape->is_shapegroup()) {
                if (m_shapegroup)
                    Throw("Only a single shapegroup can be specified per instance.");
                m_shapegroup = (ShapeGroup*)shape;
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

    PreliminaryIntersection3f ray_intersect_preliminary(const Ray3f &ray,
                                                        Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        PreliminaryIntersection3f pi = m_shapegroup->ray_intersect_preliminary(
            m_to_object.transform_affine(ray), active);

        pi.instance = this;

        return pi;
    }

    Mask ray_test(const Ray3f &ray, Mask active) const override {
        MTS_MASK_ARGUMENT(active);
        return m_shapegroup->ray_test(m_to_object.transform_affine(ray), active);
    }

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     PreliminaryIntersection3f pi,
                                                     HitComputeFlags flags,
                                                     Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        SurfaceInteraction3f si = m_shapegroup->compute_surface_interaction(
            m_to_object.transform_affine(ray), pi, flags, active);

        si.p = m_to_world.transform_affine(si.p);
        si.n = normalize(m_to_world.transform_affine(si.n));

        if (likely(has_flag(flags, HitComputeFlags::ShadingFrame))) {
            si.sh_frame.n = normalize(m_to_world.transform_affine(si.sh_frame.n));
            si.initialize_sh_frame();
        }

        if (likely(has_flag(flags, HitComputeFlags::dPdUV))) {
            si.dp_du = m_to_world.transform_affine(si.dp_du);
            si.dp_dv = m_to_world.transform_affine(si.dp_dv);
        }

        if (has_flag(flags, HitComputeFlags::dNGdUV) || has_flag(flags, HitComputeFlags::dNSdUV)) {
            Normal3f n = has_flag(flags, HitComputeFlags::dNGdUV) ? si.n : si.sh_frame.n;

            // Determine the length of the transformed normal before it was re-normalized
            Normal3f tn = m_to_world.transform_affine(
                normalize(m_to_object.transform_affine(n)));
            Float inv_len = rcp(norm(tn));
            tn *= inv_len;

            // Apply transform to dn_du and dn_dv
            si.dn_du = m_to_world.transform_affine(Normal3f(si.dn_du)) * inv_len;
            si.dn_dv = m_to_world.transform_affine(Normal3f(si.dn_dv)) * inv_len;

            si.dn_du -= tn * dot(tn, si.dn_du);
            si.dn_dv -= tn * dot(tn, si.dn_dv);
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
        if constexpr (!is_cuda_array_v<Float>) {
            RTCGeometry instance = m_shapegroup->embree_geometry(device);
            rtcSetGeometryTimeStepCount(instance, 1);
            rtcSetGeometryTransform(instance, 0, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, &m_to_world.matrix);
            rtcCommitGeometry(instance);
            return instance;
        } else {
            Throw("embree_geometry() should only be called in CPU mode.");
        }
    }
#endif

#if defined(MTS_ENABLE_OPTIX)
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
   ref<ShapeGroup> m_shapegroup;
};

MTS_IMPLEMENT_CLASS_VARIANT(Instance, Shape)
MTS_EXPORT_PLUGIN(Instance, "Instanced geometry")
NAMESPACE_END(mitsuba)
