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

#if defined(MTS_ENABLE_OPTIX)
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

        m_du = norm(dp_du);
        m_dv = norm(dp_dv);

        ScalarNormal3f n = normalize(m_to_world * ScalarNormal3f(0.f, 0.f, 1.f));
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
        ScalarFloat h = sqrt(sqr(m_dv) - sqr(dot(m_dv * m_frame.t, m_frame.s)));
        return math::Pi<ScalarFloat> * m_du * h;
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

    PreliminaryIntersection3f ray_intersect_preliminary(const Ray3f &ray_,
                                                        Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        Ray3f ray     = m_to_object.transform_affine(ray_);
        Float t       = -ray.o.z() * ray.d_rcp.z();
        Point3f local = ray(t);

        // Is intersection within ray segment and disk?
        active = active && t >= ray.mint
                        && t <= ray.maxt
                        && local.x()*local.x() + local.y()*local.y() <= 1;

        PreliminaryIntersection3f pi = zero<PreliminaryIntersection3f>();
        pi.t = select(active, t, math::Infinity<Float>);
        pi.prim_uv = Point2f(local.x(), local.y());
        pi.shape = this;

        return pi;
    }

    Mask ray_test(const Ray3f &ray_, Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        Ray3f ray     = m_to_object.transform_affine(ray_);
        Float t      = -ray.o.z() * ray.d_rcp.z();
        Point3f local = ray(t);

        // Is intersection within ray segment and rectangle?
        return active && t >= ray.mint
                      && t <= ray.maxt
                      && local.x()*local.x() + local.y()*local.y() <= 1;
    }

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     PreliminaryIntersection3f pi,
                                                     HitComputeFlags flags,
                                                     Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        bool differentiable = false;
        if constexpr (is_diff_array_v<Float>)
            differentiable = requires_gradient(ray.o) ||
                             requires_gradient(ray.d) ||
                             parameters_grad_enabled();

        // Recompute ray intersection to get differentiable prim_uv and t
        if (differentiable && !has_flag(flags, HitComputeFlags::NonDifferentiable))
            pi = ray_intersect_preliminary(ray, active);

        active &= pi.is_valid();

        SurfaceInteraction3f si = zero<SurfaceInteraction3f>();
        si.t = select(active, pi.t, math::Infinity<Float>);

        si.p = ray(pi.t);

        if (likely(has_flag(flags, HitComputeFlags::UV))) {
            Float r = norm(Point2f(pi.prim_uv.x(), pi.prim_uv.y())),
                  inv_r = rcp(r);

            Float v = atan2(pi.prim_uv.y(), pi.prim_uv.x()) * math::InvTwoPi<Float>;
            masked(v, v < 0.f) += 1.f;
            si.uv = Point2f(r, v);

            if (likely(has_flag(flags, HitComputeFlags::dPdUV))) {
                Float cos_phi = select(neq(r, 0.f), pi.prim_uv.x() * inv_r, 1.f),
                      sin_phi = select(neq(r, 0.f), pi.prim_uv.y() * inv_r, 0.f);

                si.dp_du = m_to_world * Vector3f( cos_phi, sin_phi, 0.f);
                si.dp_dv = m_to_world * Vector3f(-sin_phi, cos_phi, 0.f);
            }
        }

        si.n          = m_frame.n;
        si.sh_frame.n = m_frame.n;

        si.dn_du = si.dn_dv = zero<Vector3f>();

        return si;
    }

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
    }

    void parameters_changed(const std::vector<std::string> &/*keys*/) override {
        update();
        Base::parameters_changed();
#if defined(MTS_ENABLE_OPTIX)
        optix_prepare_geometry();
#endif
    }

#if defined(MTS_ENABLE_OPTIX)
    using Base::m_optix_data_ptr;

    void optix_prepare_geometry() override {
        if constexpr (is_cuda_array_v<Float>) {
            if (!m_optix_data_ptr)
                m_optix_data_ptr = cuda_malloc(sizeof(OptixDiskData));

            OptixDiskData data = { bbox(), m_to_world, m_to_object };

            cuda_memcpy_to_device(m_optix_data_ptr, &data, sizeof(OptixDiskData));
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
