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

#if defined(MI_ENABLE_CUDA)
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
   - |exposed|

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

.. tabs::
    .. code-tab:: xml
        :name: disk

        <shape type="disk">
            <bsdf type="diffuse">
                <texture name="reflectance" type="checkerboard">
                    <transform name="to_uv">
                        <scale x="2" y="10" />
                    </transform>
                </texture>
            </bsdf>
        </shape>

    .. code-tab:: python

        'type': 'disk',
        'material': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'checkerboard',
                'to_uv': mi.ScalarTransform4f([2, 10, 0])
            }
        }
 */

template <typename Float, typename Spectrum>
class Disk final : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_to_world, m_to_object, m_is_instance, initialize,
                   mark_dirty, get_children_string, parameters_grad_enabled)
    MI_IMPORT_TYPES()

    using typename Base::ScalarSize;

    Disk(const Properties &props) : Base(props) {
        if (props.get<bool>("flip_normals", false))
            m_to_world =
                m_to_world.scalar() *
                ScalarTransform4f::scale(ScalarVector3f(1.f, 1.f, -1.f));

        update();
        initialize();
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("to_world", *m_to_world.ptr(), +ParamFlags::NonDifferentiable);
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

    void update() {
        m_to_object = m_to_world.scalar().inverse();

        Vector3f dp_du = m_to_world.scalar() * Vector3f(1.f, 0.f, 0.f);
        Vector3f dp_dv = m_to_world.scalar() * Vector3f(0.f, 1.f, 0.f);

        m_du = dr::norm(dp_du);
        m_dv = dr::norm(dp_dv);

        Normal3f n = dr::normalize(m_to_world.scalar() * Normal3f(0.f, 0.f, 1.f));
        m_frame = Frame3f(dp_du / m_du, dp_dv / m_dv, n);
        m_inv_surface_area = dr::rcp(surface_area());

        dr::make_opaque(m_frame, m_inv_surface_area);
        mark_dirty();
   }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f bbox;
        bbox.expand(m_to_world.scalar().transform_affine(ScalarPoint3f(-1.f, -1.f, 0.f)));
        bbox.expand(m_to_world.scalar().transform_affine(ScalarPoint3f(-1.f,  1.f, 0.f)));
        bbox.expand(m_to_world.scalar().transform_affine(ScalarPoint3f( 1.f, -1.f, 0.f)));
        bbox.expand(m_to_world.scalar().transform_affine(ScalarPoint3f( 1.f,  1.f, 0.f)));
        return bbox;
    }

    Float surface_area() const override {
        // First compute height of the ellipse
        Float h = dr::sqrt(dr::sqr(m_dv) - dr::sqr(dr::dot(m_dv * m_frame.t, m_frame.s)));
        return dr::Pi<ScalarFloat> * m_du * h;
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        Point2f p = warp::square_to_uniform_disk_concentric(sample);

        PositionSample3f ps = dr::zeros<PositionSample3f>();
        ps.p    = m_to_world.value().transform_affine(Point3f(p.x(), p.y(), 0.f));
        ps.n    = m_frame.n;
        ps.pdf  = m_inv_surface_area;
        ps.time = time;
        ps.delta = false;

        Float r = dr::norm(p);
        Float v = dr::atan2(p.y(), p.x()) * dr::InvTwoPi<Float>;
        dr::masked(v, v < 0.f) += 1.f;
        ps.uv = Point2f(r, v);

        return ps;
    }

    Float pdf_position(const PositionSample3f & /*ps*/, Mask active) const override {
        MI_MASK_ARGUMENT(active);
        return m_inv_surface_area;
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

        // Is intersection within ray segment and disk?
        active = active && t >= 0.f && t <= ray.maxt
                        && local.x() * local.x() + local.y() * local.y() <= 1.f;

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

        Ray3fP ray = to_object.transform_affine(ray_);
        FloatP t   = -ray.o.z() / ray.d.z();
        Point<FloatP, 3> local = ray(t);

        // Is intersection within ray segment and rectangle?
        return active && t >= 0.f && t <= ray.maxt
                      && local.x() * local.x() + local.y() * local.y() <= 1.f;
    }

    MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     uint32_t recursion_depth,
                                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        // Early exit when tracing isn't necessary
        if (!m_is_instance && recursion_depth > 0)
            return dr::zeros<SurfaceInteraction3f>();

        // Recompute ray intersection to get differentiable prim_uv and t
        Float t = pi.t;
        Point2f prim_uv = pi.prim_uv;
        if constexpr (dr::is_diff_v<Float>) {
            PreliminaryIntersection3f pi_d = ray_intersect_preliminary(ray, active);
            prim_uv = dr::replace_grad(prim_uv, pi_d.prim_uv);
            t = dr::replace_grad(t, pi_d.t);
        }

        // TODO handle RayFlags::FollowShape and RayFlags::DetachShape

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.t = dr::select(active, t, dr::Infinity<Float>);

        // Re-project onto the disk to improve accuracy
        Point3f p = ray(t);
        Float dist = dr::dot(m_to_world.value().translation() - p, m_frame.n);
        si.p = p + dist * m_frame.n;

        if (likely(has_flag(ray_flags, RayFlags::UV) ||
                   has_flag(ray_flags, RayFlags::dPdUV) ||
                   has_flag(ray_flags, RayFlags::BoundaryTest))) {
            Float r = dr::norm(Point2f(prim_uv.x(), prim_uv.y())),
                  inv_r = dr::rcp(r);

            Float v = dr::atan2(prim_uv.y(), prim_uv.x()) * dr::InvTwoPi<Float>;
            dr::masked(v, v < 0.f) += 1.f;
            si.uv = Point2f(r, v);

            if (likely(has_flag(ray_flags, RayFlags::dPdUV))) {
                Float cos_phi = dr::select(dr::neq(r, 0.f), prim_uv.x() * inv_r, 1.f),
                      sin_phi = dr::select(dr::neq(r, 0.f), prim_uv.y() * inv_r, 0.f);

                si.dp_du = m_to_world.value() * Vector3f( cos_phi, sin_phi, 0.f);
                si.dp_dv = m_to_world.value() * Vector3f(-sin_phi, cos_phi, 0.f);
            }
        }

        si.n          = m_frame.n;
        si.sh_frame.n = m_frame.n;

        si.dn_du = si.dn_dv = dr::zeros<Vector3f>();
        si.shape    = this;
        si.instance = nullptr;

        if (unlikely(has_flag(ray_flags, RayFlags::BoundaryTest)))
            si.boundary_test = dr::abs(1.f - si.uv.x());

        return si;
    }

#if defined(MI_ENABLE_CUDA)
    using Base::m_optix_data_ptr;

    void optix_prepare_geometry() override {
        if constexpr (dr::is_cuda_v<Float>) {
            if (!m_optix_data_ptr)
                m_optix_data_ptr = jit_malloc(AllocType::Device, sizeof(OptixDiskData));

            OptixDiskData data = { bbox(), m_to_object.scalar() };

            jit_memcpy(JitBackend::CUDA, m_optix_data_ptr, &data, sizeof(OptixDiskData));
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

    MI_DECLARE_CLASS()
private:
    Frame3f m_frame;
    Float m_du, m_dv;
    Float m_inv_surface_area;
};

MI_IMPLEMENT_CLASS_VARIANT(Disk, Shape)
MI_EXPORT_PLUGIN(Disk, "Disk intersection primitive");
NAMESPACE_END(mitsuba)
