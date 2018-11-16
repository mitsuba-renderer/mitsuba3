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
            Transform4f::translate(Vector3f(props.point3f("center", Point3f(0.f))));
        m_radius = props.float_("radius", 1.f);

        if (props.has_property("to_world")) {
            Transform4f object_to_world = props.transform("to_world");
            Float radius = norm(object_to_world * Vector3f(1, 0, 0));
            // Remove the scale from the object-to-world transform
            m_object_to_world =
                object_to_world
                * Transform4f::scale(Vector3f(1.f / radius))
                * m_object_to_world;
            m_radius *= radius;
        }

        /// Are the sphere normals pointing inwards? default: no
        m_flip_normals = props.bool_("flip_normals", false);
        m_center = m_object_to_world * Point3f(0, 0, 0);
        m_world_to_object = m_object_to_world.inverse();
        m_inv_surface_area = 1.f / (4.f * math::Pi * m_radius * m_radius);

        if (m_radius <= 0.f) {
            m_radius = std::abs(m_radius);
            m_flip_normals = !m_flip_normals;
        }

        if (is_emitter())
            emitter()->set_shape(this);
    }

    BoundingBox3f bbox() const override {
        BoundingBox3f bbox;
        bbox.min = m_center - m_radius;
        bbox.max = m_center + m_radius;
        return bbox;
    }

    Float surface_area() const override {
        return 4.f * math::Pi * m_radius * m_radius;
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    template <typename Point2, typename Value = value_t<Point2>>
    auto sample_position_impl(Value time, const Point2 &sample,
                              mask_t<Value> /* active */) const {
        using Point3 = point3_t<Point2>;

        Point3 p = warp::square_to_uniform_sphere(sample);

        PositionSample<Point3> ps;
        ps.p = fmadd(p, m_radius, m_center);
        ps.n = p;
        if (m_flip_normals)
            ps.n = -ps.n;
        ps.time = time;
        ps.delta = m_radius != 0.f;
        ps.pdf = m_inv_surface_area;

        return ps;
    }

    template <typename PositionSample3,
              typename Value = typename PositionSample3::Value>
    Value pdf_position_impl(PositionSample3 &/* ps */,
                            mask_t<Value> /* active */) const {
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
            mask_t<Value> active) const {
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

        Mask ref_outside     = active && (sin_alpha < (1.f - math::Epsilon));
        Mask not_ref_outside = active && !ref_outside;

        // TODO: simplify this to avoid masked assignments
        if (any(ref_outside)) {
            // The reference point lies outside of the sphere.
            // => sample based on the projected cone.
            Value cos_alpha = enoki::safe_sqrt(1.f - sin_alpha * sin_alpha);

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
            Value A = 1.f,
                  B = -2.f * query_proj_dist,
                  C = query_dist_2 - m_radius * m_radius;

            auto [solution_found, near_t, far_t] = math::solve_quadratic(A, B, C);

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
            ds.n *= -1.f;
        ds.delta = (m_radius != 0.f);
        return ds;
    }

    template <typename Interaction3, typename DirectionSample3,
              typename Value = typename Interaction3::Value>
    Value pdf_direction_impl(const Interaction3 &it, const DirectionSample3 &ds,
                             const mask_t<Value> &/* active */) const {
        using Vector3 = typename DirectionSample3::Vector3;
        // Sine of the angle of the cone containing the sphere as seen from 'it.p'.
        Value sin_alpha = m_radius * rcp(norm(m_center - it.p)),
              cos_alpha = enoki::safe_sqrt(1.f - sin_alpha * sin_alpha);

        return select(sin_alpha < (1.f - math::Epsilon),
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
        using Float64  = float64_array_t<Value>;
        using Vector3 = Vector<Float64, 3>;
        using Mask    = mask_t<Value>;

        Float64 mint = Float64(ray.mint);
        Float64 maxt = Float64(ray.maxt);

        Vector3 o = Vector3(ray.o) - Vector3d(m_center);
        Vector3 d(ray.d);

        Float64 A = squared_norm(d);
        Float64 B = 2.0 * dot(o, d);
        Float64 C = squared_norm(o) - sqr((double) m_radius);

        auto [solution_found, near_t, far_t] = math::solve_quadratic(A, B, C);

        // Sphere doesn't intersect with the segment on the ray
        Mask out_bounds = !(near_t <= maxt && far_t >= mint); // NaN-aware conditionals

        // Sphere fully contains the segment of the ray
        Mask in_bounds = near_t < mint && far_t > maxt;

        Mask valid_intersection =
            active && solution_found && !out_bounds && !in_bounds;

        return { valid_intersection, select(near_t < mint, far_t, near_t) };
    }

    template <typename Ray3, typename Value = typename Ray3::Value>
    mask_t<Value> ray_test_impl(const Ray3 &ray, const mask_t<Value> &active) const {
        using Float64 = float64_array_t<Value>;
        using Vector3 = Vector<Float64, 3>;
        using Mask    = mask_t<Value>;

        Float64 mint = Float64(ray.mint);
        Float64 maxt = Float64(ray.maxt);

        Vector3 o = Vector3(ray.o) - Vector3d(m_center);
        Vector3 d(ray.d);

        Float64 A = squared_norm(d);
        Float64 B = 2.0 * dot(o, d);
        Float64 C = squared_norm(o) - sqr((double) m_radius);

        auto [solution_found, near_t, far_t] = math::solve_quadratic(A, B, C);

        // Sphere doesn't intersect with the segment on the ray
        Mask out_bounds = !(near_t <= maxt && far_t >= mint); // NaN-aware conditionals

        // Sphere fully contains the segment of the ray
        Mask in_bounds  = near_t < mint && far_t > maxt;

        return solution_found && !out_bounds && !in_bounds && active;
    }

    template <typename Ray3,
              typename SurfaceInteraction3,
              typename Value = typename SurfaceInteraction3::Value>
    void fill_surface_interaction_impl(const Ray3 &ray, const Value* /* cache */,
                                       SurfaceInteraction3 &si_out,
                                       mask_t<Value> active) const {
        using Vector3 = typename SurfaceInteraction3::Vector3;
        using Point2  = typename SurfaceInteraction3::Point2;
        using Mask    = mask_t<Value>;

        SurfaceInteraction3 si(si_out);
        si.sh_frame.n = normalize(ray(si.t) - m_center);

        // Re-project onto the sphere to improve accuracy
        si.p = fmadd(si.sh_frame.n, m_radius, m_center);

        Vector3 local   = m_world_to_object * (si.p - m_center),
                d       = local / m_radius;

        Value   rd_2    = sqr(d.x()) + sqr(d.y()),
                theta   = 2.f * safe_asin(.5f * sqrt(rd_2 + sqr(d.z() - 1.f))),
                phi     = atan2(d.y(), d.x());

        masked(phi, phi < 0.f) += 2.f * math::Pi;

        si.uv = Point2(phi * math::InvTwoPi, theta * math::InvPi);
        si.dp_du = Vector3(-local.y(), local.x(), 0.f);

        Value rd      = sqrt(rd_2),
              inv_rd  = rcp(rd),
              cos_phi = d.x() * inv_rd,
              sin_phi = d.y() * inv_rd;

        si.dp_dv = Vector3(local.z() * cos_phi,
                           local.z() * sin_phi,
                           -rd * m_radius);

        Mask singularity_mask = active && eq(rd, 0.f);
        if (unlikely(any(singularity_mask)))
            si.dp_dv[singularity_mask] = Vector3(m_radius, 0.f, 0.f);

        si.dp_du = m_object_to_world * si.dp_du * (2.f * math::Pi);
        si.dp_dv = m_object_to_world * si.dp_dv * math::Pi;

        if (m_flip_normals)
            si.sh_frame.n = -si.sh_frame.n;

        si.n = si.sh_frame.n;
        si.time = ray.time;

        si_out[active] = si;
    }

    template <typename SurfaceInteraction3,
              typename Value   = typename SurfaceInteraction3::Value,
              typename Vector3 = typename SurfaceInteraction3::Vector3>
    std::pair<Vector3, Vector3> normal_derivative_impl(
            const SurfaceInteraction3 &si, bool, mask_t<Value>) const {
        Value inv_radius = (m_flip_normals ? -1.f : 1.f) / m_radius;
        return { si.dp_du * inv_radius, si.dp_dv * inv_radius };
    }

    //! @}
    // =============================================================

    Size primitive_count() const override { return 1; }

    Size effective_primitive_count() const override { return 1; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Sphere[" << std::endl
            << "  radius = "  << m_radius << "," << std::endl
            << "  center = "  << m_center << std::endl
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
