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
    MTS_IMPORT_BASE(Shape, m_to_world, m_to_object, initialize, mark_dirty,
                    get_children_string, parameters_grad_enabled)
    MTS_IMPORT_TYPES()

    using typename Base::ScalarSize;

    Rectangle(const Properties &props) : Base(props) {
        if (props.get<bool>("flip_normals", false))
            m_to_world =
                m_to_world.scalar() *
                ScalarTransform4f::scale(ScalarVector3f(1.f, 1.f, -1.f));

        update();
        initialize();
    }

    void update() {
        m_to_object = m_to_world.scalar().inverse();

        Vector3f dp_du = m_to_world.scalar() * Vector3f(2.f, 0.f, 0.f);
        Vector3f dp_dv = m_to_world.scalar() * Vector3f(0.f, 2.f, 0.f);
        Normal3f normal = dr::normalize(m_to_world.scalar() * Normal3f(0.f, 0.f, 1.f));
        m_frame = Frame3f(dp_du, dp_dv, normal);
        m_inv_surface_area = dr::rcp(surface_area());

        dr::make_opaque(m_frame, m_inv_surface_area);
        mark_dirty();
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f bbox;
        bbox.expand(m_to_world.scalar().transform_affine(ScalarPoint3f(-1.f, -1.f, 0.f)));
        bbox.expand(m_to_world.scalar().transform_affine(ScalarPoint3f( 1.f, -1.f, 0.f)));
        bbox.expand(m_to_world.scalar().transform_affine(ScalarPoint3f( 1.f,  1.f, 0.f)));
        bbox.expand(m_to_world.scalar().transform_affine(ScalarPoint3f(-1.f,  1.f, 0.f)));
        return bbox;
    }

    Float surface_area() const override {
        return dr::norm(dr::cross(m_frame.s, m_frame.t));
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        PositionSample3f ps;
        ps.p = m_to_world.value().transform_affine(
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

    SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                               uint32_t /*ray_flags*/,
                                               Mask /*active*/) const override {
        auto si = dr::zero<SurfaceInteraction3f>();
        si.t    = 0.f;  // Mark interaction as valid
        si.p    = m_to_world.value().transform_affine(
            Point3f(uv.x() * 2.f - 1.f, uv.y() * 2.f - 1.f, 0.f));
        si.n        = m_frame.n;
        si.shape    = this;
        si.uv       = uv;
        si.sh_frame = m_frame;
        si.dp_du    = m_frame.s;
        si.dp_dv    = m_frame.t;
        return si;
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    template <typename FloatP, typename Ray3fP>
    std::tuple<FloatP, Point<FloatP, 2>, dr::uint32_array_t<FloatP>,
               dr::uint32_array_t<FloatP>>
    ray_intersect_preliminary_impl(const Ray3fP &ray_,
                                   dr::mask_t<FloatP> active) const {
        Transform<Point<FloatP, 4>> to_object;
        if constexpr (!dr::is_jit_array_v<FloatP>)
            to_object = m_to_object.scalar();
        else
            to_object = m_to_object.value();

        Ray3fP ray = to_object.transform_affine(ray_);
        FloatP t   = -ray.o.z() / ray.d.z();
        Point<FloatP, 3> local = ray(t);

        // Is intersection within ray segment and rectangle?
        active = active && t >= 0.f
                        && t <= ray.maxt
                        && dr::abs(local.x()) <= 1.f
                        && dr::abs(local.y()) <= 1.f;

        return { dr::select(active, t, dr::Infinity<FloatP>),
                 Point<FloatP, 2>(local.x(), local.y()), ((uint32_t) -1), 0 };
    }

    template <typename FloatP, typename Ray3fP>
    dr::mask_t<FloatP> ray_test_impl(const Ray3fP &ray_,
                                     dr::mask_t<FloatP> active) const {
        MTS_MASK_ARGUMENT(active);

        Transform<Point<FloatP, 4>> to_object;
        if constexpr (!dr::is_jit_array_v<FloatP>)
            to_object = m_to_object.scalar();
        else
            to_object = m_to_object.value();

        Ray3fP ray     = to_object.transform_affine(ray_);
        FloatP t       = -ray.o.z() / ray.d.z();
        Point<FloatP, 3> local = ray(t);

        // Is intersection within ray segment and rectangle?
        return active && t >= 0.f
                      && t <= ray.maxt
                      && dr::abs(local.x()) <= 1.f
                      && dr::abs(local.y()) <= 1.f;
    }

    MTS_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     uint32_t /*recursion_depth*/,
                                                     Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        // Recompute ray intersection to get differentiable prim_uv and t
        Float t = pi.t;
        Point2f prim_uv = pi.prim_uv;
        if constexpr (dr::is_diff_array_v<Float>) {
            PreliminaryIntersection3f pi_d = ray_intersect_preliminary(ray, active);
            prim_uv = dr::replace_grad(prim_uv, pi_d.prim_uv);
            t = dr::replace_grad(t, pi_d.t);
        }

        // TODO handle RayFlags::FollowShape and RayFlags::DetachShape

        SurfaceInteraction3f si = dr::zero<SurfaceInteraction3f>();
        si.t = dr::select(active, t, dr::Infinity<Float>);

        // Re-project onto the rectangle to improve accuracy
        Point3f p = ray(t);
        Float dist = dr::dot(m_to_world.value().translation() - p, m_frame.n);
        si.p = p + dist * m_frame.n;

        si.n          = m_frame.n;
        si.sh_frame.n = m_frame.n;
        si.dp_du      = m_frame.s;
        si.dp_dv      = m_frame.t;
        si.uv         = Point2f(dr::fmadd(prim_uv.x(), 0.5f, 0.5f),
                                dr::fmadd(prim_uv.y(), 0.5f, 0.5f));

        si.dn_du = si.dn_dv = dr::zero<Vector3f>();
        si.shape    = this;
        si.instance = nullptr;

        if (unlikely(has_flag(ray_flags, RayFlags::BoundaryTest)))
            si.boundary_test = dr::hmin(0.5f - dr::abs(si.uv - 0.5f));

        return si;
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("to_world", *m_to_world.ptr());
        Base::traverse(callback);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (keys.empty() || string::contains(keys, "to_world")) {
            // Update the scalar value of the matrix
            m_to_world = m_to_world.value();
            update();
        }
        Base::parameters_changed();
    }

#if defined(MTS_ENABLE_CUDA)
    using Base::m_optix_data_ptr;

    void optix_prepare_geometry() override {
        if constexpr (dr::is_cuda_array_v<Float>) {
            if (!m_optix_data_ptr)
                m_optix_data_ptr = jit_malloc(AllocType::Device, sizeof(OptixRectangleData));

            OptixRectangleData data = { bbox(), m_to_object.scalar() };

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
