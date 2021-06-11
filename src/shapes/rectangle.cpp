#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

#if defined(MTS_ENABLE_CUDA)
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

        Vector3f dp_du = m_to_world * Vector3f(2.f, 0.f, 0.f);
        Vector3f dp_dv = m_to_world * Vector3f(0.f, 2.f, 0.f);
        Normal3f normal = ek::normalize(m_to_world * Normal3f(0.f, 0.f, 1.f));
        m_frame = Frame3f(dp_du, dp_dv, normal);
        m_inv_surface_area = ek::rcp(surface_area());

        ek::make_opaque(m_frame, m_inv_surface_area);
        ek::make_opaque(m_to_world, m_to_object);
    }

    ScalarBoundingBox3f bbox() const override {
        auto to_world = ek::get_slice<ScalarTransform4f>(m_to_world);
        ScalarBoundingBox3f bbox;
        bbox.expand(to_world.transform_affine(ScalarPoint3f(-1.f, -1.f, 0.f)));
        bbox.expand(to_world.transform_affine(ScalarPoint3f( 1.f, -1.f, 0.f)));
        bbox.expand(to_world.transform_affine(ScalarPoint3f( 1.f,  1.f, 0.f)));
        bbox.expand(to_world.transform_affine(ScalarPoint3f(-1.f,  1.f, 0.f)));
        return bbox;
    }

    Float surface_area() const override {
        return ek::norm(ek::cross(m_frame.s, m_frame.t));
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

    template <typename FloatP, typename Ray3fP>
    std::tuple<FloatP, Point<FloatP, 2>, ek::uint32_array_t<FloatP>,
               ek::uint32_array_t<FloatP>>
    ray_intersect_preliminary_impl(const Ray3fP &ray_,
                                   ek::mask_t<FloatP> active) const {
        Transform<Point<FloatP, 4>> to_object;
        if constexpr (!ek::is_jit_array_v<FloatP>)
            to_object = ek::get_slice<ScalarTransform4f>(m_to_object, 0, true);
        else
            to_object = m_to_object;

        Ray3fP ray = to_object.transform_affine(ray_);
        FloatP t   = -ray.o.z() / ray.d.z();
        Point<FloatP, 3> local = ray(t);

        // Is intersection within ray segment and rectangle?
        active = active && t >= ray.mint
                        && t <= ray.maxt
                        && ek::abs(local.x()) <= 1.f
                        && ek::abs(local.y()) <= 1.f;

        return { ek::select(active, t, ek::Infinity<FloatP>),
                 Point<FloatP, 2>(local.x(), local.y()), ((uint32_t) -1), 0 };
    }

    template <typename FloatP, typename Ray3fP>
    ek::mask_t<FloatP> ray_test_impl(const Ray3fP &ray_,
                                     ek::mask_t<FloatP> active) const {
        MTS_MASK_ARGUMENT(active);

        Transform<Point<FloatP, 4>> to_object;
        if constexpr (!ek::is_jit_array_v<FloatP>)
            to_object = ek::get_slice<ScalarTransform4f>(m_to_object, 0, true);
        else
            to_object = m_to_object;

        Ray3fP ray     = to_object.transform_affine(ray_);
        FloatP t       = -ray.o.z() / ray.d.z();
        Point<FloatP, 3> local = ray(t);

        // Is intersection within ray segment and rectangle?
        return active && t >= ray.mint
                      && t <= ray.maxt
                      && ek::abs(local.x()) <= 1.f
                      && ek::abs(local.y()) <= 1.f;
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

        si.n          = m_frame.n;
        si.sh_frame.n = m_frame.n;
        si.dp_du      = m_frame.s;
        si.dp_dv      = m_frame.t;
        si.uv         = Point2f(ek::fmadd(pi.prim_uv.x(), 0.5f, 0.5f),
                                ek::fmadd(pi.prim_uv.y(), 0.5f, 0.5f));

        si.dn_du = si.dn_dv = ek::zero<Vector3f>();
        si.shape    = this;
        si.instance = nullptr;

        return si;
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("to_world", m_to_world);
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
                m_optix_data_ptr = jit_malloc(AllocType::Device, sizeof(OptixRectangleData));

            OptixRectangleData data = { bbox(), ek::get_slice<ScalarTransform4f>(m_to_object) };

            jit_memcpy(JitBackend::CUDA, m_optix_data_ptr, &data,
                       sizeof(OptixRectangleData));
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
    Frame3f m_frame;
    Float m_inv_surface_area;
};

MTS_IMPLEMENT_CLASS_VARIANT(Rectangle, Shape)
MTS_EXPORT_PLUGIN(Rectangle, "Rectangle intersection primitive");
NAMESPACE_END(mitsuba)
