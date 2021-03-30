#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

#if defined(MTS_ENABLE_CUDA)
    #include "optix/disk.cuh"
#endif

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-disk:

Disk (:monosp:`disk`)
-------------------------------------------------

.. pluginparameters::

 * - flip_normals
   - |bool|
   - Is the disk inverted, i.e. should the normal vectors be flipped? (Default: |false|)
 * - to_world
   - |transform|
   - Specifies a linear object-to-world transformation. Note that non-uniform scales are not
     permitted! (Default: none, i.e. object space = world space)

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/shape_disk.jpg
   :caption: Basic example
.. subfigure:: ../../resources/data/docs/images/render/shape_disk_parameterization.jpg
   :caption: A textured disk with the default parameterization
.. subfigend::
   :label: fig-disk

This shape plugin describes a simple disk intersection primitive. It is
usually preferable over discrete approximations made from triangles.

By default, the disk has unit radius and is located at the origin. Its
surface normal points into the positive Z-direction.
To change the disk scale, rotation, or translation, use the
:monosp:`to_world` parameter.

The following XML snippet instantiates an example of a textured disk shape:

.. code-block:: xml

    <shape type="disk">
        <bsdf type="diffuse">
            <texture name="reflectance" type="checkerboard">
                <transform name="to_uv">
                    <scale x="2" y="10" />
                </transform>
            </texture>
        </bsdf>
    </shape>
 */

template <typename Float, typename Spectrum>
class Disk final : public Shape<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Shape, m_to_world, m_to_object, set_children,
                    get_children_string, parameters_grad_enabled)
    MTS_IMPORT_TYPES()

    using typename Base::ScalarSize;

    Disk(const Properties &props) : Base(props) {
        if (props.bool_("flip_normals", false))
            m_to_world = m_to_world * ScalarTransform4f::scale(ScalarVector3f(1.f, 1.f, -1.f));

        update();
        set_children();
    }

    void update() {
        m_to_object = m_to_world.inverse();

        ScalarVector3f dp_du = m_to_world * ScalarVector3f(1.f, 0.f, 0.f);
        ScalarVector3f dp_dv = m_to_world * ScalarVector3f(0.f, 1.f, 0.f);

        m_du = ek::norm(dp_du);
        m_dv = ek::norm(dp_dv);

        ScalarNormal3f n = ek::normalize(m_to_world * ScalarNormal3f(0.f, 0.f, 1.f));
        m_frame = ScalarFrame3f(dp_du / m_du, dp_dv / m_dv, n);

        m_inv_surface_area = 1.f / surface_area();
   }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f bbox;
        bbox.expand(m_to_world.transform_affine(ScalarPoint3f(-1.f, -1.f, 0.f)));
        bbox.expand(m_to_world.transform_affine(ScalarPoint3f(-1.f,  1.f, 0.f)));
        bbox.expand(m_to_world.transform_affine(ScalarPoint3f( 1.f, -1.f, 0.f)));
        bbox.expand(m_to_world.transform_affine(ScalarPoint3f( 1.f,  1.f, 0.f)));
        return bbox;
    }

    ScalarFloat surface_area() const override {
        // First compute height of the ellipse
        ScalarFloat h = ek::sqrt(ek::sqr(m_dv) - ek::sqr(ek::dot(m_dv * m_frame.t, m_frame.s)));
        return ek::Pi<ScalarFloat> * m_du * h;
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        Point2f p = warp::square_to_uniform_disk_concentric(sample);

        PositionSample3f ps;
        ps.p    = m_to_world.transform_affine(Point3f(p.x(), p.y(), 0.f));
        ps.n    = m_frame.n;
        ps.pdf  = m_inv_surface_area;
        ps.time = time;
        ps.delta = false;

        return ps;
    }

    Float pdf_position(const PositionSample3f & /*ps*/, Mask active) const override {
        MTS_MASK_ARGUMENT(active);
        return m_inv_surface_area;
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    template <typename FloatP, typename Ray3fP>
    std::tuple<FloatP, Point<FloatP, 2>, ek::uint32_array_t<FloatP>,
               ek::uint32_array_t<FloatP>>
    ray_intersect_preliminary_impl(const Ray3fP &ray_,
                                   ek::mask_t<FloatP> active) const {
        Ray3fP ray = m_to_object.transform_affine(ray_);
        FloatP t   = -ray.o.z() / ray.d.z();
        Point<FloatP, 3> local = ray(t);

        // Is intersection within ray segment and disk?
        active = active && t >= ray.mint
                        && t <= ray.maxt
                        && local.x() * local.x() + local.y() * local.y() <= 1.f;

        return { ek::select(active, t, ek::Infinity<FloatP>),
                 Point<FloatP, 2>(local.x(), local.y()), ((uint32_t) -1), 0 };
    }

    template <typename FloatP, typename Ray3fP>
    ek::mask_t<FloatP> ray_test_impl(const Ray3fP &ray_,
                                     ek::mask_t<FloatP> active) const {
        MTS_MASK_ARGUMENT(active);

        Ray3fP ray = m_to_object.transform_affine(ray_);
        FloatP t   = -ray.o.z() / ray.d.z();
        Point<FloatP, 3> local = ray(t);

        // Is intersection within ray segment and rectangle?
        return active && t >= ray.mint
                      && t <= ray.maxt
                      && local.x() * local.x() + local.y() * local.y() <= 1.f;
    }

    MTS_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     PreliminaryIntersection3f pi,
                                                     uint32_t hit_flags,
                                                     Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        bool differentiable = ek::grad_enabled(ray) || parameters_grad_enabled();

        // Recompute ray intersection to get differentiable prim_uv and t
        if (differentiable && !has_flag(hit_flags, HitComputeFlags::NonDifferentiable))
            pi = ray_intersect_preliminary(ray, active);

        active &= pi.is_valid();

        // TODO handle sticky derivatives
        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();
        si.t = ek::select(active, pi.t, ek::Infinity<Float>);

        si.p = ray(pi.t);

        if (likely(has_flag(hit_flags, HitComputeFlags::UV) ||
                   has_flag(hit_flags, HitComputeFlags::dPdUV))) {
            Float r = ek::norm(Point2f(pi.prim_uv.x(), pi.prim_uv.y())),
                  inv_r = ek::rcp(r);

            Float v = ek::atan2(pi.prim_uv.y(), pi.prim_uv.x()) * ek::InvTwoPi<Float>;
            ek::masked(v, v < 0.f) += 1.f;
            si.uv = Point2f(r, v);

            if (likely(has_flag(hit_flags, HitComputeFlags::dPdUV))) {
                Float cos_phi = ek::select(ek::neq(r, 0.f), pi.prim_uv.x() * inv_r, 1.f),
                      sin_phi = ek::select(ek::neq(r, 0.f), pi.prim_uv.y() * inv_r, 0.f);

                si.dp_du = m_to_world * Vector3f( cos_phi, sin_phi, 0.f);
                si.dp_dv = m_to_world * Vector3f(-sin_phi, cos_phi, 0.f);
            }
        }

        si.n          = m_frame.n;
        si.sh_frame.n = m_frame.n;

        si.dn_du = si.dn_dv = ek::zero<Vector3f>();
        si.shape    = this;
        si.instance = nullptr;

        return si;
    }

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/) override {
        update();
        Base::parameters_changed();
#if defined(MTS_ENABLE_CUDA)
        optix_prepare_geometry();
#endif
    }

#if defined(MTS_ENABLE_CUDA)
    using Base::m_optix_data_ptr;

    void optix_prepare_geometry() override {
        if constexpr (ek::is_cuda_array_v<Float>) {
            if (!m_optix_data_ptr)
                m_optix_data_ptr = jit_malloc(AllocType::Device, sizeof(OptixDiskData));

            OptixDiskData data = { bbox(), m_to_object };

            jit_memcpy(JitBackend::CUDA, m_optix_data_ptr, &data,
                       sizeof(OptixDiskData));
        }
    }
#endif

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Disk[" << std::endl
            << "  to_world = " << string::indent(m_to_world, 13) << "," << std::endl
            << "  frame = " << string::indent(m_frame) << "," << std::endl
            << "  surface_area = " << surface_area() << "," << std::endl
            << "  " << string::indent(get_children_string()) << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    ScalarFrame3f m_frame;
    ScalarFloat m_du, m_dv;
    ScalarFloat m_inv_surface_area;
};

MTS_IMPLEMENT_CLASS_VARIANT(Disk, Shape)
MTS_EXPORT_PLUGIN(Disk, "Disk intersection primitive");
NAMESPACE_END(mitsuba)
