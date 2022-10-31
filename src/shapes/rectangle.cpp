#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

#if defined(MI_ENABLE_CUDA)
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
   - |exposed|

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

.. tabs::
    .. code-tab:: xml
        :name: rectangle

        <shape type="rectangle">
            <bsdf type="diffuse">
                <texture name="reflectance" type="checkerboard">
                    <transform name="to_uv">
                        <scale x="5" y="5" />
                    </transform>
                </texture>
            </bsdf>
        </shape>

    .. code-tab:: python

        'type': 'rectangle',
        'material': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'checkerboard',
                'to_uv': mi.ScalarTransform4f.scale([5, 5, 1])
            }
        }

 */

template <typename Float, typename Spectrum>
class Rectangle final : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_to_world, m_to_object, m_is_instance, initialize,
                   mark_dirty, get_children_string, parameters_grad_enabled)
    MI_IMPORT_TYPES()

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
        m_to_object = m_to_world.value().inverse();

        Vector3f dp_du = m_to_world.value() * Vector3f(2.f, 0.f, 0.f);
        Vector3f dp_dv = m_to_world.value() * Vector3f(0.f, 2.f, 0.f);
        Normal3f normal = dr::normalize(m_to_world.value() * Normal3f(0.f, 0.f, 1.f));
        m_frame = Frame3f(dp_du, dp_dv, normal);
        m_inv_surface_area = dr::rcp(surface_area());

        dr::make_opaque(m_frame, m_inv_surface_area);
        mark_dirty();
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f bbox;
        ScalarTransform4f to_world = m_to_world.scalar();

        bbox.expand(to_world.transform_affine(ScalarPoint3f(-1.f, -1.f, 0.f)));
        bbox.expand(to_world.transform_affine(ScalarPoint3f(-1.f,  1.f, 0.f)));
        bbox.expand(to_world.transform_affine(ScalarPoint3f( 1.f, -1.f, 0.f)));
        bbox.expand(to_world.transform_affine(ScalarPoint3f( 1.f,  1.f, 0.f)));

        return bbox;
    }

    void traverse(TraversalCallback *callback) override {
        Base::traverse(callback);
        callback->put_parameter("to_world", *m_to_world.ptr(), ParamFlags::Differentiable | ParamFlags::Discontinuous);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (keys.empty() || string::contains(keys, "to_world")) {
            // Ensure previous ray-tracing operation are fully evaluated before
            // modifying the scalar values of the fields in this class
            if constexpr (dr::is_jit_v<Float>)
                dr::sync_thread();
            // Update the scalar value of the matrix
            m_to_world = m_to_world.value();
            update();
        }
        Base::parameters_changed();
    }

    Float surface_area() const override {
        return dr::norm(dr::cross(m_frame.s, m_frame.t));
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        PositionSample3f ps = dr::zeros<PositionSample3f>();
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
        MI_MASK_ARGUMENT(active);
        return m_inv_surface_area;
    }

    SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                               uint32_t ray_flags,
                                               Mask active) const override {
        Point3f p = m_to_world.value().transform_affine(
            Point3f(uv.x() * 2.f - 1.f, uv.y() * 2.f - 1.f, 0.f));

        Ray3f ray(p + m_frame.n, -m_frame.n, 0, Wavelength(0));

        PreliminaryIntersection3f pi = ray_intersect_preliminary(ray, active);
        active &= pi.is_valid();

        if (dr::none_or<false>(active))
            return dr::zeros<SurfaceInteraction3f>();

        SurfaceInteraction3f si =
            compute_surface_interaction(ray, pi, ray_flags, 0, active);
        si.finalize_surface_interaction(pi, ray, ray_flags, active);

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
        if constexpr (!dr::is_jit_v<FloatP>)
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
        MI_MASK_ARGUMENT(active);

        Transform<Point<FloatP, 4>> to_object;
        if constexpr (!dr::is_jit_v<FloatP>)
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

    MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     uint32_t recursion_depth,
                                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);
        constexpr bool IsDiff = dr::is_diff_v<Float>;

        // Early exit when tracing isn't necessary
        if (!m_is_instance && recursion_depth > 0)
            return dr::zeros<SurfaceInteraction3f>();

        bool detach_shape = has_flag(ray_flags, RayFlags::DetachShape);
        bool follow_shape = has_flag(ray_flags, RayFlags::FollowShape);

        Transform4f to_world = m_to_world.value();
        Transform4f to_object = m_to_object.value();

        dr::suspend_grad<Float> scope(detach_shape, to_world, to_object, m_frame);

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        Point2f prim_uv = pi.prim_uv;

        if constexpr (IsDiff) {
            if (follow_shape) {
                /* FollowShape glues the interaction point with the shape.
                   Therefore, to also account for a possible differential motion
                   of the shape, we first compute a detached intersection point
                   in local space and transform it back in world space to get a
                   point rigidly attached to the shape's motion, including
                   translation, scaling and rotation. */
                Point3f local = to_object.transform_affine(ray(pi.t));
                /* With FollowShape the local position should always be static as
                   the intersection point follows any motion of the sphere. */
                local = dr::detach(local);
                si.p = to_world.transform_affine(local);
                si.t = dr::sqrt(dr::squared_norm(si.p - ray.o) / dr::squared_norm(ray.d));
                prim_uv = dr::head<2>(local);
            } else {
                /* To ensure that the differential interaction point stays along
                   the traced ray, we first recompute the intersection distance
                   in a differentiable way (w.r.t. to the rectangle parameters)
                   and then compute the corresponding point along the ray. */
                PreliminaryIntersection3f pi_d = ray_intersect_preliminary(ray, active);
                si.t = dr::replace_grad(pi.t, pi_d.t);
                si.p = ray(si.t);
                prim_uv = dr::replace_grad(pi.prim_uv, pi_d.prim_uv);
            }
        } else {
            si.t = pi.t;
            // Re-project onto the rectangle to improve accuracy
            Point3f p = ray(pi.t);
            Float dist = dr::dot(to_world.translation() - p, m_frame.n);
            si.p = p + dist * m_frame.n;
        }

        si.t = dr::select(active, si.t, dr::Infinity<Float>);

        si.n          = m_frame.n;
        si.sh_frame.n = m_frame.n;
        si.dp_du      = m_frame.s;
        si.dp_dv      = m_frame.t;
        si.uv         = Point2f(dr::fmadd(prim_uv.x(), 0.5f, 0.5f),
                                dr::fmadd(prim_uv.y(), 0.5f, 0.5f));

        si.dn_du = si.dn_dv = dr::zeros<Vector3f>();
        si.shape    = this;
        si.instance = nullptr;

        if (unlikely(has_flag(ray_flags, RayFlags::BoundaryTest)))
            si.boundary_test = dr::min(0.5f - dr::abs(si.uv - 0.5f));

        return si;
    }

#if defined(MI_ENABLE_CUDA)
    using Base::m_optix_data_ptr;

    void optix_prepare_geometry() override {
        if constexpr (dr::is_cuda_v<Float>) {
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

    MI_DECLARE_CLASS()
private:
    Frame3f m_frame;
    Float m_inv_surface_area;
};

MI_IMPLEMENT_CLASS_VARIANT(Rectangle, Shape)
MI_EXPORT_PLUGIN(Rectangle, "Rectangle intersection primitive");
NAMESPACE_END(mitsuba)
