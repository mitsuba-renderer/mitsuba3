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
 * Cylinder shape.
 */
class Cylinder final : public Shape {
public:
    Cylinder(const Properties &props) : Shape(props) {
        m_radius = props.float_("radius", 1.f);

        Point3f p0 = props.point3f("p0", Point3f(0.f, 0.f, 0.f)),
                p1 = props.point3f("p1", Point3f(0.f, 0.f, 1.f));

        Vector3f d = p1 - p0;
        m_length = norm(d);

        m_object_to_world = Transform4f::translate(p0) *
                            Transform4f::to_frame(Frame(d / m_length)) *
                            Transform4f::scale(Vector3f(m_radius, m_radius, m_length));

        // Are the cylinder normals pointing inwards? default: no
        m_flip_normals = props.bool_("flip_normals", false);

        if (props.has_property("to_world")) {
            m_object_to_world = props.transform("to_world") * m_object_to_world;
            m_radius = norm(m_object_to_world * Vector3f(1.f, 0.f, 0.f));
            m_length = norm(m_object_to_world * Vector3f(0.f, 0.f, 1.f));
        }

        // Remove the scale from the object-to-world transform
        m_object_to_world = m_object_to_world * Transform4f::scale(
            rcp(Vector3f(m_radius, m_radius, m_length)));

        m_world_to_object = m_object_to_world.inverse();
        m_inv_surface_area = 1.f / surface_area();

        if (m_radius <= 0.f) {
            m_radius = std::abs(m_radius);
            m_flip_normals = !m_flip_normals;
        }
        if (is_emitter())
            emitter()->set_shape(this);
    }

    BoundingBox3f bbox() const override {
        Vector3f x1 = m_object_to_world * Vector3f(m_radius, 0.f, 0.f),
                 x2 = m_object_to_world * Vector3f(0.f, m_radius, 0.f),
                 x  = sqrt(sqr(x1) + sqr(x2));

        Point3f p0 = m_object_to_world * Point3f(0.f, 0.f, 0.f),
                p1 = m_object_to_world * Point3f(0.f, 0.f, m_length);

        /* To bound the cylinder, it is sufficient to find the
           smallest box containing the two circles at the endpoints.
           This can be done component-wise as follows */
        return BoundingBox3f(min(p0 - x, p1 - x), max(p0 + x, p1 + x));
    }

    Float surface_area() const override {
        return 2.f * math::Pi * m_radius * m_length;
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    template <typename Point2, typename Value = value_t<Point2>>
    MTS_INLINE auto sample_position_impl(Value time, const Point2 &sample,
                                         mask_t<Value> /* active */) const {
        using Point3  = point3_t<Point2>;
        using Normal3 = normal3_t<Point2>;

        auto [sin_theta, cos_theta] = sincos(2.f * math::Pi * sample.y());

        Point3 p(cos_theta * m_radius,
                 sin_theta * m_radius,
                 sample.x() * m_length);

        Normal3 n(cos_theta, sin_theta, 0.f);

        if (m_flip_normals)
            n *= -1;

        PositionSample<Point3> ps;
        ps.p     = m_object_to_world.transform_affine(p);
        ps.n     = normalize(n);
        ps.pdf   = m_inv_surface_area;
        ps.time  = time;
        ps.delta = false;
        return ps;
    }

    template <typename PositionSample3,
              typename Value = typename PositionSample3::Value>
    MTS_INLINE Value pdf_position_impl(PositionSample3 & /* ps */,
                                       mask_t<Value> /* active */) const {
        return m_inv_surface_area;
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    template <typename Ray3, typename Value = typename Ray3::Value>
    MTS_INLINE std::pair<mask_t<Value>, Value> ray_intersect_impl(
            const Ray3 &ray_, Value* /* cache */, mask_t<Value> active) const {
        using Float64 = float64_array_t<Value>;
        using Mask    = mask_t<Value>;

        Ray3 ray = m_world_to_object * ray_;

        Float64 mint = Float64(ray.mint),
                maxt = Float64(ray.maxt);

        Float64 ox = Float64(ray.o.x()),
                oy = Float64(ray.o.y()),
                oz = Float64(ray.o.z()),
                dx = Float64(ray.d.x()),
                dy = Float64(ray.d.y()),
                dz = Float64(ray.d.z());

        double  radius = double(m_radius),
                length = double(m_length);

        Float64 A = sqr(dx) + sqr(dy),
                B = 2.0 * (dx * ox + dy * oy),
                C = sqr(ox) + sqr(oy) - sqr(radius);

        auto [solution_found, near_t, far_t] =
            math::solve_quadratic(A, B, C);

        // Cylinder doesn't intersect with the segment on the ray
        Mask out_bounds = !(near_t <= maxt && far_t >= mint); // NaN-aware conditionals

        Float64 z_pos_near = oz + dz*near_t,
                z_pos_far  = oz + dz*far_t;

        // Cylinder fully contains the segment of the ray
        Mask in_bounds = near_t < mint && far_t > maxt;

        Mask valid_intersection =
            active && solution_found && !out_bounds && !in_bounds &&
            ((z_pos_near >= 0 && z_pos_near <= length && near_t >= mint) ||
             (z_pos_far  >= 0 && z_pos_far <= length  && far_t <= maxt));

        return { valid_intersection,
                 select(z_pos_near >= 0 && z_pos_near <= length && near_t >= mint, near_t, far_t) };
    }

    template <typename Ray3, typename Value = typename Ray3::Value>
    MTS_INLINE mask_t<Value> ray_test_impl(const Ray3 &ray_, mask_t<Value> active) const {
        using Float64  = float64_array_t<Value>;
        using Mask    = mask_t<Value>;

        Ray3 ray = m_world_to_object * ray_;

        Float64 mint = Float64(ray.mint);
        Float64 maxt = Float64(ray.maxt);

        Float64 ox = Float64(ray.o.x()),
                oy = Float64(ray.o.y()),
                oz = Float64(ray.o.z()),
                dx = Float64(ray.d.x()),
                dy = Float64(ray.d.y()),
                dz = Float64(ray.d.z());

        double  radius = double(m_radius),
                length = double(m_length);

        Float64 A = dx*dx + dy*dy,
                B = 2.0*(dx*ox + dy*oy),
                C = ox*ox + oy*oy - radius*radius;

        auto [solution_found, near_t, far_t] = math::solve_quadratic(A, B, C);

        // Cylinder doesn't intersect with the segment on the ray
        Mask out_bounds = !(near_t <= maxt && far_t >= mint); // NaN-aware conditionals

        Float64 z_pos_near = oz + dz*near_t,
                z_pos_far  = oz + dz*far_t;

        // Cylinder fully contains the segment of the ray
        Mask in_bounds = near_t < mint && far_t > maxt;

        Mask valid_intersection =
            active && solution_found && !out_bounds && !in_bounds &&
            ((z_pos_near >= 0 && z_pos_near <= length && near_t >= mint) ||
             (z_pos_far >= 0 && z_pos_far <= length && far_t <= maxt));

        return valid_intersection;
    }

    template <typename Ray3,
              typename SurfaceInteraction3,
              typename Value = typename SurfaceInteraction3::Value>
    MTS_INLINE void fill_surface_interaction_impl(const Ray3 &ray, const Value* /* cache */,
                                                  SurfaceInteraction3 &si_out,
                                                  mask_t<Value> active) const {
        using Vector3 = typename SurfaceInteraction3::Vector3;
        using Normal3 = typename SurfaceInteraction3::Normal3;
        using Point2  = typename SurfaceInteraction3::Point2;

        SurfaceInteraction3 si(si_out);

        si.p = ray(si.t);
        Vector3 local = m_world_to_object * si.p;

        Value phi = atan2(local.y(), local.x());
        masked(phi, phi < 0.f) += 2.f * math::Pi;

        si.uv = Point2(phi * math::InvTwoPi,
                       local.z() / m_length);

        Vector3 dp_du = 2.f * math::Pi * Vector3(-local.y(), local.x(), 0.f);
        Vector3 dp_dv = Vector3(0.f, 0.f, m_length);
        si.dp_du = m_object_to_world * dp_du;
        si.dp_dv = m_object_to_world * dp_dv;
        si.n = Normal3(cross(normalize(si.dp_du), normalize(si.dp_dv)));

        /* Migitate roundoff error issues by a normal shift of the computed
           intersection point */
        si.p += si.n * (m_radius - norm(head<2>(local)));

        if (m_flip_normals)
            si.n *= -1.f;

        si.sh_frame.n = si.n;
        si.time = ray.time;

        si_out[active] = si;
    }

    template <typename SurfaceInteraction3,
              typename Value   = typename SurfaceInteraction3::Value,
              typename Vector3 = typename SurfaceInteraction3::Vector3>
    MTS_INLINE std::pair<Vector3, Vector3> normal_derivative_impl(
            const SurfaceInteraction3 &si, bool, mask_t<Value>) const {
        Vector3 dn_du = si.dp_du / (m_radius * (m_flip_normals ? -1.f : 1.f)),
                dn_dv = Vector3(0.f);

        return { dn_du, dn_dv };
    }

    //! @}
    // =============================================================

    Size primitive_count() const override { return 1; }

    Size effective_primitive_count() const override { return 1; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Cylinder[" << std::endl
            << "  p0 = "  << m_object_to_world * Point3f(0.f, 0.f, 0.f) << "," << std::endl
            << "  p1 = "  << m_object_to_world * Point3f(0.f, 0.f, m_length) << "," << std::endl
            << "  radius = "  << m_radius << "," << std::endl
            << "  length = "  << m_length << "," << std::endl
            << "  bsdf = " << string::indent(bsdf()->to_string()) << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_SHAPE()
    MTS_DECLARE_CLASS()
private:
    Transform4f m_object_to_world;
    Transform4f m_world_to_object;
    Float   m_radius, m_length;
    Float   m_inv_surface_area;
    bool    m_flip_normals;
};

MTS_IMPLEMENT_CLASS(Cylinder, Shape)
MTS_EXPORT_PLUGIN(Cylinder, "Cylinder intersection primitive");

NAMESPACE_END(mitsuba)
