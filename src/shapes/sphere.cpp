#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/sensor.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Sphere shape.
 */
class Sphere : public Shape {
public:
    Sphere(const Properties &props) : Shape(props) {
        m_object_to_world =
            Transform4f::translate(Vector3f(props.point3f("center", Point3f(0.0f))));
        m_radius = props.float_("radius", 1.0f);

        if (props.has_property("to_world")) {
            Transform4f object_to_world = props.transform("to_world");
            Float radius = norm(object_to_world * Vector3f(1, 0, 0));
            // Remove the scale from the object-to-world transform
            m_object_to_world =
                object_to_world
                * Transform4f::scale(Vector3f(1.0f / radius))
                * m_object_to_world;
            m_radius *= radius;
        }

        /// Are the sphere normals pointing inwards? default: no
        m_flip_normals = props.bool_("flip_normals", false);
        m_center = m_object_to_world * Point3f(0, 0, 0);
        m_world_to_object = m_object_to_world.inverse();
        m_inv_surface_area = 1.0f / (4.0f * math::Pi * m_radius * m_radius);

        if (m_radius <= 0.0f)
            Throw("Cannot create spheres of radius <= 0");

        if (is_emitter())
            emitter()->set_shape(this);
    }

    BoundingBox3f bbox() const override {
        BoundingBox3f bbox;
        bbox.min = m_center - Vector3f(m_radius);
        bbox.max = m_center + Vector3f(m_radius);
        return bbox;
    }

    Float surface_area() const override {
        return 4.0f * math::Pi * m_radius * m_radius;
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================
    template <typename Point2, typename Value = value_t<Point2>>
    auto sample_position_impl(Value time, const Point2 &sample,
                              const mask_t<Value> &/*active*/) const {
        using Point3   = point3_t<Point2>;
        using Normal3  = normal3_t<Point2>;

        auto v = warp::square_to_uniform_sphere(sample);

        PositionSample<Point3> ps;
        ps.p = Point3(v * m_radius) + m_center;
        ps.n = Normal3(v);
        if (m_flip_normals)
            ps.n *= -1;
        // TODO: fill UVs?
        ps.time = time;
        ps.delta = m_radius != 0.f;
        ps.pdf = m_inv_surface_area;

        return ps;
    }

    template <typename PositionSample3,
              typename Value = typename PositionSample3::Value>
    Value pdf_position_impl(PositionSample3 &/*p_rec*/,
                            const mask_t<Value> &/*active*/) const {
        return m_inv_surface_area;
    }

    /**
     * Improved sampling strategy given in
     * "Monte Carlo techniques for direct lighting calculations" by
     * Shirley, P. and Wang, C. and Zimmerman, K. (TOG 1996)
     */
    template <typename Interaction3, typename Point2,
              typename Value = value_t<Point2>>
    auto sample_direction_impl(
            const Interaction3 &it, const Point2 &sample,
            const mask_t<Value> &active) const {
        using Vector3 = vector3_t<Point2>;
        using Normal3 = normal3_t<Point2>;
        using Point3  = point3_t<Point2>;
        using Frame3  = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        // TODO: should initialize with sample_position and / or set all fields?
        DirectionSample<Point3> ds;

        const Vector3 ref_to_center = m_center - it.p;
        const Value ref_dist_2 = squared_norm(ref_to_center);
        const Value inv_ref_dist = rcp(sqrt(ref_dist_2));

        // Sine of the angle of the cone containing the
        // sphere as seen from 'it.p'
        const Value sin_alpha = m_radius * inv_ref_dist;

        Mask ref_outside     = active && (sin_alpha < (1.0f - math::Epsilon));
        Mask not_ref_outside = active && !ref_outside;

        // TODO: simplify this to avoid masked assignments
        if (any(ref_outside)) {
            // The reference point lies outside of the sphere.
            // => sample based on the projected cone.
            Value cos_alpha = enoki::safe_sqrt(1.0f - sin_alpha * sin_alpha);

            masked(ds.d, ref_outside) = Frame3(
                ref_to_center * inv_ref_dist).to_world(
                    warp::square_to_uniform_cone(sample, cos_alpha)
            );
            masked(ds.pdf, ref_outside) = warp::square_to_uniform_cone_pdf(
                Vector3(0, 0, 0), cos_alpha
            );

            // Distance to the projection of the sphere center
            // onto the ray (it.p, ds.d)
            const Value proj_dist = dot(ref_to_center, ds.d);

            // To avoid numerical problems move the query point to the
            // intersection of the of the original direction ray and a plane
            // with normal ref_to_center which goes through the sphere's center
            const Value base_t = ref_dist_2 / proj_dist;
            const Point3 query = it.p + ds.d * base_t;

            const Vector3 query_to_center = m_center - query;
            const Value   query_dist_2    = squared_norm(query_to_center);
            const Value   query_proj_dist = dot(query_to_center, ds.d);

            // Try to find the intersection point between the sampled ray and the sphere.
            Value A = 1.0f,
                  B = -2.0f * query_proj_dist,
                  C = query_dist_2 - m_radius * m_radius;

            Mask solution_found;
            Value near_t, far_t;
            std::tie(solution_found, near_t, far_t) = math::solve_quadratic(A, B, C);

            // The intersection couldn't be found due to roundoff errors..
            // Don't give up -- one workaround is to project the closest
            // ray position onto the sphere
            masked(near_t, !solution_found) = query_proj_dist;

            masked(ds.dist, ref_outside) = base_t + near_t;
            masked(ds.n,    ref_outside) = normalize(
                ds.d * near_t - query_to_center);
            masked(ds.p,    ref_outside) = m_center + ds.n * m_radius;
        }

        if (any(!ref_outside)) {
            // The reference point lies inside the sphere
            // => use uniform sphere sampling.
            Vector3 d = warp::square_to_uniform_sphere(sample);

            masked(ds.p, not_ref_outside) = m_center + d * m_radius;
            masked(ds.n, not_ref_outside) = Normal3(d);
            masked(ds.d, not_ref_outside) = ds.p - it.p;

            Value dist2 = squared_norm(ds.d);
            masked(ds.dist, not_ref_outside) = sqrt(dist2);
            masked(ds.d,    not_ref_outside) = ds.d / ds.dist;
            masked(ds.pdf,  not_ref_outside) = m_inv_surface_area * dist2
                                                  / abs_dot(ds.d, ds.n);
        }

        if (m_flip_normals)
            ds.n *= -1.0f;
        ds.delta = m_radius != 0.f;
        return ds;
    }

    template <typename Interaction3, typename DirectionSample3,
              typename Value = typename Interaction3::Value>
    Value pdf_direction_impl(const Interaction3 &it, const DirectionSample3 &ds,
                             const mask_t<Value> &/*active*/) const {
        using Vector3 = typename DirectionSample3::Vector3;
        // Sine of the angle of the cone containing the sphere as seen from 'it.p'.
        Value sin_alpha = m_radius * rcp(norm(m_center - it.p)),
              cos_alpha = enoki::safe_sqrt(1 - sin_alpha * sin_alpha);

        return select(sin_alpha < (1 - math::Epsilon),
            // Reference point lies outside the sphere
            warp::square_to_uniform_cone_pdf(Vector3(0, 0, 0), cos_alpha),
            m_inv_surface_area * ds.dist * ds.dist / abs_dot(ds.d, ds.n)
        );
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    template <typename Ray3, typename Value = typename Ray3::Value>
    std::pair<mask_t<Value>, Value> ray_intersect_impl(
            const Ray3 &ray, Value * /*cache*/, const mask_t<Value> &active) const {
        using Double  = like_t<Value, double>;
        using Vector3 = Vector<Double, 3>;
        using Mask    = mask_t<Value>;

        Double mint = Double(ray.mint);
        Double maxt = Double(ray.maxt);

        Vector3 o = Vector3(ray.o) - Vector<double, 3>(m_center);
        Vector3 d(ray.d);

        Double A = squared_norm(d);
        Double B = 2.0 * dot(o, d);
        Double C = squared_norm(o) - (double) (m_radius * m_radius);

        Mask solution_found;
        Double near_t, far_t;
        std::tie(solution_found, near_t, far_t) = math::solve_quadratic(A, B, C);

        // Sphere doesn't intersect with the segment on the ray
        Mask out_bounds = !((near_t <= maxt) && (far_t >= mint)); // NaN-aware conditionals
        // Sphere fully contains the segment of the ray
        Mask in_bounds = (near_t < mint) && (far_t > maxt);

        Mask valid_intersection = solution_found && (!out_bounds) && (!in_bounds);
        Value t = select(near_t < mint, far_t, near_t);

        return { valid_intersection && active, t };
    }

    template <typename Ray3, typename Value = typename Ray3::Value>
    mask_t<Value> ray_test_impl(const Ray3 &ray, const mask_t<Value> &active) const {
        using Double  = like_t<Value, double>;
        using Vector3 = Vector<Double, 3>;
        using Mask    = mask_t<Value>;

        Double mint = Double(ray.mint);
        Double maxt = Double(ray.maxt);

        Vector3 o = Vector3(ray.o) - Vector<double, 3>(m_center);
        Vector3 d(ray.d);

        Double A = squared_norm(d);
        Double B = 2.0 * dot(o, d);
        Double C = squared_norm(o) - (double) (m_radius * m_radius);

        Mask solution_found;
        Double near_t, far_t;
        std::tie(solution_found, near_t, far_t) = math::solve_quadratic(A, B, C);

        // Sphere doesn't intersect with the segment on the ray
        Mask out_bounds = !((near_t <= maxt) && (far_t >= mint)); // NaN-aware conditionals
        // Sphere fully contains the segment of the ray
        Mask in_bounds  = ((near_t < mint) && (far_t > maxt));

        return solution_found && (!out_bounds) && (!in_bounds) && active;
    }

    template <typename Ray3,
              typename SurfaceInteraction3,
              typename Value = typename SurfaceInteraction3::Value>
    void fill_intersection_record_impl(const Ray3 &ray, const Value * /*cache*/,
                                       SurfaceInteraction3 &si,
                                       const mask_t<Value> &active) const {
        using Vector3 = typename SurfaceInteraction3::Vector3;
        using Mask    = mask_t<Value>;

        masked(si.p, active) = ray(si.t);

        // Re-project onto the sphere to limit cancellation effects
        masked(si.p, active) = m_center + normalize(si.p - m_center) * m_radius;

        Vector3 local = m_world_to_object * (si.p - m_center);
        Value   theta = safe_acos(local.z() / m_radius);
        Value   phi   = atan2(local.y(), local.x());

        masked(phi, phi < 0.0f) += 2.0f * math::Pi;

        masked(si.uv.x(), active) = phi * math::InvTwoPi;
        masked(si.uv.y(), active) = theta * math::InvPi;
        masked(si.dp_du,  active) = m_object_to_world * Vector3(
            -local.y(), local.x(), 0.0f) * (2.0f * math::Pi);
        masked(si.sh_frame.n, active) = normalize(si.p - m_center);
        Value z_rad = sqrt(local.x() * local.x() + local.y() * local.y());
        masked(si.shape, active) = this;

        Mask z_rad_gt_0  = (z_rad > 0.0f);
        Mask z_rad_leq_0 = !z_rad_gt_0;

        if (any(z_rad_gt_0 && active)) {
            Mask mask = z_rad_gt_0 && active;
            Value inv_z_rad = rcp(z_rad),
                  cos_phi   = local.x() * inv_z_rad,
                  sin_phi   = local.y() * inv_z_rad;
            masked(si.dp_dv, mask) = m_object_to_world
                                            * Vector3(local.z() * cos_phi,
                                                      local.z() * sin_phi,
                                                      -sin(theta) * m_radius) * math::Pi;
            masked(si.sh_frame.s, mask) = normalize(si.dp_du);
            masked(si.sh_frame.t, mask) = normalize(si.dp_dv);
        }

        if (any(z_rad_leq_0 && active)) {
            Mask mask = z_rad_leq_0 && active;
            // avoid a singularity
            const Value cos_phi = 0.0f, sin_phi = 1.0f;
            masked(si.dp_dv, mask) = m_object_to_world
                                             * Vector3(local.z() * cos_phi,
                                                       local.z() * sin_phi,
                                                       -sin(theta) * m_radius) * math::Pi;
            Vector3 a, b;
            std::tie(a, b) = coordinate_system(si.sh_frame.n);
            masked(si.sh_frame.s, mask) = a;
            masked(si.sh_frame.t, mask) = b;
        }

        if (m_flip_normals)
            masked(si.sh_frame.n, active) = -si.sh_frame.n;

        masked(si.n,        active) = si.sh_frame.n;
        masked(si.instance, active) = nullptr;
        masked(si.time,     active) = ray.time;
        si.has_uv_partials = false;
    }

    template <typename SurfaceInteraction3,
              typename Value   = typename SurfaceInteraction3::Value,
              typename Vector3 = typename SurfaceInteraction3::Vector3>
    std::pair<Vector3, Vector3> normal_derivative_impl(
            const SurfaceInteraction3 &si, bool, const mask_t<Value> &) const {
        Value inv_radius = (m_flip_normals ? -1.0f : 1.0f) / m_radius;
        Vector3 dn_du = si.dp_du * inv_radius;
        Vector3 dn_dv = si.dp_dv * inv_radius;
        return { dn_du , dn_dv };
    }

    //! @}
    // =============================================================

    Size primitive_count() const override { return 1; }

    Size effective_primitive_count() const override { return 1; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Sphere[" << std::endl
            << "  radius = "  << m_radius << "," << std::endl
            << "  center = "  << m_center << "," << std::endl
            << "  bsdf = "    << string::indent(bsdf()->to_string()) << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_SHAPE()
    MTS_DECLARE_CLASS()
private:
    Transform4f m_object_to_world;
    Transform4f m_world_to_object;
    Point3f m_center;
    Float   m_radius;
    Float   m_inv_surface_area;
    bool    m_flip_normals;
};

MTS_IMPLEMENT_CLASS(Sphere, Shape)
MTS_EXPORT_PLUGIN(Sphere, "Sphere intersection primitive");

NAMESPACE_END(mitsuba)
