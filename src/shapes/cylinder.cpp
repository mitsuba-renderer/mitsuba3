#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

#include <drjit/packet.h>

#if defined(MI_ENABLE_CUDA)
    #include "optix/cylinder.cuh"
#endif

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-cylinder:

Cylinder (:monosp:`cylinder`)
----------------------------------------------------

.. pluginparameters::


 * - p0
   - |point|
   - Object-space starting point of the cylinder's centerline.
     (Default: (0, 0, 0))

 * - p1
   - |point|
   - Object-space endpoint of the cylinder's centerline (Default: (0, 0, 1))

 * - radius
   - |float|
   - Radius of the cylinder in object-space units (Default: 1)

 * - flip_normals
   - |bool|
   -  Is the cylinder inverted, i.e. should the normal vectors
      be flipped? (Default: |false|, i.e. the normals point outside)

 * - to_world
   - |transform|
   - Specifies an optional linear object-to-world transformation. Note that non-uniform scales are
     not permitted! (Default: none, i.e. object space = world space)
   - |exposed|

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/shape_cylinder_onesided.jpg
   :caption: Cylinder with the default one-sided shading
.. subfigure:: ../../resources/data/docs/images/render/shape_cylinder_twosided.jpg
   :caption: Cylinder with two-sided shading
.. subfigend::
   :label: fig-cylinder

This shape plugin describes a simple cylinder intersection primitive.
It should always be preferred over approximations modeled using
triangles. Note that the cylinder does not have endcaps -- also,
its normals point outward, which means that the inside will be treated
as fully absorbing by most material models. If this is not
desirable, consider using the :ref:`twosided <bsdf-twosided>` plugin.

A simple example for instantiating a cylinder, whose interior is visible:

.. tabs::
    .. code-tab:: xml
        :name: cylinder

        <shape type="cylinder">
            <float name="radius" value="0.3"/>
            <bsdf type="twosided">
                <bsdf type="diffuse"/>
            </bsdf>
        </shape>

    .. code-tab:: python

        'type': 'cylinder',
        'radius': 0.3,
        'material': {
            'type': 'diffuse'
        }
 */

template <typename Float, typename Spectrum>
class Cylinder final : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_to_world, m_to_object, m_is_instance, initialize,
                   mark_dirty, get_children_string, parameters_grad_enabled)
    MI_IMPORT_TYPES()

    using typename Base::ScalarIndex;
    using typename Base::ScalarSize;

    Cylinder(const Properties &props) : Base(props) {
        /// Are the sphere normals pointing inwards? default: no
        m_flip_normals = props.get<bool>("flip_normals", false);

        // Update the to_world transform if face points and radius are also provided
        ScalarFloat radius = props.get<ScalarFloat>("radius", 1.f);
        ScalarPoint3f p0 = props.get<ScalarPoint3f>("p0", ScalarPoint3f(0.f, 0.f, 0.f)),
                      p1 = props.get<ScalarPoint3f>("p1", ScalarPoint3f(0.f, 0.f, 1.f));

        ScalarVector3f d = p1 - p0;
        ScalarFloat length = dr::norm(d);

        m_to_world =
            m_to_world.scalar() * ScalarTransform4f::translate(p0) *
            ScalarTransform4f::to_frame(ScalarFrame3f(d / length)) *
            ScalarTransform4f::scale(ScalarVector3f(radius, radius, length));

        update();
        initialize();
    }

    void update() {
         // Extract center and radius from to_world matrix (25 iterations for numerical accuracy)
        auto [S, Q, T] = transform_decompose(m_to_world.scalar().matrix, 25);

        if (dr::abs(S[0][1]) > 1e-6f || dr::abs(S[0][2]) > 1e-6f || dr::abs(S[1][0]) > 1e-6f ||
            dr::abs(S[1][2]) > 1e-6f || dr::abs(S[2][0]) > 1e-6f || dr::abs(S[2][1]) > 1e-6f)
            Log(Warn, "'to_world' transform shouldn't contain any shearing!");

        if (!(dr::abs(S[0][0] - S[1][1]) < 1e-6f))
            Log(Warn, "'to_world' transform shouldn't contain non-uniform scaling along the X and Y axes!");

        m_radius = dr::norm(m_to_world.value().transform_affine(Vector3f(1.f, 0.f, 0.f)));
        m_length = dr::norm(m_to_world.value().transform_affine(Vector3f(0.f, 0.f, 1.f)));

        if (S[0][0] <= 0.f) {
            m_radius = dr::abs(m_radius.value());
            m_flip_normals = !m_flip_normals;
        }

        // Compute the to_object transformation with uniform scaling and no shear
        m_to_object = m_to_world.value().inverse();

        m_inv_surface_area = dr::rcp(surface_area());

        dr::make_opaque(m_radius, m_length, m_inv_surface_area);
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
        ScalarVector3f x1 = m_to_world.scalar() * ScalarVector3f(1.f, 0.f, 0.f),
                       x2 = m_to_world.scalar() * ScalarVector3f(0.f, 1.f, 0.f),
                       x  = dr::sqrt(dr::sqr(x1) + dr::sqr(x2));

        ScalarPoint3f p0 = m_to_world.scalar() * ScalarPoint3f(0.f, 0.f, 0.f),
                      p1 = m_to_world.scalar() * ScalarPoint3f(0.f, 0.f, 1.f);

        /* To bound the cylinder, it is sufficient to find the
           smallest box containing the two circles at the endpoints. */
        return ScalarBoundingBox3f(dr::minimum(p0 - x, p1 - x),
                                   dr::maximum(p0 + x, p1 + x));
    }

    ScalarBoundingBox3f bbox(ScalarIndex /*index*/, const ScalarBoundingBox3f &clip) const override {
        using FloatP8         = dr::Packet<ScalarFloat, 8>;
        using MaskP8          = dr::mask_t<FloatP8>;
        using Point3fP8       = Point<FloatP8, 3>;
        using Vector3fP8      = Vector<FloatP8, 3>;
        using BoundingBox3fP8 = BoundingBox<Point3fP8>;

        ScalarPoint3f cyl_p = m_to_world.scalar().transform_affine(ScalarPoint3f(0.f, 0.f, 0.f));
        ScalarVector3f cyl_d =
            m_to_world.scalar().transform_affine(ScalarVector3f(0.f, 0.f, 1.f));

        // Compute a base bounding box
        ScalarBoundingBox3f bbox(this->bbox());
        bbox.clip(clip);

        /* Now forget about the cylinder ends and intersect an infinite
           cylinder with each bounding box face, then compute a bounding
           box of the resulting ellipses. */
        Point3fP8  face_p = dr::zeros<Point3fP8>();
        Vector3fP8 face_n = dr::zeros<Vector3fP8>();

        for (size_t i = 0; i < 3; ++i) {
            face_p.entry(i,  i * 2 + 0) = bbox.min.entry(i);
            face_p.entry(i,  i * 2 + 1) = bbox.max.entry(i);
            face_n.entry(i,  i * 2 + 0) = -1.f;
            face_n.entry(i,  i * 2 + 1) = 1.f;
        }

        // Project the cylinder direction onto the plane
        FloatP8 dp   = dr::dot(cyl_d, face_n);
        MaskP8 valid = dr::neq(dp, 0.f);

        // Compute semimajor/minor axes of ellipse
        Vector3fP8 v1 = dr::fnmadd(face_n, dp, cyl_d);
        FloatP8 v1_n2 = dr::squared_norm(v1);
        v1 = dr::select(dr::neq(v1_n2, 0.f), v1 * dr::rsqrt(v1_n2),
                    coordinate_system(face_n).first);
        Vector3fP8 v2 = dr::cross(face_n, v1);

        // Compute length of axes
        v1 *= m_radius.scalar() / dr::abs(dp);
        v2 *= m_radius.scalar();

        // Compute center of ellipse
        FloatP8 t = dr::dot(face_n, face_p - cyl_p) / dp;
        Point3fP8 center = dr::fmadd(Vector3fP8(cyl_d), t, Vector3fP8(cyl_p));
        center[neq(face_n, 0.f)] = face_p;

        // Compute ellipse minima and maxima
        Vector3fP8 x = dr::sqrt(dr::sqr(v1) + dr::sqr(v2));
        BoundingBox3fP8 ellipse_bounds(center - x, center + x);
        MaskP8 ellipse_overlap = valid && bbox.overlaps(ellipse_bounds);
        ellipse_bounds.clip(bbox);

        return ScalarBoundingBox3f(
            min_inner(select(ellipse_overlap, ellipse_bounds.min,
                              Point3fP8(dr::Infinity<ScalarFloat>))),
            max_inner(select(ellipse_overlap, ellipse_bounds.max,
                              Point3fP8(-dr::Infinity<ScalarFloat>))));
    }

    Float surface_area() const override {
        return dr::TwoPi<ScalarFloat> * m_radius.value() * m_length.value();
    }

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        auto [sin_theta, cos_theta] = dr::sincos(dr::TwoPi<Float> * sample.y());

        Point3f p(cos_theta, sin_theta, sample.x());
        Normal3f n(cos_theta, sin_theta, 0.f);

        if (m_flip_normals)
            n *= -1;

        PositionSample3f ps = dr::zeros<PositionSample3f>();
        ps.p     = m_to_world.value().transform_affine(p);
        ps.n     = dr::normalize(n);
        ps.pdf   = m_inv_surface_area;
        ps.time  = time;
        ps.delta = false;
        ps.uv = Point2f(sample.y(), sample.x());

        return ps;
    }

    Float pdf_position(const PositionSample3f & /*ps*/, Mask active) const override {
        MI_MASK_ARGUMENT(active);
        return m_inv_surface_area;
    }

    SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                               uint32_t ray_flags,
                                               Mask active) const override {
        auto [sin_phi, cos_phi] = dr::sincos(dr::TwoPi<Float> * uv.x());
        Point3f local(cos_phi, sin_phi, uv.y());
        Point3f p = m_to_world.value().transform_affine(local);

        Ray3f ray(p + local, -local, 0, Wavelength(0));

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
        MI_MASK_ARGUMENT(active);

        using Value = std::conditional_t<dr::is_cuda_v<FloatP> || dr::is_diff_v<Float>,
                                         dr::float32_array_t<FloatP>,
                                         dr::float64_array_t<FloatP>>;
        using ScalarValue = dr::scalar_t<Value>;

        Ray3fP ray;
        Value radius(1.0); // Constant kept for readability
        Value length(1.0);
        if constexpr (!dr::is_jit_v<Value>)
            ray = m_to_object.scalar().transform_affine(ray_);
        else
            ray = m_to_object.value().transform_affine(ray_);

        Value maxt = Value(ray.maxt);

        Value ox = Value(ray.o.x()),
              oy = Value(ray.o.y()),
              oz = Value(ray.o.z()),
              dx = Value(ray.d.x()),
              dy = Value(ray.d.y()),
              dz = Value(ray.d.z());

        Value A = dr::sqr(dx) + dr::sqr(dy),
              B = ScalarValue(2.f) * (dx * ox + dy * oy),
              C = dr::sqr(ox) + dr::sqr(oy) - dr::sqr(radius);

        auto [solution_found, near_t, far_t] = math::solve_quadratic(A, B, C);

        // Cylinder doesn't intersect with the segment on the ray
        dr::mask_t<FloatP> out_bounds =
            !(near_t <= maxt && far_t >= Value(0.0)); // NaN-aware conditionals

        Value z_pos_near = oz + dz * near_t,
              z_pos_far  = oz + dz * far_t;

        // Cylinder fully contains the segment of the ray
        dr::mask_t<FloatP> in_bounds = near_t < Value(0.0) && far_t > maxt;

        active &= solution_found && !out_bounds && !in_bounds &&
                  ((z_pos_near >= Value(0.0) && z_pos_near <= length && near_t >= Value(0.0)) ||
                   (z_pos_far  >= Value(0.0) && z_pos_far <= length  && far_t <= maxt));

        FloatP t =
            dr::select(active,
                       dr::select(z_pos_near >= Value(0.0) && z_pos_near <= length &&
                                      near_t >= Value(0.0),
                                  FloatP(near_t), FloatP(far_t)),
                       dr::Infinity<FloatP>);

        return { t, dr::zeros<Point<FloatP, 2>>(), ((uint32_t) -1), 0 };
    }

    template <typename FloatP, typename Ray3fP>
    dr::mask_t<FloatP> ray_test_impl(const Ray3fP &ray_,
                                     dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);

        using Value = std::conditional_t<dr::is_cuda_v<FloatP> || dr::is_diff_v<Float>,
                                         dr::float32_array_t<FloatP>,
                                         dr::float64_array_t<FloatP>>;
        using ScalarValue = dr::scalar_t<Value>;

        Ray3fP ray;
        Value radius(1.0); // Constant kept for readability
        Value length(1.0);
        if constexpr (!dr::is_jit_v<Value>)
            ray = m_to_object.scalar().transform_affine(ray_);
        else
            ray = m_to_object.value().transform_affine(ray_);

        Value maxt = Value(ray.maxt);

        Value ox = Value(ray.o.x()),
              oy = Value(ray.o.y()),
              oz = Value(ray.o.z()),
              dx = Value(ray.d.x()),
              dy = Value(ray.d.y()),
              dz = Value(ray.d.z());

        Value A = dr::sqr(dx) + dr::sqr(dy),
              B = ScalarValue(2.f) * (dx * ox + dy * oy),
              C = dr::sqr(ox) + dr::sqr(oy) - dr::sqr(radius);

        auto [solution_found, near_t, far_t] = math::solve_quadratic(A, B, C);

        // Cylinder doesn't intersect with the segment on the ray
        dr::mask_t<FloatP> out_bounds = !(near_t <= maxt && far_t >= Value(0.0)); // NaN-aware conditionals

        Value z_pos_near = oz + dz * near_t,
              z_pos_far  = oz + dz * far_t;

        // Cylinder fully contains the segment of the ray
        dr::mask_t<FloatP> in_bounds = near_t < Value(0.0) && far_t > maxt;

        dr::mask_t<FloatP> valid_intersection =
            active && solution_found && !out_bounds && !in_bounds &&
            ((z_pos_near >= Value(0.0) && z_pos_near <= length && near_t >= Value(0.0)) ||
             (z_pos_far  >= Value(0.0) && z_pos_far  <= length && far_t  <= maxt));

        return valid_intersection;
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

        // Fields requirement dependencies
        bool need_dn_duv  = has_flag(ray_flags, RayFlags::dNSdUV) ||
                            has_flag(ray_flags, RayFlags::dNGdUV);
        bool detach_shape = has_flag(ray_flags, RayFlags::DetachShape);
        bool follow_shape = has_flag(ray_flags, RayFlags::FollowShape);

        const Float& radius = m_radius.value();
        const Transform4f& to_world = m_to_world.value();
        const Transform4f& to_object = m_to_object.value();

        /* If necessary, temporally suspend gradient tracking for all shape
        parameters to construct a surface interaction completely detach from
        the shape. */
        dr::suspend_grad<Float> scope(detach_shape, radius, to_world, to_object);

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

        si.t = pi.t;
        si.p = ray(si.t);
        Point3f local;
        if constexpr (IsDiff) {
            if (follow_shape) {
                /* FollowShape glues the interaction point with the shape.
                   Therefore, to also account for a possible differential motion
                   of the shape, we first compute a detached intersection point
                   in local space and transform it back in world space to get a
                   point rigidly attached to the shape's motion, including
                   translation, scaling and rotation. */
                local = to_object.transform_affine(si.p);
                Float r = dr::norm(dr::head<2>(local));
                local.x() /= r;
                local.y() /= r;
                local = dr::detach(local);
                si.p = to_world.transform_affine(local);
                si.t = dr::sqrt(dr::squared_norm(si.p - ray.o) / dr::squared_norm(ray.d));
            } else {
                /* To ensure that the differential interaction point stays along
                   the traced ray, we first recompute the intersection distance
                   in a differentiable way (w.r.t. to the cylindrical parameters) and
                   then compute the corresponding point along the ray. */
                si.t = dr::replace_grad(si.t, ray_intersect_preliminary(ray, active).t);
                si.p = ray(si.t);
                local = to_object.transform_affine(si.p);
            }
        } else {
            /* Mitigate roundoff error issues by a normal shift of the computed
               intersection point */
            local = to_object.transform_affine(si.p);
            si.p += si.n * (1.f - dr::norm(dr::head<2>(local)));
        }

        si.t = dr::select(active, si.t, dr::Infinity<Float>);

        // si.uv
        Float phi = dr::atan2(local.y(), local.x());
        dr::masked(phi, phi < 0.f) += dr::TwoPi<Float>;
        si.uv = Point2f(phi * dr::InvTwoPi<Float>, local.z());
        // si.dp_duv & si.n
        Vector3f dp_du = dr::TwoPi<Float> * Vector3f(-local.y(), local.x(), 0.f);
        Vector3f dp_dv = Vector3f(0.f, 0.f, 1.f);
        si.dp_du = to_world.transform_affine(dp_du);
        si.dp_dv = to_world.transform_affine(dp_dv);
        si.n = Normal3f(dr::normalize(dr::cross(si.dp_du, si.dp_dv)));

        if (m_flip_normals)
            si.n = -si.n;
        si.sh_frame.n = si.n;

        if (likely(need_dn_duv)) {
            si.dn_du = si.dp_du / (radius * (m_flip_normals ? -1.f : 1.f));
            si.dn_dv = Vector3f(0.f);
        }

        si.shape    = this;
        si.instance = nullptr;

        if (unlikely(has_flag(ray_flags, RayFlags::BoundaryTest))) {
            // Distance to cylinder edges
            Float dist_edge = dr::dot(si.sh_frame.n, -ray.d);

            // Distance to cap edges
            Float dist_caps = 0.5f - dr::abs(si.uv.y() - 0.5f);

            // Take the minimum of both distances to ensure 0.0 at silhouette.
            si.boundary_test = dr::minimum(dist_caps, dist_edge);
        }

        return si;
    }

#if defined(MI_ENABLE_CUDA)
    using Base::m_optix_data_ptr;

    void optix_prepare_geometry() override {
        if constexpr (dr::is_cuda_v<Float>) {
            if (!m_optix_data_ptr)
                m_optix_data_ptr = jit_malloc(AllocType::Device, sizeof(OptixCylinderData));

            OptixCylinderData data = { bbox(), m_to_object.scalar(),
                                       (float) 1.f,
                                       (float) 1.f };

            jit_memcpy(JitBackend::CUDA, m_optix_data_ptr, &data, sizeof(OptixCylinderData));
        }
    }
#endif

    bool parameters_grad_enabled() const override {
        return dr::grad_enabled(m_radius) || dr::grad_enabled(m_length);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Cylinder[" << std::endl
            << "  to_world = " << string::indent(m_to_world, 13) << "," << std::endl
            << "  radius = "  << m_radius << "," << std::endl
            << "  length = "  << m_length << "," << std::endl
            << "  surface_area = " << surface_area() << "," << std::endl
            << "  " << string::indent(get_children_string()) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    field<Float> m_radius, m_length;
    Float m_inv_surface_area;
    bool m_flip_normals;
};

MI_IMPLEMENT_CLASS_VARIANT(Cylinder, Shape)
MI_EXPORT_PLUGIN(Cylinder, "Cylinder intersection primitive");
NAMESPACE_END(mitsuba)
