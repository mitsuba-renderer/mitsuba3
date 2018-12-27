#include <mitsuba/core/fwd.h>
#include <mitsuba/core/math.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/string.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/core/util.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/emitter.h>
#include <mitsuba/render/fwd.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Flat disk shape.
 *
 * By default, the disk has unit radius and is located at the origin. Its
 * surface normal points into the positive $Z$ direction. To change the disk
 * scale, rotation, or translation, use the 'to_world' parameter.
 */
class Disk final : public Shape {
public:
    Disk(const Properties &props) : Shape(props) {
        m_object_to_world = props.transform("to_world", Transform4f());
        if (props.bool_("flip_normals", false))
            m_object_to_world = m_object_to_world * Transform4f::scale(Vector3f(1.f, 1.f, -1.f));

        m_world_to_object = m_object_to_world.inverse();

        m_dp_du = m_object_to_world * Vector3f(1.f, 0.f, 0.f);
        m_dp_dv = m_object_to_world * Vector3f(0.f, 1.f, 0.f);
        Normal3f normal = normalize(m_object_to_world * Normal3f(0.f, 0.f, 1.f));
        m_frame = Frame3f(normalize(m_dp_du), normalize(m_dp_dv), normal);

        m_inv_surface_area = 1.f / surface_area();
        if (abs_dot(m_frame.s, m_frame.t) > math::Epsilon ||
            abs_dot(m_frame.s, m_frame.n) > math::Epsilon)
            Throw("The `to_world` transformation contains shear, which is not"
                  " supported by the Disk shape.");

        if (is_emitter())
            emitter()->set_shape(this);
    }

    BoundingBox3f bbox() const override {
        BoundingBox3f bbox;
        bbox.expand(m_object_to_world.transform_affine(Point3f( 1.f,  0.f, 0.f)));
        bbox.expand(m_object_to_world.transform_affine(Point3f(-1.f,  0.f, 0.f)));
        bbox.expand(m_object_to_world.transform_affine(Point3f( 0.f,  1.f, 0.f)));
        bbox.expand(m_object_to_world.transform_affine(Point3f( 0.f, -1.f, 0.f)));
        return bbox;
    }

    Float surface_area() const override {
        return math::Pi * norm(m_dp_du) * norm(m_dp_dv);
    }

    // =============================================================
    //! @{ \name Sampling routines
    // =============================================================

    template <typename Point2, typename Value>
    MTS_INLINE auto sample_position_impl(Value time, const Point2 &sample,
                                         mask_t<Value> /* active */) const {
        using Point3 = point3_t<Point2>;

        Point2 p = warp::square_to_uniform_disk_concentric(sample);

        PositionSample<Point3> ps;
        ps.p    = m_object_to_world.transform_affine(Point3(p.x(), p.y(), 0.f));
        ps.n    = m_frame.n;
        ps.pdf  = m_inv_surface_area;
        ps.time = time;
        ps.delta = false;

        return ps;
    }

    template <typename PositionSample3,
              typename Value = typename PositionSample3::Value>
    MTS_INLINE Value pdf_position_impl(const PositionSample3 & /* ps */,
                                       mask_t<Value> /* active */) const {
        return m_inv_surface_area;
    }

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    template <typename Ray3, typename Value>
    MTS_INLINE std::pair<mask_t<Value>, Value> ray_intersect_impl(
        const Ray3 &ray_, Value *cache, mask_t<Value> active) const {
        using Point3 = Point<Value, 3>;

        Ray3 ray     = m_world_to_object.transform_affine(ray_);
        Value t      = -ray.o.z() / ray.d.z();
        Point3 local = ray(t);

        // Is intersection within ray segment and disk?
        active = active && t >= ray.mint
                        && t <= ray.maxt
                        && local.x()*local.x() + local.y()*local.y() <= 1;

        if (cache) {
            masked(cache[0], active) = local.x();
            masked(cache[1], active) = local.y();
        }

        return { active, t };
    }

    template <typename Ray3, typename Value = typename Ray3::Value>
    MTS_INLINE mask_t<Value> ray_test_impl(const Ray3 &ray_, mask_t<Value> active) const {
        using Point3 = Point<Value, 3>;

        Ray3 ray     = m_world_to_object * ray_;
        Value t      = -ray.o.z() / ray.d.z();
        Point3 local = ray(t);

        // Is intersection within ray segment and rectangle?
        return active && t >= ray.mint
                      && t <= ray.maxt
                      && local.x()*local.x() + local.y()*local.y() <= 1;
    }

    template <typename Ray3,
              typename SurfaceInteraction3,
              typename Value = typename SurfaceInteraction3::Value>
    MTS_INLINE void fill_surface_interaction_impl(const Ray3 &ray, const Value *cache,
                                                  SurfaceInteraction3 &si_out,
                                                  mask_t<Value> active) const {
        using Point2   = typename SurfaceInteraction3::Point2;
        using Vector3  = typename SurfaceInteraction3::Vector3;

        SurfaceInteraction3 si(si_out);

        Value r = norm(Point2(cache[0], cache[1])),
              inv_r = rcp(r);

        Value v = atan2(cache[1], cache[0]) * math::InvTwoPi;
        masked(v, v < 0.f) += 1.f;

        Value cos_phi = select(neq(r, 0.f), cache[0] * inv_r, 1.f),
              sin_phi = select(neq(r, 0.f), cache[1] * inv_r, 0.f);

        si.dp_du      = m_object_to_world * Vector3( cos_phi, sin_phi, 0.f);
        si.dp_dv      = m_object_to_world * Vector3(-sin_phi, cos_phi, 0.f);

        si.n          = m_frame.n;
        si.sh_frame.n = m_frame.n;
        si.uv         = Point2(r, v);
        si.p          = ray(si.t);
        si.time       = ray.time;

        si_out[active] = si;
    }

    template <typename SurfaceInteraction3,
              typename Value   = typename SurfaceInteraction3::Value,
              typename Vector3 = typename SurfaceInteraction3::Vector3>
    MTS_INLINE std::pair<Vector3, Vector3> normal_derivative_impl(
            const SurfaceInteraction3 &/*si*/, bool, mask_t<Value>) const {
        return { Vector3(0.f), Vector3(0.f) };
    }

    Size primitive_count() const override { return 1; }

    Size effective_primitive_count() const override { return 1; }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Disk[" << std::endl
            << "  center = "  << m_object_to_world * Point3f(0.f, 0.f, 0.f) << "," << std::endl
            << "  n = "  << m_object_to_world * Normal3f(0.f, 0.f, 1.f) << "," << std::endl
            << "  du = "  << norm(m_object_to_world * Normal3f(1.f, 0.f, 0.f)) << "," << std::endl
            << "  dv = "  << norm(m_object_to_world * Normal3f(0.f, 1.f, 0.f)) << "," << std::endl
            << "  bsdf = " << string::indent(bsdf()->to_string()) << std::endl
            << "]";
        return oss.str();
    }

    MTS_IMPLEMENT_SHAPE()
    MTS_DECLARE_CLASS()
private:
    Transform4f m_object_to_world;
    Transform4f m_world_to_object;
    Frame<Vector3f> m_frame;
    Vector3f m_dp_du, m_dp_dv;
    Float m_inv_surface_area;
};

MTS_IMPLEMENT_CLASS(Disk, Shape)
MTS_EXPORT_PLUGIN(Disk, "Disk intersection primitive");

NAMESPACE_END(mitsuba)
