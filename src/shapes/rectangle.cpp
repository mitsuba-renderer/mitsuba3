#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

#if defined(MTS_ENABLE_OPTIX)
    #include "optix/rectangle.cuh"
#endif

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-rectangle:

Rectangle (:monosp:`rectangle`)
-------------------------------------------------

.. pluginparameters::

 * - flip_normals
   - |bool|
   - Is the rectangle inverted, i.e. should the normal vectors be flipped? (Default: |false|)
 * - to_world
   - |transform|
   - Specifies a linear object-to-world transformation. (Default: none (i.e. object space = world space))

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/shape_rectangle.jpg
   :caption: Basic example
.. subfigure:: ../../resources/data/docs/images/render/shape_rectangle_parameterization.jpg
   :caption: A textured rectangle with the default parameterization
.. subfigend::
   :label: fig-rectangle

This shape plugin describes a simple rectangular shape primitive.
It is mainly provided as a convenience for those cases when creating and
loading an external mesh with two triangles is simply too tedious, e.g.
when an area light source or a simple ground plane are needed.
By default, the rectangle covers the XY-range :math:`[-1,1]\times[-1,1]`
and has a surface normal that points into the positive Z-direction.
To change the rectangle scale, rotation, or translation, use the
:monosp:`to_world` parameter.


The following XML snippet showcases a simple example of a textured rectangle:

.. code-block:: xml

    <shape type="rectangle">
        <bsdf type="diffuse">
            <texture name="reflectance" type="checkerboard">
                <transform name="to_uv">
                    <scale x="5" y="5" />
                </transform>
            </texture>
        </bsdf>
    </shape>
 */

template <typename Float, typename Spectrum>
class Rectangle final : public Shape<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(Shape, m_to_world, m_to_object, set_children,
                    get_children_string, parameters_grad_enabled)
    MTS_IMPORT_TYPES()

    using typename Base::ScalarSize;

    Rectangle(const Properties &props) : Base(props) {
        if (props.bool_("flip_normals", false))
            m_to_world = m_to_world * ScalarTransform4f::scale(ScalarVector3f(1.f, 1.f, -1.f));

        update();
        set_children();
    }

    void update() {
        m_to_object = m_to_world.inverse();

        ScalarVector3f dp_du = m_to_world * ScalarVector3f(2.f, 0.f, 0.f);
        ScalarVector3f dp_dv = m_to_world * ScalarVector3f(0.f, 2.f, 0.f);
        ScalarNormal3f normal = normalize(m_to_world * ScalarNormal3f(0.f, 0.f, 1.f));
        m_frame = ScalarFrame3f(dp_du, dp_dv, normal);

        m_inv_surface_area = rcp(surface_area());
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f bbox;
        bbox.expand(m_to_world.transform_affine(ScalarPoint3f(-1.f, -1.f, 0.f)));
        bbox.expand(m_to_world.transform_affine(ScalarPoint3f( 1.f, -1.f, 0.f)));
        bbox.expand(m_to_world.transform_affine(ScalarPoint3f( 1.f,  1.f, 0.f)));
        bbox.expand(m_to_world.transform_affine(ScalarPoint3f(-1.f,  1.f, 0.f)));
        return bbox;
    }

    ScalarFloat surface_area() const override {
        return norm(cross(m_frame.s, m_frame.t));
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        PositionSample3f ps;
        ps.p = m_to_world.transform_affine(
            Point3f(sample.x() * 2.f - 1.f, sample.y() * 2.f - 1.f, 0.f));
        ps.n    = m_frame.n;
        ps.pdf  = m_inv_surface_area;
        ps.uv   = sample;
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

        // Is intersection within ray segment and rectangle?
        active = active && t >= ray.mint
                        && t <= ray.maxt
                        && abs(local.x()) <= 1.f
                        && abs(local.y()) <= 1.f;

        PreliminaryIntersection3f pi = zero<PreliminaryIntersection3f>();
        pi.t = select(active, t, math::Infinity<Float>);
        pi.prim_uv = Point2f(local.x(), local.y());
        pi.shape = this;

        return pi;
    }

    Mask ray_test(const Ray3f &ray_, Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        Ray3f ray     = m_to_object.transform_affine(ray_);
        Float t       = -ray.o.z() * ray.d_rcp.z();
        Point3f local = ray(t);

        // Is intersection within ray segment and rectangle?
        return active && t >= ray.mint
                      && t <= ray.maxt
                      && abs(local.x()) <= 1.f
                      && abs(local.y()) <= 1.f;
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

        si.n          = m_frame.n;
        si.sh_frame.n = m_frame.n;
        si.dp_du      = m_frame.s;
        si.dp_dv      = m_frame.t;
        si.uv         = Point2f(fmadd(pi.prim_uv.x(), .5f, .5f),
                                fmadd(pi.prim_uv.y(), .5f, .5f));

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
                m_optix_data_ptr = cuda_malloc(sizeof(OptixRectangleData));

            OptixRectangleData data = { bbox(), m_to_object, m_frame.n,
                                        m_frame.s, m_frame.t };

            cuda_memcpy_to_device(m_optix_data_ptr, &data, sizeof(OptixRectangleData));
        }
    }
#endif

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Rectangle[" << std::endl
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
    ScalarFloat m_inv_surface_area;
};

MTS_IMPLEMENT_CLASS_VARIANT(Rectangle, Shape)
MTS_EXPORT_PLUGIN(Rectangle, "Rectangle intersection primitive");
NAMESPACE_END(mitsuba)
