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
   - |exposed|, |differentiable|, |discontinuous|

 * - silhouette_sampling_weight
   - |float|
   - Weight associated with this shape when sampling silhoeuttes in the scene. (Default: 1)
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
    MI_IMPORT_BASE(Shape, m_to_world, m_to_object, m_is_instance,
                   m_discontinuity_types, m_shape_type, initialize, mark_dirty,
                   get_children_string, parameters_grad_enabled)
    MI_IMPORT_TYPES()

    using typename Base::ScalarIndex;
    using typename Base::ScalarSize;

    Disk(const Properties &props) : Base(props) {
        if (props.get<bool>("flip_normals", false))
            m_to_world =
                m_to_world.scalar() *
                ScalarTransform4f::scale(ScalarVector3f(1.f, 1.f, -1.f));

        m_discontinuity_types = (uint32_t) DiscontinuityFlags::PerimeterType;

        m_shape_type = ShapeType::Disk;

        update();
        initialize();
    }

    void update() {
        m_to_object = m_to_world.value().inverse();

        Vector3f dp_du = m_to_world.value() * Vector3f(1.f, 0.f, 0.f);
        Vector3f dp_dv = m_to_world.value() * Vector3f(0.f, 1.f, 0.f);

        m_du = dr::norm(dp_du);
        m_dv = dr::norm(dp_dv);

        Normal3f n = dr::normalize(m_to_world.value() * Normal3f(0.f, 0.f, 1.f));
        m_frame = Frame3f(dp_du / m_du, dp_dv / m_dv, n);
        m_inv_surface_area = dr::rcp(surface_area());

        dr::make_opaque(m_frame, m_inv_surface_area);
        mark_dirty();
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


    ScalarBoundingBox3f bbox() const override {
        ScalarTransform4f to_world = m_to_world.scalar();

        ScalarPoint3f c = to_world * ScalarPoint3f(0.f, 0.f, 0.f);
        ScalarVector3f u = to_world * ScalarVector3f(1.f, 0.f, 0.f);
        ScalarVector3f v = to_world * ScalarVector3f(0.f, 1.f, 0.f);
        ScalarVector3f e = dr::sqrt(dr::square(u) + dr::square(v));

        return ScalarBoundingBox3f(
            dr::minimum(c - e, c + e),
            dr::maximum(c - e, c + e)
        );
    }

    Float surface_area() const override {
        // First compute height of the ellipse
        Float h = dr::sqrt(dr::square(m_dv) - dr::square(dr::dot(m_dv * m_frame.t, m_frame.s)));
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


    SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                               uint32_t ray_flags,
                                               Mask active) const override {
        Point2f uniform_disk = warp::square_to_uniform_disk_concentric(uv);
        Point3f local = Point3f(uniform_disk.x(), uniform_disk.y(), 0.f);

        Point3f p = m_to_world.value().transform_affine(local);

        Ray3f ray(p + m_frame.n, -m_frame.n, 0, Wavelength(0));

        PreliminaryIntersection3f pi = ray_intersect_preliminary(ray, 0, active);
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
    //! @{ \name Silhouette sampling routines and other utilities
    // =============================================================

    SilhouetteSample3f sample_silhouette(const Point3f &sample,
                                         uint32_t flags,
                                         Mask active) const override {
        MI_MASK_ARGUMENT(active);

        if (!has_flag(flags, DiscontinuityFlags::PerimeterType))
            return dr::zeros<SilhouetteSample3f>();

        const Transform4f& to_world = m_to_world.value();
        SilhouetteSample3f ss = dr::zeros<SilhouetteSample3f>();

        /// Sample a point on the shape surface
        ss.uv = Point2f(1.f, sample.x());
        Float theta = sample.x() * dr::TwoPi<Float>;
        Float sin_theta, cos_theta;
        std::tie(sin_theta, cos_theta) = dr::sincos(theta);
        Point3f local_p = Point3f(cos_theta, sin_theta, 0.f);
        ss.p = to_world.transform_affine(Point3f(local_p.x(), local_p.y(), 0.f));

        /// Sample a tangential direction at the point
        ss.d = warp::square_to_uniform_sphere(Point2f(sample.y(), sample.z()));

        /// Fill other fields
        ss.discontinuity_type = (uint32_t) DiscontinuityFlags::PerimeterType;
        ss.flags = flags;
        ss.silhouette_d  = dr::normalize(to_world.transform_affine(
            Vector3f(local_p.y(), -local_p.x(), 0.f)));
        Normal3f frame_n = dr::normalize(dr::cross(ss.d, ss.silhouette_d));

        // Normal direction `ss.n` must point outwards
        Vector3f inward_dir = -local_p;
        inward_dir = to_world.transform_affine(inward_dir);
        dr::masked(frame_n, dr::dot(inward_dir, frame_n) > 0.f) *= -1.f;
        ss.n = frame_n;

        // Arc-length ratio
        ss.pdf = dr::InvTwoPi<Float> *
                 dr::rcp(dr::norm(to_world.transform_affine(
                     Vector3f(local_p.y(), -local_p.x(), 0.f))));
        ss.pdf *= warp::square_to_uniform_sphere_pdf(ss.d);
        ss.foreshortening = dr::norm(dr::cross(ss.d, ss.silhouette_d));
        ss.shape = this;

        return ss;
    }

    Point3f invert_silhouette_sample(const SilhouetteSample3f &ss,
                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        Point3f sample = dr::zeros<Point3f>(dr::width(ss));

        sample.x() = ss.uv.y();
        Point2f sample_yz = warp::uniform_sphere_to_square(ss.d);
        sample.y() = sample_yz.x();
        sample.z() = sample_yz.y();

        return sample;
    }

    Point3f differential_motion(const SurfaceInteraction3f &si,
                         Mask active) const override {
        MI_MASK_ARGUMENT(active);

        if constexpr (!dr::is_diff_v<Float>) {
            return si.p;
        } else {
            Point2f uv = dr::detach(si.uv);

            Float theta = uv.y() * dr::TwoPi<Float>;
            Float sin_theta, cos_theta;
            std::tie(sin_theta, cos_theta) = dr::sincos(theta);
            Point3f local  = uv.x() * Point3f(cos_theta, sin_theta, 0.f);
            Point3f p_diff = m_to_world.value().transform_affine(local);

            return dr::replace_grad(si.p, p_diff);
        }
    }

    SilhouetteSample3f primitive_silhouette_projection(const Point3f &viewpoint,
                                                       const SurfaceInteraction3f &si,
                                                       uint32_t flags,
                                                       Float /*sample*/,
                                                       Mask active) const override {
        MI_MASK_ARGUMENT(active);

        if (!has_flag(flags, DiscontinuityFlags::PerimeterType))
            return dr::zeros<SilhouetteSample3f>();

        const Transform4f &to_world = m_to_world.value();
        SilhouetteSample3f ss = dr::zeros<SilhouetteSample3f>();

        ss.uv = Point2f(1.f, si.uv.y());

        Float theta = ss.uv.y() * dr::TwoPi<Float>;
        Float sin_theta, cos_theta;
        std::tie(sin_theta, cos_theta) = dr::sincos(theta);

        Point3f local_p = Point3f(cos_theta, sin_theta, 0.f);

        ss.p = to_world.transform_affine(local_p);
        ss.d = dr::normalize(ss.p - viewpoint);

        ss.silhouette_d  = dr::normalize(to_world.transform_affine(
            Vector3f(local_p.y(), -local_p.x(), 0.f)));
        Normal3f frame_n = dr::normalize(dr::cross(ss.d, ss.silhouette_d));

        Vector3f inward_dir = -local_p;
        inward_dir = to_world.transform_affine(inward_dir);
        dr::masked(frame_n, dr::dot(inward_dir, frame_n) > 0.f) *= -1.f;
        ss.n = frame_n;

        ss.discontinuity_type = (uint32_t) DiscontinuityFlags::PerimeterType;
        ss.flags = flags;
        ss.shape = this;

        return ss;
    }

    std::tuple<DynamicBuffer<UInt32>, DynamicBuffer<Float>>
    precompute_silhouette(const ScalarPoint3f &/*viewpoint*/) const override {
        DynamicBuffer<UInt32> indices((uint32_t)DiscontinuityFlags::PerimeterType);
        DynamicBuffer<Float> weights(1.f);

        return {indices, weights};
    }

    SilhouetteSample3f
    sample_precomputed_silhouette(const Point3f &viewpoint,
                                  UInt32 /*sample1*/,
                                  Float sample,
                                  Mask active) const override {
        MI_MASK_ARGUMENT(active);

        // Call `primitive_silhouette_projection` which uses `si.uv` to compute
        // the silhouette point.

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.uv = Point2f(0.5f, sample);

        uint32_t flags = (uint32_t) DiscontinuityFlags::PerimeterType;
        SilhouetteSample3f ss = primitive_silhouette_projection(viewpoint, si, flags, 0.f, active);

        Point3f local_p = m_to_object.value().transform_affine(ss.p);
        // Arc-length ratio
        ss.pdf = dr::InvTwoPi<Float> *
                 dr::rcp(dr::norm(m_to_world.value().transform_affine(
                     Vector3f(local_p.y(), -local_p.x(), 0.f))));

        return ss;
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
                                   ScalarIndex /*prim_index*/,
                                   dr::mask_t<FloatP> active) const {
        Transform<Point<FloatP, 4>> to_object;
        if constexpr (!dr::is_jit_v<FloatP>)
            to_object = m_to_object.scalar();
        else
            to_object = m_to_object.value();

        Ray3fP ray = to_object.transform_affine(ray_);
        FloatP t = -ray.o.z() / ray.d.z();
        Point<FloatP, 3> local = ray(t);

        // Is intersection within ray segment and disk?
        active = active && t >= 0.f && t <= ray.maxt
                        && local.x() * local.x() + local.y() * local.y() <= 1.f;

        return { dr::select(active, t, dr::Infinity<FloatP>),
                 Point<FloatP, 2>(local.x(), local.y()), ((uint32_t) -1), 0 };
    }

    template <typename FloatP, typename Ray3fP>
    dr::mask_t<FloatP> ray_test_impl(const Ray3fP &ray_,
                                     ScalarIndex /*prim_index*/,
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
                   in a differentiable way (w.r.t. to the disk parameters) and
                   then compute the corresponding point along the ray. */
                PreliminaryIntersection3f pi_d = ray_intersect_preliminary(ray, 0, active);
                si.t = dr::replace_grad(pi.t, pi_d.t);
                si.p = ray(si.t);
                prim_uv = dr::replace_grad(pi.prim_uv, pi_d.prim_uv);
            }
        } else {
            si.t = pi.t;
            // Re-project onto the disk to improve accuracy
            Point3f p = ray(pi.t);
            Float dist = dr::dot(to_world.translation() - p, m_frame.n);
            si.p = p + dist * m_frame.n;
        }

        si.t = dr::select(active, si.t, dr::Infinity<Float>);

        if (likely(has_flag(ray_flags, RayFlags::UV) ||
                   has_flag(ray_flags, RayFlags::dPdUV))) {
            Float r = dr::norm(Point2f(prim_uv.x(), prim_uv.y())),
                  inv_r = dr::rcp(r);

            Float v = dr::atan2(prim_uv.y(), prim_uv.x()) * dr::InvTwoPi<Float>;
            dr::masked(v, v < 0.f) += 1.f;
            si.uv = Point2f(r, v);

            if (likely(has_flag(ray_flags, RayFlags::dPdUV))) {
                Float cos_phi = dr::select(r != 0.f, prim_uv.x() * inv_r, 1.f),
                      sin_phi = dr::select(r != 0.f, prim_uv.y() * inv_r, 0.f);

                si.dp_du = to_world * Vector3f( cos_phi, sin_phi, 0.f);
                si.dp_dv = to_world * Vector3f(-sin_phi, cos_phi, 0.f);
            }
        }

        si.n          = m_frame.n;
        si.sh_frame.n = m_frame.n;

        si.dn_du = si.dn_dv = dr::zeros<Vector3f>();
        si.shape    = this;
        si.instance = nullptr;

        return si;
    }

    bool parameters_grad_enabled() const override {
        return dr::grad_enabled(m_to_world.value());
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
