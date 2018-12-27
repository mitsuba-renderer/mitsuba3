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
class Sphere final : public Shape {
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
    MTS_INLINE auto sample_position_impl(Value time, const Point2 &sample,
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
    MTS_INLINE Value pdf_position_impl(PositionSample3 & /* ps */,
                                       mask_t<Value> /* active */) const {
        return m_inv_surface_area;
    }

    template <typename Interaction3, typename Point2,
              typename Value = value_t<Point2>>
    MTS_INLINE auto sample_direction_impl(
            const Interaction3 &it, const Point2 &sample,
            mask_t<Value> active) const {
        using Vector3 = vector3_t<Point2>;
        using Point3  = point3_t<Point2>;
        using Frame3  = Frame<Vector3>;
        using Mask    = mask_t<Value>;
        using DirectionSample3 = mitsuba::DirectionSample<Point3>;

        DirectionSample3 result;

        Vector3 dc_v = m_center - it.p;
        Value dc_2 = squared_norm(dc_v);

        Float radius_adj = m_radius * (m_flip_normals ? (1.f + math::Epsilon) :
                                                        (1.f - math::Epsilon));

        Mask outside_mask = active && dc_2 > sqr(radius_adj);
        if (likely(any(outside_mask))) {
            Value inv_dc            = rsqrt(dc_2),
                  sin_theta_max     = m_radius * inv_dc,
                  sin_theta_max_2   = sqr(sin_theta_max),
                  inv_sin_theta_max = rcp(sin_theta_max),
                  cos_theta_max     = safe_sqrt(1.f - sin_theta_max_2);

            /* Fall back to a Taylor series expansion for small angles, where
               the standard approach suffers from severe cancellation errors */
            Value sin_theta_2 = select(sin_theta_max_2 > 0.00068523f, /* sin^2(1.5 deg) */
                                       1.f - sqr(fmadd(cos_theta_max - 1.f, sample.x(), 1.f)),
                                       sin_theta_max_2 * sample.x()),
                  cos_theta = safe_sqrt(1.f - sin_theta_2);

            /* Based on https://www.akalin.com/sampling-visible-sphere */
            Value cos_alpha = sin_theta_2 * inv_sin_theta_max +
                              cos_theta * safe_sqrt(fnmadd(sin_theta_2, sqr(inv_sin_theta_max), 1.f)),
                  sin_alpha = safe_sqrt(fnmadd(cos_alpha, cos_alpha, 1.f));

            auto [sin_phi, cos_phi] = sincos(sample.y() * (2.f * math::Pi));

            Vector3 d = Frame3(dc_v * -inv_dc).to_world(Vector3(
                cos_phi * sin_alpha,
                sin_phi * sin_alpha,
                cos_alpha));

            DirectionSample3 ds = zero<DirectionSample3>();
            ds.p        = fmadd(d, m_radius, m_center);
            ds.n        = d;
            ds.d        = ds.p - it.p;

            Value dist2 = squared_norm(ds.d);
            ds.dist     = sqrt(dist2);
            ds.d        = ds.d / ds.dist;
            ds.pdf      = warp::square_to_uniform_cone_pdf(zero<Vector3>(), cos_theta_max);

            result[outside_mask] = ds;
        }

        Mask inside_mask = andnot(active, outside_mask);
        if (unlikely(any(inside_mask))) {
            Vector3 d = warp::square_to_uniform_sphere(sample);
            DirectionSample3 ds = zero<DirectionSample3>();
            ds.p        = fmadd(d, m_radius, m_center);
            ds.n        = d;
            ds.d        = ds.p - it.p;

            Value dist2 = squared_norm(ds.d);
            ds.dist     = sqrt(dist2);
            ds.d        = ds.d / ds.dist;
            ds.pdf      = m_inv_surface_area * dist2 / abs_dot(ds.d, ds.n);

            result[inside_mask] = ds;
        }

        result.time = it.time;
        result.delta = m_radius != 0;

        if (m_flip_normals)
            result.n = -result.n;

        return result;
    }

    template <typename Interaction3, typename DirectionSample3,
              typename Value = typename Interaction3::Value>
    MTS_INLINE Value pdf_direction_impl(const Interaction3 &it, const DirectionSample3 &ds,
                                        mask_t<Value> /* active */) const {
        using Vector3 = typename DirectionSample3::Vector3;
        // Sine of the angle of the cone containing the sphere as seen from 'it.p'.
        Value sin_alpha = m_radius * rcp(norm(m_center - it.p)),
              cos_alpha = enoki::safe_sqrt(1.f - sin_alpha * sin_alpha);

        return select(sin_alpha < (1.f - math::Epsilon),
            // Reference point lies outside the sphere
            warp::square_to_uniform_cone_pdf(zero<Vector3>(), cos_alpha),
            m_inv_surface_area * sqr(ds.dist) / abs_dot(ds.d, ds.n)
        );
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    template <typename Ray3, typename Value = typename Ray3::Value>
    MTS_INLINE std::pair<mask_t<Value>, Value> ray_intersect_impl(
            const Ray3 &ray, Value* /* cache */, mask_t<Value> active) const {
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
    MTS_INLINE mask_t<Value> ray_test_impl(const Ray3 &ray, mask_t<Value> active) const {
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
    MTS_INLINE void fill_surface_interaction_impl(const Ray3 &ray, const Value* /* cache */,
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
                theta   = unit_angle_z(d),
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
    MTS_INLINE std::pair<Vector3, Vector3> normal_derivative_impl(
            const SurfaceInteraction3 &si, bool, mask_t<Value>) const {
        Float inv_radius = (m_flip_normals ? -1.f : 1.f) / m_radius;
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
    MTS_IMPLEMENT_SHAPE_SAMPLE_DIRECTION()
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
