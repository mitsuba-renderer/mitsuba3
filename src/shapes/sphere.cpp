#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/scene_ir.h>

#if defined(MI_ENABLE_CUDA)
    #include "optix/sphere.cuh"
#endif

#if defined(MI_ENABLE_METAL) || defined(MI_ENABLE_CUDA)
    #include <mitsuba/render/shapedata.h>
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
            'to_world': mi.ScalarAffineTransform4f().scale([2, 2, 2]).translate([1, 0, 0]),
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
    MI_IMPORT_BASE(Shape, m_to_world, m_discontinuity_types, m_shape_type,
                   initialize, mark_dirty, get_children_string)
    MI_IMPORT_TYPES()

    using typename Base::ScalarIndex;

    /// Bound on the rounding error of a position on the sphere along ``n``
    Float position_error(const Normal3f &n) const {
        Vector3f mag = dr::abs(Vector3f(dr::detach(m_center.value()))) +
                       dr::detach(m_radius.value());
        return dr::dot(dr::abs(dr::detach(n)), mag) * math::PositionEpsilon<Float>;
    }

    /// Direction sampling either samples the entire sphere or the visible
    /// cone when a reference point with squared distance ``dist2`` lies
    /// outside. This method determines which case applies.
    Mask is_outside(const Float &dist2) const {
        return dist2 > dr::square(m_radius_thresh);
    }

    /// ``1 - cos_theta`` without the cancellation of the direct subtraction
    static Float one_minus_cos(const Float &sin_theta_2, const Float &cos_theta) {
        return sin_theta_2 / (1.f + cos_theta);
    }

    /// Spherical UV parameterization of a direction in object space
    static Point2f local_to_uv(const Vector3f &local) {
        // The azimuth is undefined at the poles, pin it and its derivative to zero
        Mask pole = local.x() == 0.f && local.y() == 0.f;
        Float theta = dr::unit_angle_z(local),
              phi   = dr::atan2(dr::select(pole, 0.f, local.y()),
                                dr::select(pole, 1.f, local.x()));
        dr::masked(phi, phi < 0.f) += dr::TwoPi<Float>;
        return { phi * dr::InvTwoPi<Float>, theta * dr::InvPi<Float> };
    }

    Sphere(const Properties &props) : Base(props) {
        // Are the sphere normals pointing inwards? default: no
        m_flip_normals = props.get<bool>("flip_normals", false);

        // Update the to_world transform if radius and center are also provided
        m_to_world =
            m_to_world.scalar() *
            ScalarAffineTransform4f::translate(props.get<ScalarPoint3f>("center", 0.f)) *
            ScalarAffineTransform4f::scale(props.get<ScalarFloat>("radius", 1.f));

        m_discontinuity_types = (uint32_t) DiscontinuityFlags::InteriorType;

        m_shape_type = ShapeType::Sphere;

        update();
        initialize();
    }

    void update() {
        // A sphere must be uniformly scaled (else it is an ellipsoid)
        if (!m_to_world.scalar().is_similarity())
            Log(Warn, "'to_world' transform shouldn't contain non-uniform "
                      "scaling or shearing!");

        m_radius = dr::norm(m_to_world.value() * Vector3f(1.f, 0.f, 0.f));
        m_center = m_to_world.value() * Point3f(0.f);

        m_inv_surface_area = dr::rcp(surface_area());

        // Threshold for `is_outside()` based on the rounding error of the
        // distance computation
        m_radius_thresh =
            dr::fmadd(dr::norm(m_center.value()) + m_radius.value(),
                      math::PositionEpsilon<Float>, m_radius.value());

        dr::make_opaque(m_radius, m_center, m_inv_surface_area,
                        m_radius_thresh);
        mark_dirty();
    }

    void traverse(TraversalCallback *cb) override {
        Base::traverse(cb);
        cb->put("to_world", m_to_world, ParamFlags::Differentiable | ParamFlags::Discontinuous);
    }

    void parameters_changed(const std::vector<std::string> &keys) override {
        if (keys.empty() || string::contains(keys, "to_world")) {
            // LLVM backend: don't overwrite local state until currently running
            // ray tracing kernels finish
            if constexpr (dr::is_llvm_v<Float>)
                dr::sync_thread();

            m_to_world = m_to_world.value().update();
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
    // Sampling routines
    // =============================================================

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        Point3f local = warp::square_to_uniform_sphere(sample);
        Vector3f dir = m_to_world.value() * Vector3f(local);

        PositionSample3f ps = dr::zeros<PositionSample3f>();
        ps.p = m_center.value() + dir;
        ps.n = dr::normalize(dir);
        ps.p_err = position_error(ps.n);

        if (m_flip_normals)
            ps.n = -ps.n;

        ps.time = time;
        ps.delta = m_radius.value() == 0.f;
        ps.pdf = m_inv_surface_area;
        ps.uv = local_to_uv(local);

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

        Mask outside_mask = active && is_outside(dc_2);
        if (likely(dr::any_or<true>(outside_mask))) {
            Float inv_dc            = dr::rsqrt(dc_2),
                  sin_theta_max     = m_radius.value() * inv_dc,
                  sin_theta_max_2   = dr::square(sin_theta_max),
                  inv_sin_theta_max = dr::rcp(sin_theta_max),
                  cos_theta_max     = dr::safe_sqrt(1.f - sin_theta_max_2),
                  one_minus_cos_theta_max = one_minus_cos(sin_theta_max_2, cos_theta_max);

            // Uniformly sample cos_theta in [cos_theta_max, 1]. Working with
            // the complement avoids cancellation when the cone is narrow.
            Float one_minus_cos_theta = sample.x() * one_minus_cos_theta_max,
                  cos_theta           = 1.f - one_minus_cos_theta,
                  sin_theta_2         = one_minus_cos_theta * (2.f - one_minus_cos_theta);

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
            ds.p_err    = position_error(ds.n);
            ds.d        = ds.p - it.p;

            Float dist2 = dr::squared_norm(ds.d);
            ds.dist     = dr::sqrt(dist2);
            ds.d        = ds.d / ds.dist;
            ds.pdf      = dr::InvTwoPi<Float> / one_minus_cos_theta_max;
            dr::masked(ds.pdf, ds.dist == 0.f) = 0.f;

            dr::masked(result, outside_mask) = ds;
        }

        Mask inside_mask = dr::andnot(active, outside_mask);
        if (unlikely(dr::any_or<true>(inside_mask))) {
            Vector3f d = warp::square_to_uniform_sphere(sample);
            DirectionSample3f ds = dr::zeros<DirectionSample3f>();
            ds.p        = dr::fmadd(d, m_radius.value(), m_center.value());
            ds.n        = d;
            ds.p_err    = position_error(ds.n);
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

        Float dc_2 = dr::squared_norm(m_center.value() - it.p);

        // Cone containing the sphere as seen from 'it.p'
        Float sin_theta_max_2 = dr::square(m_radius.value()) / dc_2,
              cos_theta_max   = dr::safe_sqrt(1.f - sin_theta_max_2);

        return dr::select(is_outside(dc_2),
            dr::InvTwoPi<Float> / one_minus_cos(sin_theta_max_2, cos_theta_max),
            m_inv_surface_area * dr::square(ds.dist) / dr::abs_dot(ds.d, ds.n)
        );
    }

    SurfaceInteraction3f eval_parameterization(const Point2f &uv,
                                               uint32_t ray_flags,
                                               Mask active) const override {
        MI_MASK_ARGUMENT(active);

        bool detach_shape = has_flag(ray_flags, RayFlags::DetachShape);
        AffineTransform4f to_world = detach_shape ? dr::detach(m_to_world.value())
                                                  : m_to_world.value();
        Point3f center = detach_shape ? dr::detach(m_center.value())
                                      : m_center.value();
        Float radius = detach_shape ? dr::detach(m_radius.value())
                                    : m_radius.value();

        Float phi = uv.x() * dr::TwoPi<Float>,
              theta = uv.y() * dr::Pi<Float>;
        auto [sin_phi, cos_phi] = dr::sincos(phi);
        auto [sin_theta, cos_theta] = dr::sincos(theta);
        Vector3f local(sin_theta * cos_phi, sin_theta * sin_phi, cos_theta);

        Vector3f n = dr::normalize(to_world * local);

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.t     = dr::select(active, 0.f, dr::Infinity<Float>);
        si.p     = dr::fmadd(n, radius, center);
        si.p_err = position_error(n);
        si.uv    = uv;
        si.dp_du = to_world * Vector3f(-local.y(), local.x(), 0.f) * dr::TwoPi<Float>;
        si.dp_dv = to_world * Vector3f(cos_theta * cos_phi, cos_theta * sin_phi,
                                       -sin_theta) * dr::Pi<Float>;
        si.n = m_flip_normals ? -n : n;
        si.sh_frame.n = si.n;
        si.sh_frame.s = si.dp_du;
        si.initialize_sh_frame();
        si.shape = this;
        dr::masked(si.shape, !active) = nullptr;

        if (has_flag(ray_flags, RayFlags::NormalPartials)) {
            Float inv_radius = (m_flip_normals ? -1.f : 1.f) * dr::rcp(radius);
            si.dn_du = si.dp_du * inv_radius;
            si.dn_dv = si.dp_dv * inv_radius;
        }

        return si;
    }

    // =============================================================

    // =============================================================
    // Silhouette sampling routines and other utilities
    // =============================================================

    SilhouetteSample3f sample_silhouette(const Point3f &sample,
                                         uint32_t flags,
                                         Mask active) const override {
        MI_MASK_ARGUMENT(active);

        if (!has_flag(flags, DiscontinuityFlags::InteriorType))
            return dr::zeros<SilhouetteSample3f>();

        // Sample a point on the shape surface
        SilhouetteSample3f ss(
            sample_position(0.f, dr::tail<2>(sample), active));

        // Sample a tangential direction at the point
        ss.d = warp::interval_to_tangent_direction(ss.n, sample.x());

        // Fill other fields
        ss.discontinuity_type = (uint32_t) DiscontinuityFlags::InteriorType;
        ss.flags = flags;
        ss.pdf *= dr::InvTwoPi<Float>;
        ss.shape = this;
        ss.silhouette_d = dr::cross(ss.n, -ss.d);
        ss.foreshortening = dr::rcp(m_radius.value());

        return ss;
    }

    Point3f invert_silhouette_sample(const SilhouetteSample3f &ss,
                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        Point3f sample = dr::zeros<Point3f>(dr::width(ss));
        sample.x() = warp::tangent_direction_to_interval(ss.n, ss.d);

        Float theta = ss.uv.y() * dr::Pi<Float>;
        Float phi = ss.uv.x() * dr::TwoPi<Float>;
        Vector3f local = sph_to_dir(theta, phi);

        Point2f sample_square = warp::uniform_sphere_to_square(local);
        sample.y() = sample_square.x();
        sample.z() = sample_square.y();

        return sample;
    }

    Point3f differential_motion(const SurfaceInteraction3f &si,
                         Mask active) const override {
        MI_MASK_ARGUMENT(active);

        if constexpr (!dr::is_diff_v<Float>) {
            return si.p;
        } else {
            Float phi = si.uv.x() * dr::TwoPi<Float>;
            Float theta = si.uv.y() * dr::Pi<Float>;
            Point3f local = dr::detach(sph_to_dir(theta, phi));

            Point3f p_diff = m_to_world.value() * local;

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
        Vector3f OV = viewpoint - center;
        Float inv_dist_OV = dr::rsqrt(dr::squared_norm(OV));
        Vector3f OVd = OV * inv_dist_OV,
                 OYd = dr::normalize(si.n - dr::dot(OVd, si.n) * OVd);

        // Angle at the center between the viewpoint and the silhouette point
        Float cos_theta = radius * inv_dist_OV,
              sin_theta = dr::safe_sqrt(dr::fnmadd(cos_theta, cos_theta, 1.f));

        Vector3f OXd = dr::fmadd(OVd, cos_theta, OYd * sin_theta);

        ss.p = dr::fmadd(OXd, radius, center);
        ss.d = dr::normalize(ss.p - viewpoint);
        ss.n = OXd;
        ss.uv = local_to_uv(m_to_world.value().inverse() * ss.p);
        ss.silhouette_d = dr::cross(ss.n, -ss.d);

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
        Vector3f OV = viewpoint - center;
        Float inv_OV_dist = dr::rsqrt(dr::squared_norm(OV));
        Vector3f OV_normalized = OV * inv_OV_dist;
        auto [dx, dy] = coordinate_system(OV_normalized);
        auto [sin_theta, cos_theta] = dr::sincos(sample2 * dr::TwoPi<Float>);

        // Call `primitive_silhouette_projection` which uses `si.n`
        // to compute the silhouette point as `ss.p`.
        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();
        si.n = dr::normalize(OV_normalized + dx * cos_theta + dy * sin_theta);

        uint32_t flags = (uint32_t) DiscontinuityFlags::InteriorType;
        SilhouetteSample3f ss =
            primitive_silhouette_projection(viewpoint, si, flags, 0.f, active);

        Float radius_ring = radius * inv_OV_dist * dr::norm(ss.p - viewpoint);
        ss.pdf = dr::rcp(dr::TwoPi<ScalarFloat> * radius_ring);

        return ss;
    }

    // =============================================================

    // =============================================================
    // Ray tracing routines
    // =============================================================

    template <typename FloatP, typename Ray3fP>
    std::pair<dr::mask_t<FloatP>, FloatP>
    intersect_impl(const Ray3fP &ray, dr::mask_t<FloatP> active) const {
        using Vector3fP    = Vector<FloatP, 3>;
        using ScalarFloatP = dr::scalar_t<FloatP>;

        FloatP radius;
        Vector3fP center;
        if constexpr (!dr::is_jit_v<FloatP>) {
            radius = (ScalarFloatP) m_radius.scalar();
            center = Vector<ScalarFloatP, 3>(m_center.scalar());
        } else {
            radius = (FloatP) m_radius.value();
            center = (Vector3fP) m_center.value();
        }

        // Move the ray origin to the point closest to the sphere center. The
        // quadratic then has a vanishing linear coefficient, which avoids
        // cancellation in the discriminant when the origin is far away.
        Vector3fP l = Vector3fP(ray.o) - center,
                  d = Vector3fP(ray.d);

        FloatP A = dr::squared_norm(d),
               t_offset = -dr::dot(l, d) / A;
        Vector3fP o = dr::fmadd(d, t_offset, l);

        FloatP B = ScalarFloatP(2) * dr::dot(o, d),
               C = dr::squared_norm(o) - dr::square(radius);

        auto [solution_found, near_t, far_t] = math::solve_quadratic(A, B, C);

        // Undo the origin shift
        near_t += t_offset;
        far_t += t_offset;

        dr::mask_t<FloatP> near_ok = near_t >= 0.f && near_t <= ray.maxt,
                           far_ok  = far_t  >= 0.f && far_t  <= ray.maxt;

        active &= solution_found && (near_ok || far_ok);
        FloatP t = dr::select(near_ok, near_t, far_t);

        return { active, dr::select(active, t, dr::Infinity<FloatP>) };
    }

    template <typename FloatP, typename Ray3fP>
    std::tuple<dr::mask_t<FloatP>, FloatP, Point<FloatP, 2>,
               dr::uint32_array_t<FloatP>, dr::uint32_array_t<FloatP>>
    ray_intersect_preliminary_impl(const Ray3fP &ray,
                                   ScalarIndex /*prim_index*/,
                                   dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);
        auto [valid, t] = intersect_impl<FloatP>(ray, active);
        return { valid, t, dr::zeros<Point<FloatP, 2>>(), ((uint32_t) -1), 0 };
    }

    template <typename FloatP, typename Ray3fP>
    dr::mask_t<FloatP> ray_test_impl(const Ray3fP &ray,
                                     ScalarIndex /*prim_index*/,
                                     dr::mask_t<FloatP> active) const {
        MI_MASK_ARGUMENT(active);
        return intersect_impl<FloatP>(ray, active).first;
    }

    MI_SHAPE_DEFINE_RAY_INTERSECT_METHODS()

    SurfaceInteraction3f compute_surface_interaction(const Ray3f &ray,
                                                     const PreliminaryIntersection3f &pi,
                                                     uint32_t ray_flags,
                                                     Mask active) const override {
        MI_MASK_ARGUMENT(active);

        bool shading      = has_flag(ray_flags, RayFlags::Shading);
        bool detach_shape = has_flag(ray_flags, RayFlags::DetachShape);

        const Point3f& center = m_center.value();
        const Float& radius = m_radius.value();
        const AffineTransform4f& to_world = m_to_world.value();
        AffineTransform4f to_object = to_world.inverse();

        // If necessary, temporally suspend gradient tracking for all shape
        // parameters to construct a surface interaction completely detach from
        // the shape.
        dr::suspend_grad<Float> scope(detach_shape, center, radius, to_world, to_object);

        SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

        // Re-project onto the sphere to improve accuracy
        si.t = pi.t;
        si.n = dr::detach(dr::normalize(ray(pi.t) - center));
        si.p = dr::detach(dr::fmadd(si.n, radius, center));
        si.p_err = position_error(si.n);

        // Surface position at the detached parameterization: the local
        // coordinates are static as the sphere moves.
        Point3f p_att = to_world * dr::detach(to_object * si.p);

        si.attach_motion(ray, p_att, ray_flags);

        si.sh_frame.n = dr::normalize(si.p - center);
        Point3f local = to_object * si.p;

        if (likely(shading)) {
            Float rd_2    = dr::square(local.x()) + dr::square(local.y()),
                  rd      = dr::sqrt(rd_2),
                  inv_rd  = dr::rcp(rd),
                  cos_phi = local.x() * inv_rd,
                  sin_phi = local.y() * inv_rd;

            Mask singularity_mask = active && (rd == 0.f);

            si.uv = local_to_uv(local);
            si.dp_du = Vector3f(-local.y(), local.x(), 0.f);
            si.dp_dv = Vector3f(local.z() * cos_phi, local.z() * sin_phi, -rd);

            if (unlikely(dr::any_or<true>(singularity_mask)))
                si.dp_dv[singularity_mask] = Vector3f(1.f, 0.f, 0.f);

            si.dp_du = to_world * si.dp_du * (2.f * dr::Pi<Float>);
            si.dp_dv = to_world * si.dp_dv * dr::Pi<Float>;
            si.sh_frame.s = si.dp_du;

            if (has_flag(ray_flags, RayFlags::NormalPartials)) {
                Float inv_radius =
                    (m_flip_normals ? -1.f : 1.f) * dr::rcp(radius);
                si.dn_du = si.dp_du * inv_radius;
                si.dn_dv = si.dp_dv * inv_radius;
            }
        }

        if (m_flip_normals)
            si.sh_frame.n = -si.sh_frame.n;
        si.n = si.sh_frame.n;

        si.prim_index = pi.prim_index;
        si.shape    = this;
        si.instance_index = 0;

        return si;
    }

    bool parameters_grad_enabled() const override {
        return dr::grad_enabled(m_radius) || dr::grad_enabled(m_center) ||
               dr::grad_enabled(m_to_world.value());
    }

    // =============================================================

#if defined(MI_ENABLE_METAL) || defined(MI_ENABLE_CUDA)
    void gpu_fill_data(void *out) const {
        shapedata::SphereData &d = *(shapedata::SphereData *) out;
        ScalarPoint3f c = m_center.scalar();
        d.center_radius = { (float) c.x(), (float) c.y(), (float) c.z(),
                            (float) m_radius.scalar() };
    }

    void describe(ShapeIR &g) const override {
        Base::template describe_with_data<Sphere, shapedata::SphereData>(g);
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

    MI_DECLARE_CLASS(Sphere)

private:
    /// Center in world-space
    field<Point3f> m_center;
    /// Radius in world-space
    field<Float> m_radius;
    Float m_inv_surface_area;
    Float m_radius_thresh;
    bool m_flip_normals;

    MI_TRAVERSE_CB(Base, m_center, m_radius, m_inv_surface_area,
                   m_radius_thresh)
};

MI_EXPORT_PLUGIN(Sphere)
NAMESPACE_END(mitsuba)
