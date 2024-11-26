#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

#if defined(MI_ENABLE_CUDA)
    #include "optix/sphere.cuh"
#endif

NAMESPACE_BEGIN(mitsuba)

/**!

.. _shape-sphere:

Sphere (:monosp:`sphere`)
-------------------------------------------------

.. pluginparameters::

 * - center
   - |point|
   - Center of the sphere (Default: (0, 0, 0))

 * - radius
   - |float|
   - Radius of the sphere (Default: 1)

 * - flip_normals
   - |bool|
   - Is the sphere inverted, i.e. should the normal vectors be flipped? (Default:|false|, i.e.
     the normals point outside)

 * - to_world
   - |transform|
   -  Specifies an optional linear object-to-world transformation.
      Note that non-uniform scales and shears are not permitted!
      (Default: none, i.e. object space = world space)
   - |exposed|, |differentiable|, |discontinuous|

 * - silhouette_sampling_weight
   - |float|
   - Weight associated with this shape when sampling silhoeuttes in the scene. (Default: 1)
   - |exposed|

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/shape_sphere_basic.jpg
   :caption: Basic example
.. subfigure:: ../../resources/data/docs/images/render/shape_sphere_parameterization.jpg
   :caption: A textured sphere with the default parameterization
.. subfigend::
   :label: fig-sphere

This shape plugin describes a simple sphere intersection primitive. It should
always be preferred over sphere approximations modeled using triangles.

A sphere can either be configured using a linear :monosp:`to_world` transformation or the :monosp:`center` and :monosp:`radius` parameters (or both).
The two declarations below are equivalent.

.. tabs::
    .. code-tab:: xml
        :name: sphere

        <shape type="sphere">
            <transform name="to_world">
                <translate x="1" y="0" z="0"/>
                <scale value="2"/>
            </transform>
            <bsdf type="diffuse"/>
        </shape>

        <shape type="sphere">
            <point name="center" x="1" y="0" z="0"/>
            <float name="radius" value="2"/>
            <bsdf type="diffuse"/>
        </shape>

    .. code-tab:: python

        'sphere_1': {
            'type': 'sphere',
            'to_world': mi.ScalarTransform4f().scale([2, 2, 2]).translate([1, 0, 0]),
            'bsdf': {
                'type': 'diffuse'
            }
        },

        'sphere_2': {
            'type': 'sphere',
            'center': [1, 0, 0],
            'radius': 2,
            'bsdf': {
                'type': 'diffuse'
            }
        }

When a :ref:`sphere <shape-sphere>` shape is turned into an :ref:`area <emitter-area>`
light source, Mitsuba 3 switches to an efficient
`sampling strategy <https://www.akalin.com/sampling-visible-sphere>`_ by Fred Akalin that
has particularly low variance.
This makes it a good default choice for lighting new scenes.

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/shape_sphere_light_mesh.jpg
   :caption: Spherical area light modeled using triangles
.. subfigure:: ../../resources/data/docs/images/render/shape_sphere_light_analytic.jpg
   :caption: Spherical area light modeled using the :ref:`sphere <shape-sphere>` plugin
.. subfigend::
   :label: fig-sphere-light
 */

template <typename Float, typename Spectrum>
class Sphere final : public Shape<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Shape, m_to_world, m_to_object, m_is_instance,
                   m_discontinuity_types, m_shape_type, initialize, mark_dirty,
                   get_children_string, parameters_grad_enabled)
    MI_IMPORT_TYPES()

    using typename Base::ScalarSize;
    using typename Base::ScalarIndex;

    Sphere(const Properties &props) : Base(props) {
        /// Are the sphere normals pointing inwards? default: no
        m_flip_normals = props.get<bool>("flip_normals", false);

        // Update the to_world transform if radius and center are also provided
        m_to_world =
            m_to_world.scalar() *
            ScalarTransform4f::translate(props.get<ScalarPoint3f>("center", 0.f)) *
            ScalarTransform4f::scale(props.get<ScalarFloat>("radius", 1.f));

        m_discontinuity_types = (uint32_t) DiscontinuityFlags::InteriorType;

        m_shape_type = ShapeType::Sphere;

        update();
        initialize();
    }

    void update() {
        // Extract center and radius from to_world matrix (25 iterations for numerical accuracy)
        auto [S, Q, T] = dr::transform_decompose(m_to_world.scalar().matrix, 25);

        if (dr::abs(S[0][1]) > 1e-6f || dr::abs(S[0][2]) > 1e-6f || dr::abs(S[1][0]) > 1e-6f ||
            dr::abs(S[1][2]) > 1e-6f || dr::abs(S[2][0]) > 1e-6f || dr::abs(S[2][1]) > 1e-6f)
            Log(Warn, "'to_world' transform shouldn't contain any shearing!");

        if (!(dr::abs(S[0][0] - S[1][1]) < 1e-6f && dr::abs(S[0][0] - S[2][2]) < 1e-6f))
            Log(Warn, "'to_world' transform shouldn't contain non-uniform scaling!");

        m_radius = dr::norm(m_to_world.value().transform_affine(Vector3f(1.f, 0.f, 0.f)));
        m_center = m_to_world.value().transform_affine(Point3f(0.f));

        if (S[0][0] <= 0.f) {
            m_radius = dr::abs(m_radius.value());
            m_flip_normals = !m_flip_normals;
        }

        // Compute the to_object transformation with uniform scaling and no shear
        m_to_object = m_to_world.value().inverse();

        m_inv_surface_area = dr::rcp(surface_area());

        dr::make_opaque(m_radius, m_center, m_inv_surface_area);
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

        Base::parameters_changed(keys);
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f bbox;
        bbox.min = m_center.scalar() - m_radius.scalar();
        bbox.max = m_center.scalar() + m_radius.scalar();
        return bbox;
    }

    Float surface_area() const override {
        return 4.f * dr::Pi<ScalarFloat> * dr::square(m_radius.value());
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        Point3f local = warp::square_to_uniform_sphere(sample);

        PositionSample3f ps = dr::zeros<PositionSample3f>();
        ps.p = dr::fmadd(local, m_radius.value(), m_center.value());
        ps.n = local;

        if (m_flip_normals)
            ps.n = -ps.n;

        ps.time = time;
        ps.delta = m_radius.value() == 0.f;
        ps.pdf = m_inv_surface_area;
        ps.uv = sample;

        return ps;
    }

    Float pdf_position(const PositionSample3f & /*ps*/, Mask active) const override {
        MI_MASK_ARGUMENT(active);
        return m_inv_surface_area;
    }

    DirectionSample3f sample_direction(const Interaction3f &it, const Point2f &sample,
                                       Mask active) const override {
        MI_MASK_ARGUMENT(active);
        DirectionSample3f result = dr::zeros<DirectionSample3f>();

        Vector3f dc_v = m_center.value() - it.p;
        Float dc_2 = dr::squared_norm(dc_v);

        Float radius_adj = m_radius.value() * (m_flip_normals ?
                                               (1.f + math::RayEpsilon<Float>) :
                                               (1.f - math::RayEpsilon<Float>));
        Mask outside_mask = active && dc_2 > dr::square(radius_adj);
        if (likely(dr::any_or<true>(outside_mask))) {
            Float inv_dc            = dr::rsqrt(dc_2),
                  sin_theta_max     = m_radius.value() * inv_dc,
                  sin_theta_max_2   = dr::square(sin_theta_max),
                  inv_sin_theta_max = dr::rcp(sin_theta_max),
                  cos_theta_max     = dr::safe_sqrt(1.f - sin_theta_max_2);

            /* Fall back to a Taylor series expansion for small angles, where
               the standard approach suffers from severe cancellation errors */
            Float sin_theta_2 = dr::select(sin_theta_max_2 > 0.00068523f, /* sin^2(1.5 deg) */
                                       1.f - dr::square(dr::fmadd(cos_theta_max - 1.f, sample.x(), 1.f)),
                                       sin_theta_max_2 * sample.x()),
                  cos_theta = dr::safe_sqrt(1.f - sin_theta_2);

            // Based on https://www.akalin.com/sampling-visible-sphere
            Float cos_alpha = sin_theta_2 * inv_sin_theta_max +
                              cos_theta * dr::safe_sqrt(dr::fnmadd(sin_theta_2, dr::square(inv_sin_theta_max), 1.f)),
                  sin_alpha = dr::safe_sqrt(dr::fnmadd(cos_alpha, cos_alpha, 1.f));

            auto [sin_phi, cos_phi] = dr::sincos(sample.y() * (2.f * dr::Pi<Float>));

            Vector3f d = Frame3f(dc_v * -inv_dc).to_world(Vector3f(
                cos_phi * sin_alpha,
                sin_phi * sin_alpha,
                cos_alpha));

            DirectionSample3f ds = dr::zeros<DirectionSample3f>();
            ds.p        = dr::fmadd(d, m_radius.value(), m_center.value());
            ds.n        = d;
            ds.d        = ds.p - it.p;

            Float dist2 = dr::squared_norm(ds.d);
            ds.dist     = dr::sqrt(dist2);
            ds.d        = ds.d / ds.dist;
            ds.pdf      = warp::square_to_uniform_cone_pdf(dr::zeros<Vector3f>(), cos_theta_max);
            dr::masked(ds.pdf, ds.dist == 0.f) = 0.f;

            dr::masked(result, outside_mask) = ds;
        }

        Mask inside_mask = dr::andnot(active, outside_mask);
        if (unlikely(dr::any_or<true>(inside_mask))) {
            Vector3f d = warp::square_to_uniform_sphere(sample);
            DirectionSample3f ds = dr::zeros<DirectionSample3f>();
            ds.p        = dr::fmadd(d, m_radius.value(), m_center.value());
            ds.n        = d;
            ds.d        = ds.p - it.p;

            Float dist2 = dr::squared_norm(ds.d);
            ds.dist     = dr::sqrt(dist2);
            ds.d        = ds.d / ds.dist;
            ds.pdf      = m_inv_surface_area * dist2 / dr::abs_dot(ds.d, ds.n);

            dr::masked(result, inside_mask) = ds;
        }

        result.time = it.time;
        result.delta = m_radius.value() == 0.f;

        if (m_flip_normals)
            result.n = -result.n;

        return result;
    }

    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        MI_MASK_ARGUMENT(active);

        // Sine of the angle of the cone containing the sphere as seen from 'it.p'.
        Float sin_alpha = m_radius.value() * dr::rcp(dr::norm(m_center.value() - it.p)),
              cos_alpha = dr::safe_sqrt(1.f - sin_alpha * sin_alpha);

        return dr::select(sin_alpha < dr::OneMinusEpsilon<Float>,
            // Reference point lies outside the sphere
            warp::square_to_uniform_cone_pdf(dr::zeros<Vector3f>(), cos_alpha),
            m_inv_surface_area * dr::square(ds.dist) / dr::abs_dot(ds.d, ds.n)
        );
    }

    SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                               uint32_t ray_flags,
                                               Mask active) const override {
        Point3f local = warp::square_to_uniform_sphere(uv);
        Point3f p = dr::fmadd(local, m_radius.value(), m_center.value());

        Ray3f ray(p + local, -local, 0, Wavelength(0));

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

        if (!has_flag(flags, DiscontinuityFlags::InteriorType))
            return dr::zeros<SilhouetteSample3f>();

        /// Sample a point on the shape surface
        SilhouetteSample3f ss(
            sample_position(0.f, dr::tail<2>(sample), active));

        /// Sample a tangential direction at the point
        ss.d = warp::interval_to_tangent_direction(ss.n, sample.x());

        /// Fill other fields
        ss.discontinuity_type = (uint32_t) DiscontinuityFlags::InteriorType;
        ss.flags = flags;
        ss.pdf *= dr::InvTwoPi<Float>;
        ss.shape = this;
        ss.foreshortening = dr::rcp(m_radius.value());

        return ss;
    }

    Point3f invert_silhouette_sample(const SilhouetteSample3f &ss,
                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        Point3f sample = dr::zeros<Point3f>(dr::width(ss));
        sample.x() = warp::tangent_direction_to_interval(ss.n, ss.d);
        sample.y() = ss.uv.x();
        sample.z() = ss.uv.y();

        return sample;
    }

    Point3f differential_motion(const SurfaceInteraction3f &si,
                         Mask active) const override {
        MI_MASK_ARGUMENT(active);

        if constexpr (!dr::is_diff_v<Float>) {
            return si.p;
        } else {
            Point3f local  = warp::square_to_uniform_sphere(dr::detach(si.uv));
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

        if (!has_flag(flags, DiscontinuityFlags::InteriorType))
            return dr::zeros<SilhouetteSample3f>();

        const Point3f& center = m_center.value();
        const Float& radius = m_radius.value();

        SilhouetteSample3f ss = dr::zeros<SilhouetteSample3f>();

        // O := center, V := viewpoint, Y := si.p, X := projected point
        Float dist_OV = dr::norm(viewpoint - center);
        Vector3f OVd = (viewpoint - center) / dist_OV,
                 OYd = dr::normalize(si.n - dr::dot(OVd, si.n) * OVd);

        Float sin_phi = radius * dr::rcp(dist_OV);

        Vector3f OXd  = dr::normalize(
            sin_phi * OVd +
            dr::safe_sqrt(-dr::fmsub(sin_phi, sin_phi, 1.f)) * OYd);

        ss.p = dr::fmadd(OXd, radius, center);
        ss.d = dr::normalize(ss.p - viewpoint);
        ss.n = dr::normalize(ss.p - center);
        ss.uv = warp::uniform_sphere_to_square(Vector3f(ss.n));

        Vector3f frame_t = dr::normalize(viewpoint - ss.p);
        ss.silhouette_d = dr::cross(ss.n, frame_t);

        ss.discontinuity_type = (uint32_t) DiscontinuityFlags::InteriorType;
        ss.flags = flags;
        ss.shape = this;

        return ss;
    }

    std::tuple<DynamicBuffer<UInt32>, DynamicBuffer<Float>>
    precompute_silhouette(const ScalarPoint3f & /*viewpoint*/) const override {
        DynamicBuffer<UInt32> indices((uint32_t)DiscontinuityFlags::InteriorType);
        DynamicBuffer<Float> weights(1.f);

        return {indices, weights};
    }

    SilhouetteSample3f
    sample_precomputed_silhouette(const Point3f &viewpoint, UInt32 /*sample1*/,
                                  Float sample2, Mask active) const override {
        MI_MASK_ARGUMENT(active);

        const Point3f &center = m_center.value();
        const Float &radius = m_radius.value();

        // O := center, V := viewpoint
        Float OV_dist = dr::norm(viewpoint - center);
        Vector3f OV_normalized = (viewpoint - center) / OV_dist;
        auto [dx, dy] = coordinate_system(OV_normalized);
        auto [sin_theta, cos_theta] = dr::sincos(sample2 * dr::TwoPi<Float>);

        // Call `primitive_silhouette_projection` which uses `si.n`
        // to compute the silhouette point as `ss.p`.
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.n = dr::normalize(OV_normalized + dx * cos_theta + dy * sin_theta);

        uint32_t flags = (uint32_t) DiscontinuityFlags::InteriorType;
        SilhouetteSample3f ss =
            primitive_silhouette_projection(viewpoint, si, flags, 0.f, active);

        Float radius_ring = radius / OV_dist * dr::norm(ss.p - viewpoint);
        ss.pdf = dr::rcp(dr::TwoPi<ScalarFloat> * radius_ring);

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
    ray_intersect_preliminary_impl(const Ray3fP &ray,
                                   ScalarIndex /*prim_index*/,
                                   dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);
        using Value = std::conditional_t<dr::is_cuda_v<FloatP> || dr::is_diff_v<Float>,
                                         dr::float32_array_t<FloatP>,
                                         dr::float64_array_t<FloatP>>;
        using Value3 = Vector<Value, 3>;

        using ScalarValue  = dr::scalar_t<Value>;
        using ScalarValue3 = Vector<ScalarValue, 3>;

        Value radius;
        Value3 center;
        if constexpr (!dr::is_jit_v<Value>) {
            radius = (ScalarValue)  m_radius.scalar();
            center = (ScalarValue3) m_center.scalar();
        } else {
            radius = (Value)  m_radius.value();
            center = (Value3) m_center.value();
        }

        Value maxt = Value(ray.maxt);

        // We define a plane which is perpendicular to the ray direction and
        // contains the sphere center and intersect it. We then solve the
        // ray-sphere intersection as if the ray origin was this new
        // intersection point. This additional step makes the whole intersection
        // routine numerically more robust.

        Value3 l = ray.o - center;
        Value3 d(ray.d);
        Value plane_t = dot(-l, d) / norm(d);

        // Ray is perpendicular to plane
        dr::mask_t<FloatP> no_hit =
            plane_t == Value(0) && dr::all(ray.o != center);

        Value3 plane_p = ray(FloatP(plane_t));

        // Intersection with plane outside of the sphere
        no_hit &= (norm(plane_p - center) > radius);

        Value3 o = plane_p - center;

        Value A = dr::squared_norm(d);
        Value B = dr::scalar_t<Value>(2.f) * dr::dot(o, d);
        Value C = dr::squared_norm(o) - dr::square(radius);

        auto [solution_found, near_t, far_t] = math::solve_quadratic(A, B, C);

        near_t += plane_t;
        far_t += plane_t;

        // Sphere doesn't intersect with the segment on the ray
        dr::mask_t<FloatP> out_bounds = !(near_t <= maxt && far_t >= Value(0.0)); // NaN-aware conditionals

        // Sphere fully contains the segment of the ray
        dr::mask_t<FloatP> in_bounds = near_t < Value(0.0) && far_t > maxt;

        active &= solution_found && !no_hit && !out_bounds && !in_bounds;

        FloatP t = dr::select(
            active, dr::select(near_t < Value(0.0), FloatP(far_t), FloatP(near_t)),
            dr::Infinity<FloatP>);

        return { t, dr::zeros<Point<FloatP, 2>>(), ((uint32_t) -1), 0 };
    }

    template <typename FloatP, typename Ray3fP>
    dr::mask_t<FloatP> ray_test_impl(const Ray3fP &ray,
                                     ScalarIndex /*prim_index*/,
                                     dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);

        using Value = std::conditional_t<dr::is_cuda_v<FloatP> || dr::is_diff_v<Float>,
                                         dr::float32_array_t<FloatP>,
                                         dr::float64_array_t<FloatP>>;
        using Value3 = Vector<Value, 3>;
        using ScalarValue  = dr::scalar_t<Value>;
        using ScalarValue3 = Vector<ScalarValue, 3>;

        Value radius;
        Value3 center;
        if constexpr (!dr::is_jit_v<Value>) {
            radius = (ScalarValue)  m_radius.scalar();
            center = (ScalarValue3) m_center.scalar();
        } else {
            radius = (Value)  m_radius.value();
            center = (Value3) m_center.value();
        }

        Value maxt = Value(ray.maxt);

        Value3 o = Value3(ray.o) - center;
        Value3 d(ray.d);

        Value A = dr::squared_norm(d);
        Value B = dr::scalar_t<Value>(2.f) * dr::dot(o, d);
        Value C = dr::squared_norm(o) - dr::square(radius);

        auto [solution_found, near_t, far_t] = math::solve_quadratic(A, B, C);

        // Sphere doesn't intersect with the segment on the ray
        dr::mask_t<FloatP> out_bounds = !(near_t <= maxt && far_t >= Value(0.0)); // NaN-aware conditionals

        // Sphere fully contains the segment of the ray
        dr::mask_t<FloatP> in_bounds  = near_t < Value(0.0) && far_t > maxt;

        return solution_found && !out_bounds && !in_bounds && active;
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
        bool need_dp_duv  = has_flag(ray_flags, RayFlags::dPdUV) || need_dn_duv;
        bool need_uv      = has_flag(ray_flags, RayFlags::UV) || need_dp_duv;
        bool detach_shape = has_flag(ray_flags, RayFlags::DetachShape);
        bool follow_shape = has_flag(ray_flags, RayFlags::FollowShape);

        const Point3f& center = m_center.value();
        const Float& radius = m_radius.value();
        const Transform4f& to_world = m_to_world.value();
        const Transform4f& to_object = m_to_object.value();

        /* If necessary, temporally suspend gradient tracking for all shape
           parameters to construct a surface interaction completely detach from
           the shape. */
        dr::suspend_grad<Float> scope(detach_shape, center, radius, to_world, to_object);

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

        Point3f local;
        if constexpr (IsDiff) {
            if (follow_shape) {
                /* FollowShape glues the interaction point with the shape.
                   Therefore, to also account for a possible differential motion
                   of the shape, we first compute a detached intersection point
                   in local space and transform it back in world space to get a
                   point rigidly attached to the shape's motion, including
                   translation, scaling and rotation. */
                local = to_object.transform_affine(ray(pi.t));
                /* With FollowShape the local position should always be static as
                   the intersection point follows any motion of the sphere. */
                local = dr::detach(local);
                si.p = to_world.transform_affine(local);
                si.sh_frame.n = (si.p - center) / radius;
                si.t = dr::sqrt(dr::squared_norm(si.p - ray.o) / dr::squared_norm(ray.d));
            } else {
                /* To ensure that the differential interaction point stays along
                   the traced ray, we first recompute the intersection distance
                   in a differentiable way (w.r.t. to the sphere parameters) and
                   then compute the corresponding point along the ray. */
                si.t = dr::replace_grad(pi.t, ray_intersect_preliminary(ray, 0, active).t);
                si.p = ray(si.t);
                si.sh_frame.n = dr::normalize(si.p - center);
                local = to_object.transform_affine(si.p);
            }
        } else {
            si.t = pi.t;
            si.sh_frame.n = dr::normalize(ray(si.t) - center);
            // Re-project onto the sphere to improve accuracy
            si.p = dr::fmadd(si.sh_frame.n, radius, center);
            local = to_object.transform_affine(si.p);
        }

        si.t = dr::select(active, si.t, dr::Infinity<Float>);

        if (likely(need_uv)) {
            Float rd_2  = dr::square(local.x()) + dr::square(local.y()),
                  theta = unit_angle_z(local),
                  phi   = dr::atan2(local.y(), local.x());

            dr::masked(phi, phi < 0.f) += 2.f * dr::Pi<Float>;

            si.uv = Point2f(phi * dr::InvTwoPi<Float>, theta * dr::InvPi<Float>);
            if (likely(need_dp_duv)) {
                si.dp_du = Vector3f(-local.y(), local.x(), 0.f);

                Float rd      = dr::sqrt(rd_2),
                      inv_rd  = dr::rcp(rd),
                      cos_phi = local.x() * inv_rd,
                      sin_phi = local.y() * inv_rd;

                si.dp_dv = Vector3f(local.z() * cos_phi,
                                    local.z() * sin_phi,
                                    -rd);

                Mask singularity_mask = active && (rd == 0.f);
                if (unlikely(dr::any_or<true>(singularity_mask)))
                    si.dp_dv[singularity_mask] = Vector3f(1.f, 0.f, 0.f);

                si.dp_du = to_world * si.dp_du * (2.f * dr::Pi<Float>);
                si.dp_dv = to_world * si.dp_dv * dr::Pi<Float>;
            }
        }

        if (m_flip_normals)
            si.sh_frame.n = -si.sh_frame.n;
        si.n = si.sh_frame.n;

        if (need_dn_duv) {
            Float inv_radius =
                (m_flip_normals ? -1.f : 1.f) * dr::rcp(radius);
            si.dn_du = si.dp_du * inv_radius;
            si.dn_dv = si.dp_dv * inv_radius;
        }

        si.shape    = this;
        si.instance = nullptr;

        return si;
    }

    bool parameters_grad_enabled() const override {
        return dr::grad_enabled(m_radius) || dr::grad_enabled(m_center) ||
               dr::grad_enabled(m_to_world.value());
    }

    //! @}
    // =============================================================

#if defined(MI_ENABLE_CUDA)
    using Base::m_optix_data_ptr;

    void optix_prepare_geometry() override {
        if constexpr (dr::is_cuda_v<Float>) {
            if (!m_optix_data_ptr)
                m_optix_data_ptr = jit_malloc(AllocType::Device, sizeof(OptixSphereData));

            OptixSphereData data = { bbox(),
                                     (Vector<float, 3>) m_center.scalar(),
                                     (float) m_radius.scalar() };

            jit_memcpy(JitBackend::CUDA, m_optix_data_ptr, &data,
                       sizeof(OptixSphereData));
        }
    }
#endif

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Sphere[" << std::endl
            << "  to_world = " << string::indent(m_to_world, 13) << "," << std::endl
            << "  center = "  << m_center << "," << std::endl
            << "  radius = "  << m_radius << "," << std::endl
            << "  surface_area = " << surface_area() << "," << std::endl
            << "  " << string::indent(get_children_string()) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    /// Center in world-space
    field<Point3f> m_center;
    /// Radius in world-space
    field<Float> m_radius;

    Float m_inv_surface_area;

    bool m_flip_normals;
};

MI_IMPLEMENT_CLASS_VARIANT(Sphere, Shape)
MI_EXPORT_PLUGIN(Sphere, "Sphere intersection primitive");
NAMESPACE_END(mitsuba)
