#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

#if defined(MTS_ENABLE_CUDA)
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

.. code-block:: xml

    <shape type="sphere">
        <transform name="to_world">
            <scale value="2"/>
            <translate x="1" y="0" z="0"/>
        </transform>
        <bsdf type="diffuse"/>
    </shape>

    <shape type="sphere">
        <point name="center" x="1" y="0" z="0"/>
        <float name="radius" value="2"/>
        <bsdf type="diffuse"/>
    </shape>

When a :ref:`sphere <shape-sphere>` shape is turned into an :ref:`area <emitter-area>`
light source, Mitsuba 2 switches to an efficient
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
    MTS_IMPORT_BASE(Shape, m_to_world, m_to_object, set_children,
                    get_children_string, parameters_grad_enabled)
    MTS_IMPORT_TYPES()

    using typename Base::ScalarSize;

    Sphere(const Properties &props) : Base(props) {
        /// Are the sphere normals pointing inwards? default: no
        m_flip_normals = props.bool_("flip_normals", false);

        // Update the to_world transform if radius and center are also provided
        m_to_world = m_to_world * ScalarTransform4f::translate(props.point3f("center", 0.f));
        m_to_world = m_to_world * ScalarTransform4f::scale(props.float_("radius", 1.f));

        update();
        set_children();
    }

    void update() {
        // Extract center and radius from to_world matrix (25 iterations for numerical accuracy)
        auto [S, Q, T] = ek::transform_decompose(m_to_world.matrix, 25);

        if (ek::abs(S[0][1]) > 1e-6f || ek::abs(S[0][2]) > 1e-6f || ek::abs(S[1][0]) > 1e-6f ||
            ek::abs(S[1][2]) > 1e-6f || ek::abs(S[2][0]) > 1e-6f || ek::abs(S[2][1]) > 1e-6f)
            Log(Warn, "'to_world' transform shouldn't contain any shearing!");

        if (!(ek::abs(S[0][0] - S[1][1]) < 1e-6f && ek::abs(S[0][0] - S[2][2]) < 1e-6f))
            Log(Warn, "'to_world' transform shouldn't contain non-uniform scaling!");

        m_center = T;
        m_radius = S[0][0];

        if (m_radius <= 0.f) {
            m_radius = ek::abs(m_radius);
            m_flip_normals = !m_flip_normals;
        }

        // Reconstruct the to_world transform with uniform scaling and no shear
        m_to_world = ek::transform_compose<ScalarMatrix4f>(ScalarMatrix3f(m_radius), Q, T);
        m_to_object = m_to_world.inverse();

        m_inv_surface_area = ek::rcp(surface_area());
    }

    ScalarBoundingBox3f bbox() const override {
        ScalarBoundingBox3f bbox;
        bbox.min = m_center - m_radius;
        bbox.max = m_center + m_radius;
        return bbox;
    }

    ScalarFloat surface_area() const override {
        return 4.f * ek::Pi<ScalarFloat> * m_radius * m_radius;
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    PositionSample3f sample_position(Float time, const Point2f &sample,
                                     Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        Point3f local = warp::square_to_uniform_sphere(sample);

        PositionSample3f ps;
        ps.p = ek::fmadd(local, m_radius, m_center);
        ps.n = local;

        if (m_flip_normals)
            ps.n = -ps.n;

        ps.time = time;
        ps.delta = m_radius == 0.f;
        ps.pdf = m_inv_surface_area;

        return ps;
    }

    Float pdf_position(const PositionSample3f & /*ps*/, Mask active) const override {
        MTS_MASK_ARGUMENT(active);
        return m_inv_surface_area;
    }

    DirectionSample3f sample_direction(const Interaction3f &it, const Point2f &sample,
                                       Mask active) const override {
        MTS_MASK_ARGUMENT(active);
        DirectionSample3f result = ek::zero<DirectionSample3f>();

        Vector3f dc_v = m_center - it.p;
        Float dc_2 = ek::squared_norm(dc_v);

        Float radius_adj = m_radius * (m_flip_normals ? (1.f + math::RayEpsilon<Float>) :
                                                        (1.f - math::RayEpsilon<Float>));
        Mask outside_mask = active && dc_2 > ek::sqr(radius_adj);
        if (likely(ek::any_or<true>(outside_mask))) {
            Float inv_dc            = ek::rsqrt(dc_2),
                  sin_theta_max     = m_radius * inv_dc,
                  sin_theta_max_2   = ek::sqr(sin_theta_max),
                  inv_sin_theta_max = ek::rcp(sin_theta_max),
                  cos_theta_max     = ek::safe_sqrt(1.f - sin_theta_max_2);

            /* Fall back to a Taylor series expansion for small angles, where
               the standard approach suffers from severe cancellation errors */
            Float sin_theta_2 = ek::select(sin_theta_max_2 > 0.00068523f, /* sin^2(1.5 deg) */
                                       1.f - ek::sqr(ek::fmadd(cos_theta_max - 1.f, sample.x(), 1.f)),
                                       sin_theta_max_2 * sample.x()),
                  cos_theta = ek::safe_sqrt(1.f - sin_theta_2);

            // Based on https://www.akalin.com/sampling-visible-sphere
            Float cos_alpha = sin_theta_2 * inv_sin_theta_max +
                              cos_theta * ek::safe_sqrt(ek::fnmadd(sin_theta_2, ek::sqr(inv_sin_theta_max), 1.f)),
                  sin_alpha = ek::safe_sqrt(ek::fnmadd(cos_alpha, cos_alpha, 1.f));

            auto [sin_phi, cos_phi] = ek::sincos(sample.y() * (2.f * ek::Pi<Float>));

            Vector3f d = Frame3f(dc_v * -inv_dc).to_world(Vector3f(
                cos_phi * sin_alpha,
                sin_phi * sin_alpha,
                cos_alpha));

            DirectionSample3f ds = ek::zero<DirectionSample3f>();
            ds.p        = ek::fmadd(d, m_radius, m_center);
            ds.n        = d;
            ds.d        = ds.p - it.p;

            Float dist2 = ek::squared_norm(ds.d);
            ds.dist     = ek::sqrt(dist2);
            ds.d        = ds.d / ds.dist;
            ds.pdf      = warp::square_to_uniform_cone_pdf(ek::zero<Vector3f>(), cos_theta_max);
            ek::masked(ds.pdf, ds.dist == 0.f) = 0.f;

            ek::masked(result, outside_mask) = ds;
        }

        Mask inside_mask = ek::andnot(active, outside_mask);
        if (unlikely(ek::any_or<true>(inside_mask))) {
            Vector3f d = warp::square_to_uniform_sphere(sample);
            DirectionSample3f ds = ek::zero<DirectionSample3f>();
            ds.p        = ek::fmadd(d, m_radius, m_center);
            ds.n        = d;
            ds.d        = ds.p - it.p;

            Float dist2 = ek::squared_norm(ds.d);
            ds.dist     = ek::sqrt(dist2);
            ds.d        = ds.d / ds.dist;
            ds.pdf      = m_inv_surface_area * dist2 / ek::abs_dot(ds.d, ds.n);

            ek::masked(result, inside_mask) = ds;
        }

        result.time = it.time;
        result.delta = m_radius == 0.f;

        if (m_flip_normals)
            result.n = -result.n;

        return result;
    }

    Float pdf_direction(const Interaction3f &it, const DirectionSample3f &ds,
                        Mask active) const override {
        MTS_MASK_ARGUMENT(active);

        // Sine of the angle of the cone containing the sphere as seen from 'it.p'.
        Float sin_alpha = m_radius * ek::rcp(ek::norm(m_center - it.p)),
              cos_alpha = ek::safe_sqrt(1.f - sin_alpha * sin_alpha);

        return ek::select(sin_alpha < ek::OneMinusEpsilon<Float>,
            // Reference point lies outside the sphere
            warp::square_to_uniform_cone_pdf(ek::zero<Vector3f>(), cos_alpha),
            m_inv_surface_area * ek::sqr(ds.dist) / ek::abs_dot(ds.d, ds.n)
        );
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    template <typename FloatP, typename Ray3fP>
    std::tuple<FloatP, Point<FloatP, 2>, ek::uint32_array_t<FloatP>,
               ek::uint32_array_t<FloatP>>
    ray_intersect_preliminary_impl(const Ray3fP &ray,
                                   ek::mask_t<FloatP> active) const {
        MTS_MASK_ARGUMENT(active);

        using Double = std::conditional_t<ek::is_cuda_array_v<FloatP> ||
                                              ek::is_diff_array_v<Float>,
                                          FloatP, ek::float64_array_t<FloatP>>;
        using ScalarDouble3 = Vector<ek::scalar_t<Double>, 3>;
        using Double3 = Vector<Double, 3>;

        Double mint = Double(ray.mint);
        Double maxt = Double(ray.maxt);

        Double3 o = Double3(ray.o) - ScalarDouble3(m_center);
        Double3 d(ray.d);

        Double A = ek::squared_norm(d);
        Double B = ek::scalar_t<Double>(2.f) * ek::dot(o, d);
        Double C = ek::squared_norm(o) - ek::sqr((ek::scalar_t<Double>) m_radius);

        auto [solution_found, near_t, far_t] = math::solve_quadratic(A, B, C);

        // Sphere doesn't intersect with the segment on the ray
        ek::mask_t<FloatP> out_bounds = !(near_t <= maxt && far_t >= mint); // NaN-aware conditionals

        // Sphere fully contains the segment of the ray
        ek::mask_t<FloatP> in_bounds = near_t < mint && far_t > maxt;

        active &= solution_found && !out_bounds && !in_bounds;

        FloatP t = ek::select(
            active, ek::select(near_t < mint, FloatP(far_t), FloatP(near_t)),
            ek::Infinity<FloatP>);

        return { t, ek::zero<Point<FloatP, 2>>(), ((uint32_t) -1), 0 };
    }

    template <typename FloatP, typename Ray3fP>
    ek::mask_t<FloatP> ray_test_impl(const Ray3fP &ray,
                                     ek::mask_t<FloatP> active) const {
        MTS_MASK_ARGUMENT(active);

        using Double = std::conditional_t<ek::is_cuda_array_v<FloatP> ||
                                              ek::is_diff_array_v<Float>,
                                          FloatP, ek::float64_array_t<FloatP>>;
        using ScalarDouble3 = Vector<ek::scalar_t<Double>, 3>;
        using Double3 = Vector<Double, 3>;

        Double mint = Double(ray.mint);
        Double maxt = Double(ray.maxt);

        Double3 o = Double3(ray.o) - ScalarDouble3(m_center);
        Double3 d(ray.d);

        Double A = ek::squared_norm(d);
        Double B = ek::scalar_t<Double>(2.f) * ek::dot(o, d);
        Double C = ek::squared_norm(o) - ek::sqr((ek::scalar_t<Double>) m_radius);

        auto [solution_found, near_t, far_t] = math::solve_quadratic(A, B, C);

        // Sphere doesn't intersect with the segment on the ray
        ek::mask_t<FloatP> out_bounds = !(near_t <= maxt && far_t >= mint); // NaN-aware conditionals

        // Sphere fully contains the segment of the ray
        ek::mask_t<FloatP> in_bounds  = near_t < mint && far_t > maxt;

        return solution_found && !out_bounds && !in_bounds && active;
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

        // Fields requirement dependencies
        bool need_dn_duv = has_flag(hit_flags, HitComputeFlags::dNSdUV) ||
                           has_flag(hit_flags, HitComputeFlags::dNGdUV);
        bool need_dp_duv = has_flag(hit_flags, HitComputeFlags::dPdUV) || need_dn_duv;
        bool need_uv     = has_flag(hit_flags, HitComputeFlags::UV) || need_dp_duv;

        // TODO handle sticky derivatives
        SurfaceInteraction3f si = ek::zero<SurfaceInteraction3f>();
        si.t = ek::select(active, pi.t, ek::Infinity<Float>);

        si.sh_frame.n = ek::normalize(ray(pi.t) - m_center);

        // Re-project onto the sphere to improve accuracy
        si.p = ek::fmadd(si.sh_frame.n, m_radius, m_center);

        if (likely(need_uv)) {
            Vector3f local = m_to_object.transform_affine(si.p);

            Float rd_2  = ek::sqr(local.x()) + ek::sqr(local.y()),
                  theta = unit_angle_z(local),
                  phi   = ek::atan2(local.y(), local.x());

            ek::masked(phi, phi < 0.f) += 2.f * ek::Pi<Float>;

            si.uv = Point2f(phi * ek::InvTwoPi<Float>, theta * ek::InvPi<Float>);
            if (likely(need_dp_duv)) {
                si.dp_du = Vector3f(-local.y(), local.x(), 0.f);

                Float rd      = ek::sqrt(rd_2),
                      inv_rd  = ek::rcp(rd),
                      cos_phi = local.x() * inv_rd,
                      sin_phi = local.y() * inv_rd;

                si.dp_dv = Vector3f(local.z() * cos_phi,
                                    local.z() * sin_phi,
                                    -rd);

                Mask singularity_mask = active && ek::eq(rd, 0.f);
                if (unlikely(ek::any_or<true>(singularity_mask)))
                    si.dp_dv[singularity_mask] = Vector3f(1.f, 0.f, 0.f);

                si.dp_du = m_to_world * si.dp_du * (2.f * ek::Pi<Float>);
                si.dp_dv = m_to_world * si.dp_dv * ek::Pi<Float>;
            }
        }

        if (m_flip_normals)
            si.sh_frame.n = -si.sh_frame.n;

        si.n = si.sh_frame.n;

        if (need_dn_duv) {
            ScalarFloat inv_radius = (m_flip_normals ? -1.f : 1.f) / m_radius;
            si.dn_du = si.dp_du * inv_radius;
            si.dn_dv = si.dp_dv * inv_radius;
        }

        si.shape    = this;
        si.instance = nullptr;

        return si;
    }

    //! @}
    // =============================================================

    void traverse(TraversalCallback *callback) override {
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
                m_optix_data_ptr = jit_malloc(AllocType::Device, sizeof(OptixSphereData));

            OptixSphereData data = { bbox(), (Vector<float, 3>) m_center,
                                     (float) m_radius };

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

    MTS_DECLARE_CLASS()
private:
    /// Center in world-space
    ScalarPoint3f m_center;
    /// Radius in world-space
    ScalarFloat m_radius;

    ScalarFloat m_inv_surface_area;

    bool m_flip_normals;
};

MTS_IMPLEMENT_CLASS_VARIANT(Sphere, Shape)
MTS_EXPORT_PLUGIN(Sphere, "Sphere intersection primitive");
NAMESPACE_END(mitsuba)
