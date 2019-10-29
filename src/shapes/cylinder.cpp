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
template <typename Float, typename Spectrum>
class Cylinder final : public Shape<Float, Spectrum> {
public:
    MTS_DECLARE_PLUGIN(Cylinder, Shape)
    MTS_USING_BASE(Shape, bsdf, emitter, is_emitter);
    using Index = typename Base::Index;
    using Size  = typename Base::Size;

    Cylinder(const Properties &props) : Base(props) {
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
           smallest box containing the two circles at the endpoints. */
        return BoundingBox3f(min(p0 - x, p1 - x), max(p0 + x, p1 + x));
    }

    BoundingBox3f bbox(Index /*index*/, const BoundingBox3f &clip) const override {
        using FloatP8         = Packet<Float, 8>;
        using MaskP8          = mask_t<FloatP8>;
        using Point3fP8       = Point<FloatP8, 3>;
        using Vector3fP8      = Vector<FloatP8, 3>;
        using BoundingBox3fP8 = BoundingBox<Point3fP8>;

        Point3f cyl_p  = m_object_to_world.transform_affine(Point3f(0.f, 0.f, 0.f));
        Vector3f cyl_d = m_object_to_world.transform_affine(Vector3f(0.f, 0.f, m_length));

        // Compute a base bounding box
        BoundingBox3f bbox(this->bbox());
        bbox.clip(clip);

        /* Now forget about the cylinder ends and intersect an infinite
           cylinder with each bounding box face, then compute a bounding
           box of the resulting ellipses. */
        Point3fP8 face_p = zero<Point3fP8>();
        Vector3fP8 face_n = zero<Vector3fP8>();

        for (size_t i = 0; i < 3; ++i) {
            face_p.coeff(i,  i * 2 + 0) = bbox.min.coeff(i);
            face_p.coeff(i,  i * 2 + 1) = bbox.max.coeff(i);
            face_n.coeff(i,  i * 2 + 0) = -1.f;
            face_n.coeff(i,  i * 2 + 1) = 1.f;
        }

        // Project the cylinder direction onto the plane
        FloatP8 dp   = dot(cyl_d, face_n);
        MaskP8 valid = neq(dp, 0.f);

        // Compute semimajor/minor axes of ellipse
        Vector3fP8 v1 = fnmadd(face_n, dp, cyl_d);
        FloatP8 v1_n2 = squared_norm(v1);
        v1 = select(neq(v1_n2, 0.f), v1 * rsqrt(v1_n2),
                    coordinate_system(face_n).first);
        Vector3fP8 v2 = cross(face_n, v1);

        // Compute length of axes
        v1 *= m_radius / abs(dp);
        v2 *= m_radius;

        // Compute center of ellipse
        FloatP8 t = dot(face_n, face_p - cyl_p) / dp;
        Point3fP8 center = fmadd(Vector3fP8(cyl_d), t, Vector3fP8(cyl_p));
        center[neq(face_n, 0.f)] = face_p;

        // Compute ellipse minima and maxima
        Vector3fP8 x = sqrt(sqr(v1) + sqr(v2));
        BoundingBox3fP8 ellipse_bounds(center - x, center + x);
        MaskP8 ellipse_overlap = valid && bbox.overlaps(ellipse_bounds);
        ellipse_bounds.clip(bbox);

        return BoundingBox3f(
            hmin_inner(select(ellipse_overlap, ellipse_bounds.min,
                              Point3fP8(std::numeric_limits<Float>::infinity()))),
            hmax_inner(select(ellipse_overlap, ellipse_bounds.max,
                              Point3fP8(-std::numeric_limits<Float>::infinity()))));
    }

    Float surface_area() const override {
        return 2.f * math::Pi<Float> * m_radius * m_length;
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    MTS_INLINE PositionSample3f sample_position(Float time, const Point2f &sample,
                                                Mask /*active*/) const override {
        auto [sin_theta, cos_theta] = sincos(2.f * math::Pi<Float> * sample.y());

        Point3f p(cos_theta * m_radius,
                 sin_theta * m_radius,
                 sample.x() * m_length);

        Normal3f n(cos_theta, sin_theta, 0.f);

        if (m_flip_normals)
            n *= -1;

        PositionSample3f ps;
        ps.p     = m_object_to_world.transform_affine(p);
        ps.n     = normalize(n);
        ps.pdf   = m_inv_surface_area;
        ps.time  = time;
        ps.delta = false;
        return ps;
    }

    MTS_INLINE Float pdf_position(const PositionSample3f & /*ps*/, Mask /*active*/) const override {
        return m_inv_surface_area;
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    MTS_INLINE std::pair<Mask, Float> ray_intersect(const Ray3f &ray_, Float * /*cache*/,
                                                    Mask active) const override {
        using Float64 = float64_array_t<Float>;

        Ray3f ray = m_world_to_object * ray_;
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

    MTS_INLINE Mask ray_test(const Ray3f &ray_, Mask active) const override {
        using Float64  = float64_array_t<Float>;

        Ray3f ray = m_world_to_object * ray_;
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

    MTS_INLINE void fill_surface_interaction(const Ray3f &ray, const Float * /*cache*/,
                                             SurfaceInteraction3f &si_out,
                                             Mask active) const override {
        SurfaceInteraction3f si(si_out);

        si.p = ray(si.t);
        Vector3f local = m_world_to_object * si.p;

        Float phi = atan2(local.y(), local.x());
        masked(phi, phi < 0.f) += 2.f * math::Pi<Float>;

        si.uv = Point2f(phi * math::InvTwoPi<Float>,
                       local.z() / m_length);

        Vector3f dp_du = 2.f * math::Pi<Float> * Vector3f(-local.y(), local.x(), 0.f);
        Vector3f dp_dv = Vector3f(0.f, 0.f, m_length);
        si.dp_du = m_object_to_world * dp_du;
        si.dp_dv = m_object_to_world * dp_dv;
        si.n = Normal3f(cross(normalize(si.dp_du), normalize(si.dp_dv)));

        /* Migitate roundoff error issues by a normal shift of the computed
           intersection point */
        si.p += si.n * (m_radius - norm(head<2>(local)));

        if (m_flip_normals)
            si.n *= -1.f;

        si.sh_frame.n = si.n;
        si.time = ray.time;

        si_out[active] = si;
    }

    MTS_INLINE std::pair<Vector3f, Vector3f> normal_derivative(const SurfaceInteraction3f &si,
                                                               bool /*shading_frame*/,
                                                               Mask /*active*/) const override {
        Vector3f dn_du = si.dp_du / (m_radius * (m_flip_normals ? -1.f : 1.f)),
                dn_dv = Vector3f(0.f);

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

private:
    Transform4f m_object_to_world;
    Transform4f m_world_to_object;
    ScalarFloat m_radius, m_length;
    ScalarFloat m_inv_surface_area;
    bool m_flip_normals;
};

MTS_IMPLEMENT_PLUGIN(Cylinder, Shape, "Cylinder intersection primitive");
NAMESPACE_END(mitsuba)
